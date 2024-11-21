var sysinfo = getversion()

cls()

if (!gfile("sysinfo")) {
    print("S Y S T E M   I N F O\r\n\r\n")
}

print("\r\nEnvy is Copyright (C) 2024; Andrew Pamment\r\n\r\n")

print("    |15Envy Version: |07" + sysinfo['envy'] + "\r\n")
print(" |15Duktape Version: |07" + sysinfo['duktape'] + "\r\n")
print("|15Operating System: |07" + sysinfo['system'] + " (" + sysinfo['sysver'] + ") on " + sysinfo['machine'] + "\r\n\r\n")

pause()

if (gfile("system")) {
    pause()
}
