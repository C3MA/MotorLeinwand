-- Delay the boot

ws2812.write(4, string.char(0,128,0)) -- RED
tmr.alarm(2, 1000, tmr.ALARM_SINGLE, function()
    if (file.open("main.lua")) then
        print("Compile sourcecode")
        node.compile("main.lua")
        file.remove("main.lua")
        node.restart()
    elseif (file.open("main.lc")) then
        dofile("main.lc")
    else
        print("No Main file found")
    end
end)
print("Autostart in 1 Second")
