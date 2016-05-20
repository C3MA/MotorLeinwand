-- Delay the boot

tmr.alarm(2, 1000, tmr.ALARM_SINGLE, function()
    if (file.open("main.lua")) then
        dofile("main.lua")
    else
        print("No Main file found")
    end
end)
print("Autostart in 1 Second")
