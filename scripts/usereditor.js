var users = getusers()
var index = 0

while (true) {

    cls()
    print("|14User Editor - |15" + getusername() + "|07\r\n\r\n")
    print("|15(|10E|15)mail: |07" + getattr("email", "UNKNOWN") + "\r\n")
    print("|15Pass(|10W|15)ord:|07 ****\r\n")
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
            print("|10Email address updated!\r\n")
        } else {
            print("|12Invalid Email!\r\n")
        }
        pause()
    } else if (ch == 'q' || ch == 'Q') {
        break;
    }
}