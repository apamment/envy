var users = getusers()
var index = 0

while (true) {

    cls()
    print("User Editor - " + users[index]['username'] + "(" + users[index]['uid'] + ")\r\n\r\n")
    print("(F)ullName: " + getattro(Number(users[index]['uid']), "fullname", "UNKNOWN") + "\r\n")
    print("(S)ecLevel: " + getattro(Number(users[index]['uid']), "seclevel", "10") + "\r\n")
    print("(N)ext, (P)revious, (Q)uit\r\n")
    var ch = getch()

    if (ch == 'p' || ch == 'P') {
        index = index - 1;
        if (index < 0) {
            index = 0
        }
    } else if (ch == 'n' || ch == 'N') {
        index = index + 1
        if (index >= users.length) {
            index = users.length - 1
        }
    } else if (ch == 'f' || ch == 'F') {
        print("New SecLevel: ")
        var sl = Number(gets(3))
        if (sl > 0 && sl <= 99) {
            putattro(Number(users[index]['uid']), "seclevel", sl)
        }
    } else if (ch == 's' || ch == 'S') {
        print("New FullName: ")
        var fn = gets(65)
        putattro(Number(users[index]['uid']), "fullname", fn)
    } else if (ch == 'q' || ch == 'Q') {
        break;
    }
}