dofile("wlancfg.lua")
dofile("settings.lua")
print("Initialize Hardware")
gpioRelayUp = 1   -- GPIO4
gpioRelayDown = 2 -- GPIO5

gpioBtnUp   = 6 -- GPIO12
gpioBtnDown = 7 -- GPIO13
gpioBtnStop = 5 -- GPIO14
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
STATE_UP = 1
STATE_DOWN = 2
STATE_STOP = 3
screenCommandState = 0

mqttPrefix="/room/screen/"

---------- Screen control logic

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
    end)
end

-- Publish actual state
publishMovingStart=0
publishMovingDir=0 -- 1 for down; -1 for up
currentPercent=0

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
        m:publish(mqttPrefix .. "percent", percent,0,0)
        print("Now at " .. percent .. "%")

        if ((percent < 0) or (percent > 100)) then
            print("Stop screen by percentage monitoring (" .. percent .. "% )")
            m:publish(mqttPrefix .. "state","wrongPercent:" .. percent,0,0)
            publishEndState()
            if (publishMovingDir == -1) then
                currentPercent=0
            elseif (publishMovingDir==1) then
                currentPercent=100
            end
            publishMovingDir=0
            tmr.stop(1)
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
        end
        print("Screen stop")
        if (dontPublishSomething ~= nil) then
            m:publish(mqttPrefix .. "state","stop",0,0)
        end
        gpio.write(gpioRelayUp, gpio.LOW)
        gpio.write(gpioRelayDown, gpio.LOW)
        tmr.stop(1)
    end
end


mqttIPserver="10.23.42.10"

-- The Mqtt logic
m = mqtt.Client("crash_" .. node.chipid(), 120, "user", "pass")

global_c=nil
function startTcpServer()
    s=net.createServer(net.TCP, 180)
    s:listen(2323,function(c)
    global_c=c
    function s_output(str)
      if(global_c~=nil)
         then global_c:send(str)
      end
    end
    node.output(s_output, 0)
    c:on("receive",function(c,l)
      node.input(l)
    end)
    c:on("disconnection",function(c)
      node.output(nil)
      global_c=nil
    end)
    print("Welcome to the Screen")
    end)
end

function mqttsubscribe()
 tmr.alarm(1,50,0,function() 
        m:subscribe(mqttPrefix .. "command",0, function(conn) 
            print("subscribed") 
            m:publish(mqttPrefix .. "ip",wifi.sta.getip(),0,0)
        end) 
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
      end
   end
end)

setupComplete=false


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
        print("Development-Mode without Wifi connection")
        setupComplete=true
     end
      -- The startup script
      if wifi.sta.status() ~= 5 then
         print("Connecting to AP...")
      else
         setupComplete=true
         print('IP: ',wifi.sta.getip())
         m:connect(mqttIPserver,1883,0)
         startTcpServer()
      end
  end
end)

