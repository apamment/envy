while(true) {
    print("\x1b[2J\x1b[1;1H")
    print(">>>> Main Menu <<<<\r\n")
    print("G - Goodbye!\r\n\r\n")
    print(":> ")

    var c = getch()

    print("\r\n\r\n")

    if (c == 'g' || c == 'G') {
        disconnect()
        break
    } else {
        print("Huh?")
        getch()
    }
}