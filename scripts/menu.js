exec("automsg")

while(true) {
    cls()
    if (!gfile("mainmenu")) {
        print(">>>> Main Menu <<<<\r\n")
        print("W - Write Message\r\n")
        print("L - List Messages\r\n")
        print("R - Read Messages\r\n")
        print("A - Automessage\r\n")
        print("G - Goodbye!\r\n\r\n")
    }
    print(":> ")

    var c = getch()

    print("\r\n\r\n")

    if (c == 'g' || c == 'G') {
        gfile("goodbye")
        disconnect()
        break
    } else if (c == 'w' || c == 'W') {
        writemsg()
    } else if (c == 'l' || c == 'L') {
        listmsgs(0)
    } else if (c == 'r' || c == 'R') {
        readmsgs(0);
    } else if (c == 'a' || c == 'A') {
        cls()
        exec("automsg")
    } else {
        print("Huh?\r\n")
        getch()
    }
}