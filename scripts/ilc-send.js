var msgbase = "UNDEFINED"
var bbsname = "UNDEFINED"
var bbsaddr = "UNDEFINED"

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

if (bbsname != "UNDEFINED" && msgbase != "UNDEFINED" && bbsaddr != "UNDEFINED") {
    var username = getusername()
    var dt = new Date()

    var current_date;
    var current_time;


    current_date = pad("00", dt.getDate(), true) + "/" + pad("00", dt.getMonth() + 1, true) + "/" + pad("00", dt.getFullYear() - 2000, true);

    if (dt.getHours() < 12) {
        if (dt.getHours() == 0) {
            current_time = "12:" + pad("00", dt.getMinutes(), true) + "a"
        } else {
            current_time = pad("00", dt.getHours(), true) + ":" + pad("00", dt.getMinutes(), true) + "a"
        }
    } else {
        if (dt.getHours() == 12) {
            current_time = "12:" + pad("00", dt.getMinutes(), true) + "p"
        } else {
            current_time = pad("00", dt.getHours() - 12, true) + ":" + pad("00", dt.getMinutes(), true) + "p"
        }
    }
    
    var userlocation = getattr("location", "Somewhere, The World")
    var v = getversion()

    if (isvisible()) {
        var body = ">>> BEGIN\r" + rot47(username) + "\r" + rot47(bbsname) + "\r" + rot47(current_date) + "\r" + rot47(current_time) + "\r" + rot47(userlocation) + "\r" + rot47(v['system']) + "\r" + rot47(bbsaddr) + "\r" + ">>> END\r"

        postmsg(msgbase, "ALL", "ibbslastcall", "ibbslastcall-data", body)
    }
}
