var lines = []
for (var i = 0; i < 10; i++) {
    lines.push(load("line-" + i, ""))
}

cls()

if (!gfile("oneliner")) {
    print("O N E L I N E R S\r\n\r\n")
}

for (var i = 0; i < 10; i++) {
    if (lines[i] != "") {
        print(lines[i] + "\r\n")
    }
}

print("\r\nYour Say: ")
var newline = gets(60)

if (newline != "" && newline != "Q" && newline != "q" && !newline.includes('\x1b')) {
    var finalline = "|10" + getusername() + " |07-> |15" + newline + "|07"
   
    for (var i = 0; i < 9; i++) {
        save("line-" + i, lines[i+1])
    }

    save("line-9", finalline)
}