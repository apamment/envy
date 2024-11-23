function pad(pad, str, padLeft) {
    if (padLeft) {
        return (pad + str).slice(-pad.length);
    } else {
        return (str + pad).substr(0, pad.length);
    }
}

cls()

var a = getactions()

print("|14Node# User             Location                Action\r\n")

for (var i = 0; i < a.length; i++) {
    if (a[i].uid > 0) {
        print("|12" + pad("     ", (i + 1), true) + " |10" + pad("                ", getusernameo(a[i].uid), false) + " |13" + pad("                       ", getattro(a[i].uid, "location", "Somewhere, The World"), false) + " |15" + a[i].action + "\r\n")
    } else {
        print("|08" + pad("     ", (i + 1), true) + " Waiting for caller...\r\n")
    }
}

pause()