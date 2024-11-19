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
    print("D O O R S\r\n");

    for (var index = 0; index < doors.length; ++index) {
        var element = doors[index];
        print(pad("   ", index + 1, true) + ". " + pad("                                ", element['name'], false) + pad("     ", load(element['key'], "0"), true) + " PLAYS\r\n")
    }

    print("\r\n|08:> |07")
    var ch = gets(3)

    if (ch == "q" || ch == "Q") {
        break
    } else {
        var num = Number(ch) - 1;
        if (num >= 0 && num < doors.length) {
            rundoor(doors[num]['key'])
            plays = Number(load(doors[num]['key'])) + 1;
            save(doors[num]['key'], plays)
        }
    }
}