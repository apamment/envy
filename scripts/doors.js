function pad(pad, str, padLeft) {
    if (padLeft) {
        return (pad + str).slice(-pad.length);
    } else {
        return (str + pad).substr(0, pad.length);
    }
}


function list_category(category, doors) {
    while (true) {
        cls()
        if (!gfile("doors")) {
            print("D O O R S - " + category + "\r\n\r\n");
        }

        var lines = 0

        print("|18|15 ##  |14Door                        |11PLAYS  |15 ##  |14Door                        |11PLAYS |16|07\r\n")

        for (var index = 0; index < doors.length; index += 2) {
            var element = doors[index];
            print("|15" + pad("   ", index + 1, true) + ". |10" + pad("                           ", element['name'], false) + " |03" + pad("     ", Number(load(element['key'], "0")), true) + "|07")
            if (index + 1 < doors.length) {
                element = doors[index + 1];
                print("  |15" + pad("   ", index + 2, true) + ". |10" + pad("                           ", element['name'], false) + " |03" + pad("     ", Number(load(element['key'], "0")), true) + "|07")
            }
            print("\r\n")
            lines = lines + 1
            if (lines == 15) {
                pause()
                cls()
                if (!gfile("doors")) {
                    print("D O O R S\r\n\r\n");
                }
                print("|18|15 ##  |14Door                        |11PLAYS  |15 ##  |14Door                        |11PLAYS |16|07\r\n")
                lines = 0
            }
        }
        print("\r\n|15  Q. |10Go back\r\n");
        print("\r\n|08 (|11" + category + "|08) (|11Time: |15" + timeleft() + "m|08) :> |07")
        var ch = gets(3)

        if (ch == "q" || ch == "Q") {
            break
        } else {
            var num = Number(ch) - 1;
            if (num >= 0 && num < doors.length) {
                var plays = load(doors[num]['key'], "0");
                rundoor(doors[num]['key'])
                plays++
                save(doors[num]['key'], plays)
            }
        }
    }
}

var doors = getdoors()

if (doors.length == 0) {
    cls()
    print("|14Sorry, no doors are installed on this system...|07\r\n")
    pause()
} else {
    var categories = []

    for (var i = 0; i < doors.length; i++) {
        var found = false;
        for (var j = 0; j < categories.length; j++) {
            if (categories[j] == doors[i]['category']) {
                found = true
                break
            }
        }

        if (!found) {
            categories.push(doors[i]['category'])
        }
    }

    while(true) {
        cls()
        if (!gfile("doors")) {
            print("D O O R S\r\n\r\n");
        }

        var lines = 0

        print("|18|15 ##  |14Category                          |15 ##  |14Category                           |16|07\r\n")

        for (var index = 0; index < categories.length; index += 2) {
            var element = categories[index];
            print("|15" + pad("   ", index + 1, true) + ". |10" + pad("                                ", element, false) + "|07")
            if (index + 1 < categories.length) {
                element = categories[index + 1];
                print("  |15" + pad("   ", index + 2, true) + ". |10" + pad("                                ", element, false) + "|07")
            }
            print("\r\n")
            lines = lines + 1
            if (lines == 15) {
                pause()
                cls()
                if (!gfile("doors")) {
                    print("D O O R S\r\n\r\n");
                }
                print("|18|15 ##  |14Category                          |15 ##  |14Category                           |16|07\r\n")
                lines = 0
            }
        }
        print("\r\n|15  Q. |10Quit to Main\r\n");
        print("\r\n|08 (|11Time: |15" + timeleft() + "m|08) " + ":> |07")
        var ch = gets(3)

        if (ch == "q" || ch == "Q") {
            break
        } else {
            var num = Number(ch) - 1;
            if (num >= 0 && num < categories.length) {
                var door_cat = []
                for (var i = 0; i < doors.length; i++) {
                    if (doors[i]["category"] === categories[num]) {
                        door_cat.push(doors[i])
                    }
                }
                list_category(categories[num], door_cat)
            }
        }
    }
}
