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

    for (let index = 0; index < doors.length; ++index) {
        const element = theArray[index];
        print(pad("   ", index + 1, true) + ". " + element['name'])
    }
    var ch = get_str(3)

    if (ch == "q" || ch == "Q") {
        return;
    } else {
        var num = Number(ch) - 1;
        if (num >= 0 && num < doors.length) {
            rundoor(theArray[num]['key'])
        }
    }
}