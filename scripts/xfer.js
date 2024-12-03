while (true) {
    cls()
    setaction("Browsing xfer menu")
    if (!gfile("xfermenu")) {
        print("|10                         >>>> Transfer Menu <<<<\r\n")
        print("|15S|07 - Select File Area    |15C|07 - Clear Tagged Files |15Q|07 - Quit to Main Menu\r\n")
        print("|15L|07 - List Files          |15T|07 - List Tagged Files\r\n")
        print("|15D|07 - Download Files      |15U|07 - Upload Files\r\n")

    }
    print("|08(|10" + getfilearea() + "|08) (|11Time: |15" + timeleft() + "m|08)" + " :> |07")

    var c = getch()

    print("\r\n\r\n")

    if (c == 's' || c == 'S') {
        selectfilearea()
    } else if (c == 'l' || c == 'L') {
        listfiles()
    } else if (c == 'd' || c == 'D') {
        download()
    } else if (c == 'c' || c == 'C') {
        cleartagged()
    } else if (c == 'q' || c == 'Q') {
        break;
    } else {
        print("|12Huh?|07\r\n\r\n")
        pause()
    }
}