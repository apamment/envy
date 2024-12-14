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

while (true) {
    cls()
    setaction("Browsing xfer menu")
    if (!gfile("xfermenu")) {
        print("|10                         >>>> Transfer Menu <<<<\r\n")
        print("|15J|07 - Select File Group   |15C|07 - Clear Tagged Files |15Q|07 - Quit to Main Menu\r\n")
        print("|15S|07 - Select File Area    |15T|07 - List Tagged Files  |15G|07 - Goodbye\r\n")
        print("|15L|07 - List Files          |15U|07 - Upload Files\r\n")
        print("|15D|07 - Download Files\r\n")

    }
    print("|08(|10" + getfilearea() + "|08) (|11Time: |15" + timeleft() + "m|08)" + " :> |07")

    var c = getch()

    print("\r\n\r\n")

    if (c == 's' || c == 'S') {
        setaction("Selecting file area")
        selectfilearea()
    } else if (c == 'l' || c == 'L') {
        setaction("Listing files")
        listfiles()
    } else if (c == 'd' || c == 'D') {
        setaction("Downloading files")
        download()
    } else if (c == 'c' || c == 'C') {
        cleartagged()
    } else if (c == 'q' || c == 'Q') {
        break;
    } else if (c == 't' || c == 'T') {
        setaction("Listing tagged files")
        listtagged()
    } else if (c == 'u' || c == 'U') {
        setaction("Uploading files")
        upload()
    } else if (c == 'j' || c == 'J') {
        setaction("Selecting file group")
        selectfilegroup()
    } else if (c == 'g' || c == 'G') {
        goodbye()
    } else {
        print("|12Huh?|07\r\n\r\n")
        pause()
    }
}