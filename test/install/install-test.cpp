#include <scn/scn.h>

int main() {
    int i;
    auto ret = scn::scan("0", "{}", i);
    if (!ret) {
        return 1;
    }
    return i;
}
