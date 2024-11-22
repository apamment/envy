var users = getusers()
var index = 0

while (true) {

    cls()
    print("User Editor - " + getusername() + "\r\n\r\n")
    print("Pass(W)ord: ****\r\n")
    print("\r\n(Q)uit\r\n")
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
                print("Password Updated...\r\n")
            } else {
                if (p1.length < 8) {
                    print("Password too short!\r\n")
                } else {
                    print("Passwords don't match!\r\n")
                }
            }
        } else {
            print("Incorrect password!\r\n")
        }
        pause()
    } else if (ch == 'q' || ch == 'Q') {
        break;
    }
}