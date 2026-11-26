#include <cstdio>
#include <cstdlib>
#include <orbis/libkernel.h>

int main(void)
{
    // Minimal runtime check based on the official OpenOrbis hello_world sample.
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("Aurora Store PS4 1.01 - runtime started\\n");
    sceKernelUsleep(2 * 1000000);

    // Keep the application process alive while the UI is not implemented yet.
    for (;;) {
        sceKernelUsleep(1000 * 1000);
    }
}
