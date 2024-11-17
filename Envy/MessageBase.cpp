extern "C" {
    #include <jamlib/jam.h>
}
#include <string>
#include "MessageBase.h"
#include "Node.h"

void MessageBase::list_messages(Node *n, int startingat) {
    s_JamBase *jb;
    if (JAM_OpenMB((uchar *)std::string(n->get_msg_path() + "/" + file).c_str(), &jb) == 0) {
        
        JAM_CloseMB(jb);
    }
}