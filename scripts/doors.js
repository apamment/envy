function pad(pad, str, padLeft) {
    if (padLeft) {
        return (pad + str).slice(-pad.length);
    } else {
        return (str + pad).substr(0, pad.length);
    }
}


var doors = getdoors()

if (doors.length == 0) {
    cls()
    print("Sorry, no doors are installed on this system...\r\n")
    pause()
} else {
    while (true) {
        cls()
        if (!gfile("doors")) {
            print("D O O R S\r\n\r\n");
        }

        var lines = 0

        for (var index = 0; index < doors.length; index += 2) {
            var element = doors[index];
            print("|15" + pad("   ", index + 1, true) + ". |10" + pad("                           ", element['name'], false) + " |13" + pad("     ", Number(load(element['key'], "0")), true) + "|07")
            if (index + 1 < doors.length) {
                element = doors[index + 1];
                print("|15" + pad("   ", index + 2, true) + ". |10" + pad("                           ", element['name'], false) + " |13" + pad("     ", Number(load(element['key'], "0")), true) + "|07")
            }
            print("\r\n")
            lines = lines + 1
            if (lines == 15) {
                pause()
                lines = 0
            }
        }
        print("\r\n|15  Q. |10Quit to Main Menu\r\n");
        print("\r\n|08(|11Time: |15" + timeleft() + "m|08)" + " :> |07")
        var ch = gets(3)

        if (ch == "q" || ch == "Q") {
            break
        } else {
            var num = Number(ch) - 1;
            if (num >= 0 && num < doors.length) {
                setaction("Playing " + doors[num]['name'])
                rundoor(doors[num]['key'])
                plays = Number(load(doors[num]['key'], "0")) + 1;
                save(doors[num]['key'], plays)
            }
        }
    }
}