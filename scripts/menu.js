function goodbye() {
    cls()
    if (!gfile("logoff")) {
        print("Really Log off? (Y/N)")
    }

    var c = getch()

    if (c != 'n' && c != 'N') {
        cls()
        gfile("goodbye")
        disconnect()
    }
}

function check_email() {
    var unread = unreademail()
    var total = countemail()
    cls()
    if (unread > 0) {
        print("|13You have " + total + " emails, " + unread + " are unread.|07\r\n\r\n");
        print("View your email now? (Y/N)\r\n");

        var c = getch()
        if (c == 'y' || c == 'Y') {
            listemail()
        }
    } else {
        print("|13You have " + total + " emails.|07\r\n\r\n");
        pause()
    }
}

function write_email() {
    print("To: ")
    var user = gets(64)
    var user = checkuser(user)

    if (user == "") {
        print("|12No such user!|07\r\n\r\n")
        return
    }

    print("\r\n|14Sending email to \"" + user + "\"...|07\r\n\r\n")

    print("Subject: ")
    var subject = gets(32)

    enteremail(user, subject)
}

check_email()
exec("last10")
exec("automsg")

scanbases()

while(true) {
    cls()
    if (!gfile("mainmenu")) {
        print(">>>> Main Menu <<<<\r\n")
        print("S - Select Msg Area\r\n")
        print("W - Write Message\r\n")
        print("L - List Messages\r\n")
        print("R - Read Messages\r\n")
        print("A - Automessage\r\n")
        print("O - Oneliners\r\n")
        print("D - Door Games\r\n")
        print("E - Enter Email\r\n")
        print("M - Read Email\r\n")
        print("G - Goodbye!\r\n\r\n")
    }
    print("|08(|10" + getarea() + "|08) :> |07")

    var c = getch()

    print("\r\n\r\n")

    if (c == 'g' || c == 'G') {
        goodbye()
    } else if (c == 'w' || c == 'W') {
        writemsg()
    } else if (c == 'l' || c == 'L') {
        var lr = getmsglr()
        var tot = getmsgtot()

        print("\r\nStart listing at (1-" + tot + ") or (N)ew: ")
        var sel = gets(6)

        if (sel == 'n' || sel == 'N') {
            listmsgs(lr)
        } else if (sel.length == 0) {
            listmsgs(0)
        } else {
            if (Number(sel) - 1 <= 0 || Number(sel) - 1 >= tot) {
                listmsgs(0)
            } else {
                listmsgs(Number(sel) - 1)
            }
        }
    } else if (c == 'r' || c == 'R') {
        var lr = getmsglr()
        var tot = getmsgtot()

        print("\r\nStart reading at (1-" + tot + ") or (N)ew: ")
        var sel = gets(6)

        if (sel == 'n' || sel == 'N') {
            readmsgs(lr)
        } else if (sel.length == 0) {
            readmsgs(0)
        } else {
            if (Number(sel) - 1 <= 0 || Number(sel) - 1 >= tot) {
                readmsgs(0)
            } else {
                readmsgs(Number(sel) - 1)
            }
        }
    } else if (c == 's' || c == 'S') {
        selectarea()
    } else if (c == 'a' || c == 'A') {
        cls()
        exec("automsg")
    } else if (c == 'o' || c == 'O') {
        exec("oneliner")
    } else if (c == 'd' || c == 'D') {
        exec("doors")
    } else if (c == 'e' || c == 'E') {
        write_email()
    } else if (c == 'm' || c == 'M') {
        listemail()
    } else {
        print("|12Huh?|07\r\n\r\n")
        pause()
    }
}