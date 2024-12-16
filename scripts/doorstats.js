function compare(a, b) {
    if (a['runs'] > b['runs']) {
        return -1
    } else if (a['runs'] < b['runs']) {
        return 1
    }
    return 0
}

function pad(pad, str, padLeft) {
    if (padLeft) {
        return (pad + str).slice(-pad.length);
    } else {
        return (str + pad).substr(0, pad.length);
    }
}


var doors = getdoors()

if (doors.length > 0) {
    var doorarr = []
    var total_plays = 0
    for (var i = 0; i < doors.length; i++) {
        var doordef = {}
        var plays = Number(loado('doors', doors[i]['key'], '0'))

        if (plays == 0) {
            continue
        }
        total_plays += plays

        doordef['name'] = doors[i]['name']
        doordef['runs'] = plays

        doorarr.push(doordef)
    }

    doorarr.sort(compare)

    var top_door_plays = doorarr[0]['runs']

    cls()
    if (!gfile("doorstats")) {
        print("D O O R   S T A T S\r\n\r\n")
    }

    print("|18|12 # |15DOORNAME                           |11PLAYS |13RANK                           |14   %|16|07\r\n")

    for (var i = 0; i < 15 && i < doorarr.length; i++) {
        var percentage = (doorarr[i]['runs'] / total_plays * 100).toFixed(0)
        var bars = (doorarr[i]['runs'] / top_door_plays * 30).toFixed(0)

        var pbars = ""

        for (var j = 0; j < bars; j++) {
            pbars += '#'
        }

        print("|12" + pad("  ", i + 1, true) + " |07" + pad("                                  ", doorarr[i]['name'], false) + " |03" + pad("     ", doorarr[i]['runs'], true) + " |05" + pad("                              ", pbars, false) + " |06" + pad("   ", percentage, true) + "%\r\n")
    }
    pause()
}