var users = getusers()
var index = 0

var seclevel = Number(getattr("seclevel", "10"))

while (true) {

    cls()
    print("|14User Editor - |15" + getusername() + "|07\r\n\r\n")
    print("|15(|10L|15)ocation: |07" + getattr("location", "Somewhere, The World") + "\r\n")
    print("|15(|10E|15)mail: |07" + getattr("email", "UNKNOWN") + "\r\n")
    print("|15Pass(|10W|15)ord:|07 ****\r\n")
    print("|15Edit (|10S|15)ignature\r\n")
    print("|15(|10T)|15oggle Signature: |07" + getattr("signature-toggle", "off") + "\r\n")
    if (opname() == getusername() || seclevel >= 99) {
        print("|15(|10!|15) Edit other users\r\n")
    }
    print("\r\n|15(|10Q|15)uit|07\r\n")
    var ch = getch()

    if (ch == 'w' || ch == 'W') {
        print("Current Password: ")
        var p0 = getpass(16)

        if (checkpassword(p0)) {

            print("New Password: ")
            var p1 = getpass(16)
            print("Repeat: ")
            var p2 = getpass(16)

            if (p1.length >= 8 && p1 == p2) {
                setpassword(p1)
                print("|10Password Updated...|07\r\n")
            } else {
                if (p1.length < 8) {
                    print("|12Password too short!|07\r\n")
                } else {
                    print("|12Passwords don't match!|07\r\n")
                }
            }
        } else {
            print("|12Incorrect password!|07\r\n")
        }
        pause()
    } else if (ch == 'e' || ch == 'E') {
        print("New email: ");
        var ne = gets(32)
        if (ne.includes('@') && ne.includes('.')) {
            putattr("email", ne)
            print("|10Email address updated!|07\r\n")
        } else {
            print("|12Invalid Email!|07\r\n")
        }
        pause()
    } else if (ch == 'l' || ch == 'L') {
        print("New location: ");
        var ne = gets(32)
        if (ne.length < 1) {
            print("|12You're nowhere?|07\r\n")
        } else {
            putattr("location", ne)
            print("|10Location updated!|07\r\n")
        }
        pause()
    } else if (ch == 's' || ch == 'S') {
        editsig()
    } else if (ch == 't' || ch == 'T') {
        if (getattr("signature-toggle", "off") == "off") {
            putattr("signature-toggle", "on");
        } else {
            putattr("signature-toggle", "off");
        }
    } else if (ch == '!' && (opname() == getusername() || seclevel >= 99)) {
        exec("sysopeditor")
    } else if (ch == 'q' || ch == 'Q') {
        break;
    }
}