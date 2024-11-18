function enter_automsg() {
    print("\r\n\r\nEnter your auto message (you have 5 lines)\r\n")

    var amsg = ""

    for (var i = 0; i < 5; i++) {
        print((i + 1) + ": ")
        var line = gets(75)
        if (line.length == 0) {
            break;
        }
        amsg = amsg + line + "\r\n"
    }

    if (amsg.length == 0) {
        print("\r\nCancelled!\r\n")
        return;
    }

    save("automsg", amsg);
    save("username", getusername())
    save("date", Date.now())

    print("\r\nSaved!\r\n")
}

cls()

if (!gfile("automsg")) {
    print("A U T O M S G\r\n")
}

var amsg = load("automsg", "")

if (amsg == "") {
    print("No automessage...\r\n\r\n")
} else {
    print(amsg + "\r\n");

    var date = new Date(Number(load("date", "0")))

    print(" -- " + load("username", "Unknown"))
    print(" (" + date.toDateString() + ")\r\n\r\n")
}

print("E. Enter Auto Message\r\n")
print("\r\n|08:> |07")
var ch = getch()

if (ch == 'e' || ch == 'E') {
    enter_automsg()
}
