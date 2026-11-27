# Aurora Store PS4

Projeto experimental para PS4 homebrew.

## Versao 01.06 — interface suavizada e catalogo de fontes

A interface usa tipografia antialias com FreeType, cards arredondados e navegacao por listas. PySide6 nao faz parte deste pacote de PS4: o aplicativo e compilado em C++ para OpenOrbis. Por isso, a melhoria visual usa o renderer nativo e a biblioteca FreeType do toolchain.

O catalogo inicial foi montado a partir do [catalogo PS4 do PKG-Zone](https://pkg-zone.com/?console=ps4) e aponta para repositorios e releases oficiais dos desenvolvedores:

- Orbis Mozi — goph-R, MIT.
- ezRemote Client — cy33hc, GPL-2.0.
- PS MTP Explorer — nueex, GPL-3.0.
- Apollo Save Tool — bucanero, GPL-3.0.
- Itemzflow Game Manager — LightningMods, GPL-3.0.

O aplicativo mostra os repositorios, as versoes e as licencas no proprio PS4. Ele ainda nao baixa nem instala esses projetos; os botoes levam a uma tela de detalhes com os links oficiais, sem espelhar arquivos PKG.

## Controles

- **L1/R1:** alternar entre Inicio, Jogos, Apps e Ferramentas.
- **Setas:** navegar pelos cards e pela lista.
- **X:** abrir detalhes do projeto.
- **O:** voltar ou fechar os detalhes.

## Catalogo e recursos

- `catalogs/homebrew.json`: dados do catalogo e URLs dos repositorios/releases.
- `tools/generate_catalog.py`: gera a tabela C++ durante a compilacao.
- `assets/fonts/Gontserrat-Regular.ttf.b64`: fonte TrueType codificada em Base64 para manter o arquivo de origem compativel com o conector GitHub.
- `assets/fonts/OFL.txt`: licenca SIL Open Font License 1.1 da fonte.
- `tools/prepare_font.py`: prepara o arquivo TrueType que vai dentro do PKG.
- `src/main.cpp`: interface, listas, detalhes e navegacao do controle.
- `src/graphics.cpp`, `src/graphics.h`: renderer 2D e composicao antialias baseada no Scene2D do OpenOrbis; licenca em `third_party/OpenOrbis-PS4-Toolchain/LICENSE`.

O catalogo e estatico nesta versao. Atualizacao pela internet, downloads e instalacao no console ainda precisam de uma camada de rede e instalacao.

## Compilacao

GitHub Actions gera o pacote ao receber um push na branch principal ou por execucao manual. O workflow valida a versao do pacote, o icone e os recursos graficos incluidos.

## Compatibilidade e licencas

O projeto compila com OpenOrbis 0.5.4. A fonte Gontserrat foi obtida do exemplo oficial de FreeType do OpenOrbis e esta acompanhada da licenca OFL. Compilar o pacote nao confirma suporte em cada firmware ou configuracao de jailbreak.
