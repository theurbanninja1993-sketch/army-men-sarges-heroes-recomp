// PAL preview supports only the verified English60 secondary executable.
// At this one loader strcat call, r4 is the Klink path and r5 is the
// selected language. Keep the original loader/cleanup, constrain its output.
#include "dreamcomp/hook.h"
#include <cstdio>

DC_HOOK_ENTRY(pal_boot_language) {
    if ((c.pr & 0x1fffffffu) != 0x0c0353bcu ||
        (c.r[4] & 0x1fffffffu) != 0x0c118d70u) return false;
    c.r[5] = 0x8c09d5b0u; // original loader's "English" string
    m.write32(0x8c118f70u, 60); // loader appends "60Hz" when this is 60
    std::fprintf(stderr,"PAL preview: startup selection restricted to English / 60 Hz.\n");
    return false;
}
