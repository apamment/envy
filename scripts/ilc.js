var msgbase = "UNDEFINED"

function pad(pad, str, padLeft) {
    if (padLeft) {
        return (pad + str).slice(-pad.length);
    } else {
        return (str + pad).substr(0, pad.length);
    }
}

function rot47(s) {
    var str = []
    for (var i = 0; i < s.length; i++) {
        var j = s.charCodeAt(i);
        if ( j >= 33 && j <= 126) {
            str[i] = String.fromCharCode(33+((j+ 14)%94))
        } else {
            str[i] = String.fromCharCode(j)
        }
    }
    return str.join('')
}

if (msgbase == "UNDEFINED") {
    print("|14Sorry, this BBS is not configured for InterBBS Last Callers at this stage.\r\n")
} else {

    var entries = []

    for (var i = 0; i < 15; i++) {
        var entry = {}
        entry['username'] = load("user-" + i, "No-one")
        entry['location'] = load("location-" + i, "Nowhere")
        entry['bbsname'] = load("bbsname-" + i, "No BBS")
        entry['udate'] = load("udate-" + i, "00/00/0000")
        
        entries.push(entry)
    }

    var lr = Number(load("last-read", "0"))
    var msg = readmsg(msgbase, lr + 1)

    while (msg !== undefined) {
        if (msg['subject'] == "ibbslastcall-data" && msg['from'] == "ibbslastcall") {
            var lines = msg['body'].split("\r")
            var start = 0;
            for (;start< lines.length;start++) {
                if (lines[start] == ">>> BEGIN") break;
            }
            if (start + 8 <= lines.length && lines[start + 8] == ">>> END") {
                var entry = {}
                entry['username'] = rot47(lines[start + 1])
                entry['location'] = rot47(lines[start + 5])
                entry['bbsname'] = rot47(lines[start + 2])
                entry['udate'] = rot47(lines[start + 3])
                entries.push(entry)
            }
        }

        lr = msg['idx']
        msg = readmsg(msgbase, lr + 1)
    }

    save("last-read", lr)

    if (entries.length > 15) {
        entries = entries.slice(entries.length - 15)
    }

    for (var i = 0; i < 15; i++) {
        save("user-" + i, entries[i]['username'])
        save("location-" + i, entries[i]['location'])
        save("bbsname-" + i, entries[i]['bbsname'])
        save("udate-" + i, entries[i]['udate'])
    }

    cls()
    if (!gfile("ilc-header")) {
        print("I N T E R B B S   L A S T C A L L E R S\r\n\r\n");
    }
    for (var i = 0; i < 15; i++) {
        print("|15" + pad("                  ", entries[i]['username'], false) + " |13" + pad("                      ", entries[i]['location'], false) + " |10" + pad("                      ", entries[i]['bbsname'], false) + " |11" + entries[i]['udate'] + "|07\r\n")
    }
}
pause()