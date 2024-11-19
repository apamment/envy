function pad(pad, str, padLeft) {
    if (padLeft) {
      return (pad + str).slice(-pad.length);
    } else {
      return (str + pad).substr(0, pad.length);
    }
}
  

var doors = getdoors()

while (true) {
    cls()
    if (!gfile("doors")) {
        print("D O O R S\r\n\r\n");
    }
    for (var index = 0; index < doors.length; ++index) {
        var element = doors[index];
        print("|15" + pad("   ", index + 1, true) + ". |10" + pad("                                ", element['name'], false) + " |13" + pad("     ", Number(load(element['key'], "0")), true) + " PLAYS|07\r\n")
    }
    print("|15  Q. |10Quit to Main Menur\r\n");
    print("\r\n|08:> |07")
    var ch = gets(3)

    if (ch == "q" || ch == "Q") {
        break
    } else {
        var num = Number(ch) - 1;
        if (num >= 0 && num < doors.length) {
            rundoor(doors[num]['key'])
            plays = Number(load(doors[num]['key'], "0")) + 1;
            save(doors[num]['key'], plays)
        }
    }
}