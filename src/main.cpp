#include <orbis/libkernel.h>

// Primeiro pacote de validação da toolchain.
// A interface gráfica da loja será adicionada depois que o ciclo de
// compilação/empacotamento estiver validado no PS4.
int main() {
    sceKernelUsleep(1000 * 1000);
    return 0;
}
