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

exec("automsg")

while(true) {
    cls()
    if (!gfile("mainmenu")) {
        print(">>>> Main Menu <<<<\r\n")
        print("W - Write Message\r\n")
        print("L - List Messages\r\n")
        print("R - Read Messages\r\n")
        print("A - Automessage\r\n")
        print("O - Oneliners\r\n")
        print("G - Goodbye!\r\n\r\n")
    }
    print(":> ")

    var c = getch()

    print("\r\n\r\n")

    if (c == 'g' || c == 'G') {
        goodbye()
    } else if (c == 'w' || c == 'W') {
        writemsg()
    } else if (c == 'l' || c == 'L') {
        listmsgs(0)
    } else if (c == 'r' || c == 'R') {
        readmsgs(0);
    } else if (c == 'a' || c == 'A') {
        cls()
        exec("automsg")
    } else if (c == 'o' || c == 'O') {
        exec("oneliner")
    } else {
        print("Huh?\r\n")
        getch()
    }
}