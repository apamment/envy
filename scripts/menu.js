exec("automsg")

while(true) {
    cls()
    if (!gfile("mainmenu")) {
        print(">>>> Main Menu <<<<\r\n")
        print("T - Test JS\r\n")
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
    } else if (c == 't' || c == 'T') {
        exec("test")
    } else if (c == 'a' || c == 'A') {
        cls()
        exec("automsg")
    } else {
        print("Huh?")
        getch()
    }
}