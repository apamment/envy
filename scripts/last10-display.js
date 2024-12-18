function pad(pad, str, padLeft) {
    if (padLeft) {
        return (pad + str).slice(-pad.length);
    } else {
        return (str + pad).substr(0, pad.length);
    }
}


var lines = []
for (var i = 0; i < 10; i++) {
    lines.push(loado("last10", "line-" + i, ""))
}

cls()

if (!gfile("last10")) {
    print("L A S T 1 0\r\n\r\n")
}

for (var i = 0; i < 10; i++) {
    if (lines[i] != "") {
        print(lines[i] + "\r\n")
    }
}

print("\r\n")
pause()