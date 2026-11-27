# Aurora Store PS4

Projeto experimental para PS4 homebrew.

## Versão 01.04 — interface inicial

A interface agora tem uma home visual, cabeçalho com seções, cards para Jogos, Aplicativos e Ferramentas, estados vazios e navegação pelo DualShock 4.

- **L1/R1:** alternar entre Início, Jogos, Apps e Favoritos.
- **Setas:** mover o foco e percorrer as categorias.
- **X:** selecionar uma categoria ou abrir o estado do catálogo.
- **O:** voltar para Início ou fechar a janela.

O catálogo ainda não contém itens. A interface informa isso ao usuário em vez de mostrar downloads fictícios. A leitura de catálogos remotos, busca, instalação e downloads ainda precisam ser implementados.

## Compilação

GitHub Actions gera o pacote quando há push na branch principal ou execução manual. A compilação valida que o PKG foi gerado; a navegação ainda precisa ser testada no PS4.

## Estrutura

- `src/main.cpp`: tela principal, navegação e entrada do controle.
- `src/graphics.cpp`, `src/graphics.h`: renderer 2D baseado no Scene2D do OpenOrbis, licenciado sob GPL-3.0; licença em `third_party/OpenOrbis-PS4-Toolchain/LICENSE`.
- `assets/aurora-icon.png.b64`: imagem do usuário codificada como Base64; `tools/generate_icon.py` a converte para o PNG RGB 512 × 512 do pacote.
- `catalogs/homebrew.json`: catálogo local, atualmente vazio.

## Compatibilidade

O projeto compila com OpenOrbis 0.5.4. Compilar o pacote não confirma suporte em cada firmware ou configuração de jailbreak.
