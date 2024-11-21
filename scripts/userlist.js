function pad(pad, str, padLeft) {
    if (padLeft) {
      return (pad + str).slice(-pad.length);
    } else {
      return (str + pad).substr(0, pad.length);
    }
}

cls()

if (!gfile("userlist")) {
    print("U S E R L I S T\r\n\r\n")
}

var users = getusers()
var lines = 0;
for (var i = 0; i < users.length; i++) {
    print("    |15" + pad("                 ", users[i]['username'], false) + " |11" + pad("                                 ", getattro(users[i]['uid'], "location", "Somewhere, The World"), false) + " |13" + pad("      ", getattro(users[i]['uid'], "total-calls", "0"), true) + " calls|07\r\n")
    lines++;
    if (lines == 20) {
        pause()
        lines = 0
    }
}

pause()