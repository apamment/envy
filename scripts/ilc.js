var msgbase = "fsx_dat"

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


var entries = []

for (var i = 0; i < 20; i++) {
    var entry;
    entry['username'] = load("user-" + i, "No-one")
    entry['location'] = load("location-" + i, "Nowhere")
    entry['bbsname'] = load("bbsname-" + i, "No BBS")
    entry['udate'] = load("udate-" + i, "00/00/0000")
    
    entries.push(entry)
}

var lr = Number(load("last-read", "0"))
var msg = readmsg(msgbase, lr + 1)

while (typeof msg !== 'undefined') {
    if (msg['subject'] == "ibbslastcall-data" && msg['from'] == "ibbslastcall") {
        var lines = msg['body'].split("\r")
        var start = 0;
        for (;start< lines.length;start++) {
            if (lines[start] == ">>> BEGIN") break;
        }

        if (start + 8 <= lines.length && lines[start + 8] == ">>> END") {
            var entry;
            entry['username'] = rot47(lines[start + 1])
            entry['location'] = rot47(lines[start + 5])
            entry['bbsname'] = rot47(lines[start + 2])
            entry['udate'] = rot47(lines[stat + 3])
            entries.push(entry)
        }
    }

    lr = msg['idx']
    msg = readmsg(msgbase, lr + 1)
}

save("lart-read", lr)


if (entries.length > 20) {
    entries = entries.slice(Math.max(entries.length - 20, 0))
}

for (var i = 0; i < 20; i++) {
    save("user-" + i, entries[i]['username'])
    save("location-" + i, entries[i]['location'])
    save("bbsname-" + i, entries[i]['bbsname'])
    save("udate-" + i, entries[i]['udate'])
}

cls()
if (!gfile("ilc-header")) {
    print("I N T E R B B S   L A S T C A L L E R S\r\n");
}
for (var i = 0; i < 20; i++) {
    print(pad("                  ", entries[i]['username'], false) + " " + pad("                      ", entries[i]['location'], false) + " " + pad("                      ", entries[i]['bbsname'], false) + " " + entries[i]['udate'] + "\r\n")
}

pause()