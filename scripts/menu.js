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
        listmsgs(0)
    } else if (c == 'r' || c == 'R') {
        readmsgs(0)
    } else if (c == 's' || c == 'S') {
        selectarea()
    } else if (c == 'a' || c == 'A') {
        cls()
        exec("automsg")
    } else if (c == 'o' || c == 'O') {
        exec("oneliner")
    } else {
        print("|12Huh?|07\r\n\r\n")
        pause()
    }
}