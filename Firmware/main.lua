dofile("wlancfg.lua")
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

mqttPrefix="/room/screen2/"

---------- Screen control logic
-- timer to always stop the relay after 50 seconds
tmr.alarm(6, 50000, tmr.ALARM_SINGLE, function()
    if (gpio.read(gpioRelayUp) == gpio.HIGH) then
        m:publish(mqttPrefix .. "state","up",0,1)
    end
    if (gpio.read(gpioRelayDown) == gpio.HIGH) then
        m:publish(mqttPrefix .. "state","down",0,1)
    end
    -- stop both relais
    commandScreenStop()
    print("Timer stopped relais")
end)
tmr.stop(6)

function commandScreenUp()
    if (screenCommandState ~= STATE_UP) then
        screenCommandState = STATE_UP
        print("Screen up")
        m:publish(mqttPrefix .. "state","movingup",0,1)
        gpio.write(gpioRelayDown, gpio.LOW)   
        tmr.delay(50000) -- wait 50 ms
        gpio.write(gpioRelayUp, gpio.HIGH)
       tmr.start(6)      
   end
end

function commandScreenDown()
    if (screenCommandState ~= STATE_DOWN) then
       screenCommandState = STATE_DOWN
       print("Screen down")
       m:publish(mqttPrefix .. "state","movingdown",0,1)
       gpio.write(gpioRelayUp, gpio.LOW)   
       tmr.delay(50000) -- wait 50 ms
       gpio.write(gpioRelayDown, gpio.HIGH)
       tmr.start(6) 
   end
end

function commandScreenStop(dontPublishSomething)
    if (screenCommandState ~= STATE_STOP) then
        screenCommandState = STATE_STOP
        print("Screen stop")
        if (dontPublishSomething ~= nil) then
            m:publish(mqttPrefix .. "state","stop",0,1)
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
    print("Welcome to the Trafficlight")
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

