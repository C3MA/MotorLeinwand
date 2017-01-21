-- Include the Wifi Configuration and the current settings
dofile("wlancfg.lua")
dofile("settings.lua")
local gpioRelayUp = 1   -- GPIO4
local gpioRelayDown = 2 -- GPIO5
local gpioBtnUp   = 6 -- GPIO12
local gpioBtnDown = 7 -- GPIO13
local gpioBtnStop = 5 -- GPIO14
gpio.mode(gpioRelayUp, gpio.OUTPUT)
gpio.mode(gpioRelayDown, gpio.OUTPUT)
-- Prepare inputs with PULL-UP:
gpio.mode(gpioBtnUp, gpio.INPUT, gpio.PULLUP)
gpio.mode(gpioBtnDown, gpio.INPUT, gpio.PULLUP)
gpio.mode(gpioBtnStop, gpio.INPUT, gpio.PULLUP)

-- Alles aus machen
gpio.write(gpioRelayUp, gpio.LOW)        
gpio.write(gpioRelayDown, gpio.LOW)

-- Possible states are: 
local STATE_UP = 1
local STATE_DOWN = 2
local STATE_STOP = 3
local screenCommandState = 0

local mqttPrefix="/room/screen/"
local color=0
local setupComplete=false
local connected2mqtt=false

-- Screen control logic
function publishEndState()
    if (publishMovingDir == -1) then
        m:publish(mqttPrefix .. "state","up",0,0)
    elseif (publishMovingDir==1) then
        m:publish(mqttPrefix .. "state","down",0,0)
    end
end

function startTimeoutsupervision()
    tmr.alarm(6, setting_screen100perc_time+setting_timerdelay, tmr.ALARM_SINGLE, function()
        publishEndState()
        -- stop both relais
        commandScreenStop()
        print("Timer stopped relais")
        tmr.stop(5) -- Stop blinking
        ws2812.write(4, string.char(0,0,0))
    end)
end

-- Publish actual state
local publishMovingStart=0
local publishMovingDir=0 -- 1 for down; -1 for up
local currentPercent=0
local targetPercent=nil

function getPercent()
    if (publishMovingStart ~= 0) then
        diff=(tmr.now() - publishMovingStart) / 1000 -- convert to milliseconds
        percentDiff=(100 * diff) / setting_screen100perc_time
        return currentPercent + (publishMovingDir * percentDiff)
    else
        return 0
    end
end

function updatePercent()
    tmr.stop(1)
    currentPercent = getPercent()
    publishMovingStart=0
end

function publish(direction)
    if (direction == nil) then
        return
    end
    publishMovingStart=tmr.now()
    tmr.alarm(1, 500, tmr.ALARM_AUTO, function()
        percent = getPercent()

        if ((percent < 0) or (percent > 100)) then
            print("Stop screen by percentage monitoring (" .. percent .. "% )")
            publishEndState()
            if (publishMovingDir == -1) then
                currentPercent=0
            elseif (publishMovingDir==1) then
                currentPercent=100
            end
            publishMovingDir=0
            targetPercent=nil
            tmr.stop(1)
            tmr.stop(5)
            ws2812.write(4, string.char(0,0,0))
        else
            m:publish(mqttPrefix .. "percent", percent,0,0)
            print("Now at " .. percent .. "%")
            -- Check if the screen reached the requested percentage
            if ((targetPercent ~= nil) and (percent == targetPercent)) then
                print("Reached target")
                commandScreenStop()
            end
        end
    end)
    if (direction == "up") then
        publishMovingDir=-1
    elseif (direction == "down") then
        publishMovingDir=1
    end
end

function commandScreenUp(force)
    if (screenCommandState ~= STATE_UP) then
        screenCommandState = STATE_UP
        -- Update global percentage variable
       if (publishMovingDir ~= 0) then
            updatePercent()
            publishMovingStart=0
            publishMovingDir=0
       end
        
        if (force == nil) then
            publishMovingStart=0
            publish("up")
        end
        -- BlinkCode
        tmr.alarm(5, 250, tmr.ALARM_AUTO, function()
         if (color==0) then
            ws2812.write(4, string.char(128,0,0)) -- GREEN
            color=1
         else
            ws2812.write(4, string.char(0,0,0))
            color=0
         end
        end)
        print("Screen up")
        m:publish(mqttPrefix .. "state","movingup",0,0)
        gpio.write(gpioRelayDown, gpio.LOW)   
        tmr.delay(50000) -- wait 50 ms
        gpio.write(gpioRelayUp, gpio.HIGH)
        startTimeoutsupervision()
   end
end

