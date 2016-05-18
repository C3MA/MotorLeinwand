-- Delay the boot

tmr.alarm(2, 1000, tmr.ALARM_SINGLE, function()
    dofile("main.lua")
end)
print("Autostart in 1 Second")