#include <stdlib.h>
#include <unistd.h>

#define UID 1000

int main(void) {
    setreuid(UID, UID);
    setregid(UID, UID);
    system("/bin/sh");
    return 0;
}