function commandScreenDown(force)
    if (screenCommandState ~= STATE_DOWN) then
       screenCommandState = STATE_DOWN
       -- Update global percentage variable
       if (publishMovingDir ~= 0) then
            updatePercent()
            publishMovingStart=0
            publishMovingDir=0
       end
       
       if (force == nil) then
            publishMovingStart=0
            publish("down")
        end
       -- BlinkCode
       tmr.alarm(5, 250, tmr.ALARM_AUTO, function()
         if (color==0) then
            ws2812.write(4, string.char(0,128,0)) -- RED
            color=1
         else
            ws2812.write(4, string.char(0,0,0))
            color=0
         end
       end)
       print("Screen down")
       m:publish(mqttPrefix .. "state","movingdown",0,0)
       gpio.write(gpioRelayUp, gpio.LOW)   
       tmr.delay(50000) -- wait 50 ms
       gpio.write(gpioRelayDown, gpio.HIGH)
       startTimeoutsupervision()
   end
end

function commandScreenStop(dontPublishSomething, force)
    if (screenCommandState ~= STATE_STOP) then
        screenCommandState = STATE_STOP
        if (force == nil and publishMovingDir ~= 0) then
            updatePercent()
            publishMovingStart=0
            publishMovingDir=0
            targetPercent=nil
        end
        print("Screen stop")
        if (dontPublishSomething ~= nil) then
            m:publish(mqttPrefix .. "state","stop",0,0)
        end
        gpio.write(gpioRelayUp, gpio.LOW)
        gpio.write(gpioRelayDown, gpio.LOW)
        tmr.stop(1)
        tmr.stop(5) -- Stop blinking
        ws2812.write(4, string.char(0,0,0))
    end
end

function commandScreenPercent(percent)
    if (percent == nil or percent < 0 or percent > 100) then
        return
    end
    if (percent == currentPercent) then
        print("Already at " .. percent .. "% no moving needed")
    else
        print("Screen moving to " .. percent .. "%")
        targetPercent=percent
        if (currentPercent > percent) then
            commandScreenUp()
        elseif (currentPercent < percent) then
            commandScreenDown()
        end
    end
end

local mqttIPserver="10.23.42.10"

-- The Mqtt logic
m = mqtt.Client("crash_" .. node.chipid(), 120, "user", "pass")

function mqttsubscribe()
 tmr.alarm(1,50,0,function() 
        m:subscribe(mqttPrefix .. "command",0, function(conn) 
            print("subscribed") 
            m:publish(mqttPrefix .. "ip",wifi.sta.getip(),0,0)
        end)
	connected2mqtt=true
    end)
end
m:on("connect", mqttsubscribe)
m:on("offline", function(con) 
    print ("offline")
    node.restart()
end)
m:on("message", function(conn, topic, data)
   -- skip emtpy messages
   if (data == nil) then
    return
   end
   if topic== mqttPrefix .. "command" then
      if (data == "up") then
        commandScreenUp() 
      elseif ( data == "down") then
        commandScreenDown()
        m:publish(mqttPrefix .. "state","down",0,1)
      elseif ( data == "stop" ) then
        commandScreenStop(true)
        m:publish(mqttPrefix .. "state","stop",0,1)
      elseif (tonumber(data) ~= nil) then
        commandScreenPercent(tonumber(data))
      end
   end
end)

-- Wait to be connect to the WiFi access point. 
tmr.alarm(0, 100, 1, function()
  if (setupComplete) then
    -- Logic handling buttons
    if (gpio.read(gpioBtnStop) == gpio.LOW) then
        commandScreenStop(true)
    elseif (gpio.read(gpioBtnUp) == gpio.LOW) then
        commandScreenUp()
    elseif (gpio.read(gpioBtnDown) == gpio.LOW) then
        commandScreenDown()
    end
  else
     if (setting_ignoreWifi ~= nil) then
        setupComplete=true
     end
      -- The startup script
      if wifi.sta.status() ~= 5 then
         print("Connecting to AP...")
         if (color==0) then
            ws2812.write(4, string.char(0,0,128)) -- BLUE
            color=1
         else
            ws2812.write(4, string.char(0,0,0))
            color=0
         end
      else
         setupComplete=true
         ws2812.write(4, string.char(0,0,0))
         m:connect(mqttIPserver,1883,0)
         if (file.open("telnet.lua")) then
            dofile("telnet.lua")
            startTcpServer()
         end
      end
      -- Restart if there is no Server after 5 minutes
      if (((tmr.now() / 1000000) > 300) and (connected2mqtt==false)) then
           node.restart()
      end
  end
end)
