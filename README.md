# Aurora Store PS4

Projeto experimental de uma loja nativa para PS4 homebrew.

## Estado atual

Esta primeira entrega configura a compilação automatizada de um **pacote de teste (smoke test)** com OpenOrbis. Ainda não é a loja visual final: a interface gráfica, navegação pelo DualShock 4, consulta HTTP dos catálogos e gerenciador de downloads serão implementados e testados em etapas seguintes.

O catálogo inicial usa um JSON próprio e aceita somente itens cuja distribuição seja autorizada pelo responsável.

## Compilação

Cada push na branch principal e cada execução manual acionam GitHub Actions. O artefato esperado é publicado como `AuroraStore-PS4`.

## Estrutura

- `src/main.cpp`: ponto de entrada nativo para validar o runtime.
- `catalogs/homebrew.json`: formato inicial do catálogo remoto.
- `.github/workflows/build.yml`: compilação OpenOrbis.
- `tools/generate_icon.py`: gera o ícone PNG necessário ao pacote.

## Compatibilidade

O pacote de teste usa OpenOrbis e precisa ser validado no console. A compilação bem-sucedida, por si só, não garante compatibilidade com todo firmware ou configuração de jailbreak.
