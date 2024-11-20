var users = getusers()
var index = 0

while (true) {

    cls()
    print("User Editor - " + users[index]['username'] + "(" + users[index]['uid'] + ")\r\n\r\n")
    print("(F)ullName: " + getattro(Number(users[index]['uid']), "fullname", "UNKNOWN") + "\r\n")
    print("(L)ocation: " + getattro(Number(users[index]['uid']), "location", "Somewhere, The World") + "\r\n")
    print("(E)mail: " + getattro(Number(users[index]['uid']), "email", "UNKNOWN") + "\r\n")
    print("(S)ecLevel: " + getattro(Number(users[index]['uid']), "seclevel", "10") + "\r\n")
    print("\r\n(N)ext, (P)revious, (Q)uit\r\n")
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
    } else if (ch == 's' || ch == 'S') {
        print("New SecLevel: ")
        var sl = Number(gets(3))
        if (sl > 0 && sl <= 99) {
            putattro(Number(users[index]['uid']), "seclevel", sl)
        }
    } else if (ch == 'f' || ch == 'F') {
        print("New FullName: ")
        var fn = gets(65)
        putattro(Number(users[index]['uid']), "fullname", fn)
    } else if (ch == 'l' || ch == 'L') {
        print("New Location: ")
        var lc = gets(32)
        putattro(Number(users[index]['uid']), "location", lc)
    } else if (ch == 'e' || ch == 'E') {
        print("New Email: ")
        var em = gets(32)
        putattro(Number(users[index]['uid']), "email", em)        
    } else if (ch == 'q' || ch == 'Q') {
        break;
    }
}