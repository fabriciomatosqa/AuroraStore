#include <orbis/libkernel.h>

// Aplicativo de teste persistente: um processo PS4 não deve encerrar
// imediatamente após a inicialização. A interface será adicionada nesta base.
int main() {
    for (;;) {
        sceKernelUsleep(1000 * 1000);
    }
    return 0;
}
