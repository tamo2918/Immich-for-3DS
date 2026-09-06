#include "App.hpp"
#include "Logger.hpp"

#ifdef __3DS__
#include <3ds.h>
#include <malloc.h>

// Increase main thread stack size to 256KB (libctru default is 32KB)
extern "C" u32 __stacksize__ = 256 * 1024;

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000 // 1MB buffer for sockets
static u32* soc_buffer = nullptr;
#endif

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

#ifdef __3DS__
    // Initialize services
    romfsInit();
    cfguInit();

    // Allocate aligned socket memory
    soc_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (soc_buffer) {
        Result res = socInit(soc_buffer, SOC_BUFFERSIZE);
        if (R_FAILED(res)) {
            // Socket init failed
        }
    }
#endif

    {
        App app;
        if (app.init()) {
            app.run();
        }
        app.cleanup();
    }

#ifdef __3DS__
    // Exit services
    if (soc_buffer) {
        socExit();
        free(soc_buffer);
        soc_buffer = nullptr;
    }
    cfguExit();
    romfsExit();
#endif

    return 0;
}
