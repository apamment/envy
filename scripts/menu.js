function goodbye() {
    cls()
    if (!gfile("logoff")) {
        print("Really Log off? (Y/N)")
    }

    var c = getch()

    if (c != 'n' && c != 'N') {
        cls()
        slowgfile("goodbye")
        disconnect()
    }
}

function check_email() {
    var unread = unreademail()
    var total = countemail()
    cls()

    if (unread > 0) {
        gfile("emailnew")
        print("|13You have " + total + " emails, " + unread + " are unread.|07\r\n\r\n");
        print("View your email now? (Y/N)\r\n");

        var c = getch()
        if (c == 'y' || c == 'Y') {
            listemail()
        }
    } else {
        gfile("emailold")
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

exec("ilc-send")
cls()
if (gfile("system")) {
    pause()
}
cls()
if (gfile("sysnews")) {
    pause()
}
check_email()
exec("last10")
exec("automsg")
cls()
print("|10Scanning message bases...|07\r\n")
scanbases()

while (true) {
    cls()
    setaction("Browsing main menu")
    if (!gfile("mainmenu")) {
        print("|10                          >>>> Main Menu <<<<\r\n")
        print("|15J|07 - Select Msg Group    |152|07 - System News        |15$|07 - User Settings\r\n")
        print("|15S|07 - Select Msg Area     |15U|07 - User List          |15I|07 - System Info\r\n")
        print("|15W|07 - Write Message       |15A|07 - Automessage        |15N|07 - Who's online\r\n")
        print("|15L|07 - List Messages       |15O|07 - Oneliners          |151|07 - IBBS Last Callers\r\n")
        print("|15R|07 - Read Messages       |15D|07 - Door Games         |15T|07 - Transfer Menu\r\n")
        print("|153|07 - IBBS Oneliners\r\n\r\n")
        print("|15E|07 - Enter Email         |15F|07 - Leave Feedback\r\n")
        print("|15M|07 - Read Email          |15G|07 - Goodbye!\r\n\r\n")
    }
    print("|08(|10" + getarea() + "|08) (|11Time: |15" + timeleft() + "m|08)" + " :> |07")

    var c = getch()

    print("\r\n\r\n")

    if (c == 'g' || c == 'G') {
        setaction("Logging off")
        goodbye()
    } else if (c == 'w' || c == 'W') {
        setaction("Posting a message")
        writemsg()
    } else if (c == 'l' || c == 'L') {
        setaction("Listing messages")
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
        setaction("Reading messages")
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
    } else if (c == 'j' || c == 'J') {
        setaction("Selecting message group")
        selectgroup()
    } else if (c == 's' || c == 'S') {
        setaction("Selecting message area")
        selectarea()
    } else if (c == 'a' || c == 'A') {
        setaction("Viewing automessage")
        cls()
        exec("automsg")
    } else if (c == 'o' || c == 'O') {
        setaction("Browsing oneliners")
        exec("oneliner")
    } else if (c == 'd' || c == 'D') {
        setaction("Browsing doors")
        exec("doors")
    } else if (c == 'e' || c == 'E') {
        setaction("Writing an email")
        write_email()
    } else if (c == 'm' || c == 'M') {
        setaction("Listing email")
        listemail()
    } else if (c == 'u' || c == 'U') {
        setaction("Viewing user list")
        exec("userlist")
    } else if (c == 'i' || c == "I") {
        setaction("Viewing system info")
        exec("sysinfo")
    } else if (c == 'f' || c == 'F') {
        setaction("Entering feedback")
        var user = checkuser(opname())
        if (user != "") {
            enteremail(user, "Feedback")
        }
    } else if (c == 'n' || c == 'N') {
        setaction("Viewing who's online")
        exec("nodeuse")
    } else if (c == 't' || c == 'T') {
        exec("xfer")
    } else if (c == '1') {
        setaction("Viewing IBBS last callers")
        exec("ilc")
    } else if (c == '2') {
        setaction("Viewing system news")
        cls()
        if (!gfile("sysnews")) {
            print("|12No news!\r\n");
        }
        pause()
    } else if (c == '3') {
        setaction("Viewing interbbs oneliners")
        exec("ibol")
    } else if (c == '$') {
        exec("usereditor")
    } else {
        print("|12Huh?|07\r\n\r\n")
        pause()
    }
}