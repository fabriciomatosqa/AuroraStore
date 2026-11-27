# Aurora Store PS4

Projeto experimental para PS4 homebrew.

## Versão 01.03

Esta versão substitui o executável sem interface por uma tela de inicialização visível, usando a classe Scene2D de exemplo do OpenOrbis. Se a tela “TESTE VISUAL: APP ABRIU” aparecer no console, o executável inicializou e conseguiu enviar um quadro para a saída de vídeo.

O ícone do pacote agora vem da imagem enviada pelo usuário. O arquivo-fonte codificado em `assets/aurora-icon.png.b64` é decodificado por `tools/generate_icon.py` para `sce_sys/icon0.png`. O workflow valida 512 × 512, PNG, RGB e inclui o caminho no GP4.

**Esta compilação ainda não é uma loja funcional:** catálogo, interface de navegação, conexão HTTP e downloads não estão implementados. A tela serve para diagnosticar a abertura do aplicativo.

## Compilação

Cada push na branch principal e cada execução manual acionam GitHub Actions. A compilação confirma que o PKG foi criado; ela não consegue validar execução no console. O teste final da tela precisa ser feito no PS4.

## Estrutura

- `src/main.cpp`: tela de inicialização e verificação visual.
- `src/graphics.cpp`, `src/graphics.h`: renderer 2D de exemplo do OpenOrbis, licenciado sob GPL-3.0; licença em `third_party/OpenOrbis-PS4-Toolchain/LICENSE`.
- `assets/aurora-icon.png.b64`: PNG original convertido para 512 × 512 RGB e codificado em Base64 para o workflow.
- `.github/workflows/build.yml`: compilação OpenOrbis.

## Compatibilidade

O pacote usa OpenOrbis 0.5.4. A compilação bem-sucedida não garante compatibilidade com cada firmware ou configuração de jailbreak. A versão 01.03 precisa ser instalada e testada no console.
