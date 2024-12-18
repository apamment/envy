function pad(pad, str, padLeft) {
    if (padLeft) {
        return (pad + str).slice(-pad.length);
    } else {
        return (str + pad).substr(0, pad.length);
    }
}

var lines = []
for (var i = 0; i < 10; i++) {
    lines.push(load("line-" + i, ""))
}

var thetime = new Date();
var call_count = Number(load("call-count", "0")) + 1;
var finalline = "|07" + pad("      ", call_count, true) + ". |15" + pad("                 ", getusername(), false) + " |10" + pad("                                ", getattr("location", "Somewhere, The World"), false) + " |13" + thetime.toDateString() + "|07"

if (isvisible()) {
    save("call-count", call_count)

    for (var i = 1; i < 10; i++) {
        save("line-" + i, lines[i - 1])
    }

    save("line-0", finalline)
}
