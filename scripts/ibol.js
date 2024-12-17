var msgbase = "UNDEFINED"
var bbsname = "UNDEFINED"

function pad(pad, str, padLeft) {
    if (padLeft) {
        return (pad + str).slice(-pad.length);
    } else {
        return (str + pad).substr(0, pad.length);
    }
}

function post_oneliner() {
    var msg = "Author: " + getusername() + "\r" + "Source: " + bbsname + "\r"

    for (var i = 0; i < 5; i++) {
        print("Line " + (i + 1) + ": ")
        var newline = gets(60)

        if (newline != "" && newline != "Q" && newline != "q" && !newline.includes('\x1b')) {
            msg = msg + "Oneliner: " + newline + "\r"
        } else {
            if (i == 0 || newline.includes('\x1b')) {
                print("|12Aborted!")
                pause()
                return
            }
            break;
        }
    }

    postmsg(msgbase, "IBBS1LINE", getusername(), "InterBBS Oneliner", msg)
}

if (msgbase == "UNDEFINED" || bbsname == "UNDEFINED") {
    print("|14Sorry, this BBS is not configured for InterBBS Last Callers at this stage.\r\n")
    pause()
} else {
    cls()
    if (!gfile("ibol")) {
        print("I N T E R B B S   O N E L I N E R S\r\n\r\n")
    }

    var entries = []

    for (var i = 0; i < 10; i++) {
        var entry = {}
        entry['author'] = load("author-" + i, "No-one")
        entry['source'] = load("source-" + i, "Nowhere")
        entry['oneliner'] = load("oneliner-" + i, "")
        
        entries.push(entry)
    }

    var lr = Number(load("last-read", "0"))
    var msg = readmsg(msgbase, lr + 1)

    while (msg !== undefined) {
        if (msg['subject'] == "InterBBS Oneliner" && msg['to'] == "IBBS1LINE") {
            var lines = msg['body'].split("\r")
            var oneliner = ""
            var author = ""
            var source = ""
            for (var i = 0; i < lines.length; i++) {
                if (lines[i].substr(0, 7).toUpperCase() == "AUTHOR:") {
                    author = lines[i].substr(8).replace(/\|\d{2}/g, "")
                } else if (lines[i].substr(0, 7).toUpperCase() == "SOURCE:") {
                    source = lines[i].substr(8).replace(/\|\d{2}/g, "")
                } else if (lines[i].substr(0, 9).toUpperCase() == "ONELINER:") {
                    if (oneliner == "") {
                        oneliner = lines[i].substr(10).replace(/\|\d{2}/g, "").replace(
                            /(?![^\n]{1,57}$)([^\n]{1,57})\s/g, '$1\r\n')
                    } else {
                        oneliner = oneliner + "\r\n" + lines[i].substr(10).replace(/\|\d{2}/g, "").replace(
                            /(?![^\n]{1,57}$)([^\n]{1,57})\s/g, '$1\r\n')
                    }
                    
                }
            }
            if (oneliner != "") {
                var entry = {
                    "author": author,
                    "source": source,
                    "oneliner": oneliner
                }            
                entries.push(entry)
            }
        }

        lr = msg['idx']
        msg = readmsg(msgbase, lr + 1)
    }

    save("last-read", lr)

    if (entries.length > 10) {
        entries = entries.slice(entries.length - 10)
    }

    for (var i = 0; i < 10; i++) {
        save("author-" + i, entries[i]['author'])
        save("source-" + i, entries[i]['source'])
        save("oneliner-" + i, entries[i]['oneliner'])
    }

    var start = 0;
    var total = 0;
    for (var i = 9; i >= 0; i--) {
        var count = (entries[i]['oneliner'].match(/\r\n/g) || []).length;
        if (count < 3) {
            count = 3;
        }
        if (total + count > 18) {
            start = i + 1;
            break;
        }
        total += count;
    }

    for (var i = start; i < 10; i++) {
        var lines = entries[i]['oneliner'].split("\r\n")
        if (lines.length >= 2) {
            for (var j = 0; j < lines.length; j++) {
                if (j == 0) {
                    print("|15" + pad("                    ", entries[i].author, false) + "|08: |07" + pad("                                                         ", lines[j]) + "\r\n")
                } else if (j == 1) {
                    print("|10" + pad("                    ", entries[i].source, false) + "|08: |07" + pad("                                                         ", lines[j]) + "\r\n")
                } else {
                    print("                    |08: |07" + pad("                                                         ", lines[j]) + "\r\n")
                }
            }
        } else {
            print("|15" + pad("                    ", entries[i].author, false) + "|08: |07" + pad("                                                         ", lines[0]) + "\r\n")
            print("|10" + pad("                    ", entries[i].source, false) + "|08: \r\n")
        }
        print("|08-------------------------------------------------------------------------------|07\r\n")
    }

    print("|15Add something? |08(|10Y|08/|15N|08)|07")

    var ch = getch()

    print("\r\n\r\n")

    if (ch == 'y' || ch == 'Y') {
        post_oneliner()        
    }
}
