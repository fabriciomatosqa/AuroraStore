# Aurora Store PS4

Projeto homebrew de catalogo e download para PS4.

## Versao 01.08

A interface usa a fonte Gontserrat com FreeType, capas ilustradas para cada projeto, detalhes em tela cheia e cartoes de recomendacao. As formas e linhas vetoriais recebem antialias, com hierarquia de texto maior para leitura na TV.

O catalogo foi montado a partir do [PKG-Zone para PS4](https://pkg-zone.com/?console=ps4) e aponta para repositorios oficiais dos projetos:

- Orbis Mozi — goph-R, MIT.
- ezRemote Client — cy33hc, GPL-2.0.
- PS MTP Explorer — nueex, GPL-3.0.
- Apollo Save Tool — bucanero, GPL-3.0.
- Itemzflow Game Manager — LightningMods, GPL-3.0.

## Downloads no PS4

Abra Apps ou Ferramentas, escolha um projeto com as setas e pressione **X** para abrir os detalhes. Pressione **X** novamente para baixar o PKG; a tela mostra o progresso, permite cancelar com **O** e confirma quando o arquivo foi salvo. Se falhar, a tela mostra o codigo HTTP, TLS ou de rede para identificar a causa. Os pacotes dos quatro primeiros projetos vêm dos releases do GitHub; Itemzflow vem do endpoint de download do PKG-Zone.

Os arquivos completos ficam em `/data/aurora/downloads`. O app baixa e valida o arquivo, mas nao inicia a instalacao; use o instalador de pacotes do console para instalar o PKG baixado.

## Controles

- **L1/R1:** alternar entre Inicio, Jogos, Apps e Ferramentas.
- **Setas:** navegar pelos cartoes de apps e ferramentas.
- **X:** abrir detalhes ou iniciar/tentar novamente o download.
- **O:** voltar, fechar detalhes ou cancelar um download em andamento.

## Catalogo e recursos

- `catalogs/homebrew.json`: versoes, repositorios e URLs dos pacotes.
- `tools/generate_catalog.py`: gera a tabela C++ durante a compilacao.
- `assets/aurora-icon.png.b64`: logo original usado no icone do pacote e convertido em RGBA para a interface.
- `assets/fonts/Gontserrat-Regular.ttf.b64`: fonte TrueType codificada em Base64.
- `assets/fonts/OFL.txt`: licenca SIL Open Font License 1.1 da fonte.
- `tools/prepare_font.py` e `tools/generate_icon.py`: preparam fonte, icone e logo incluidos no PKG.
- `src/main.cpp`: interface, navegacao, rede e fluxo de download.
- `src/graphics.cpp`, `src/graphics.h`: renderer 2D e composicao de imagens.

O catalogo e estatico nesta versao. Cada URL de pacote aponta para uma release oficial do projeto ou para o endpoint PS4 do PKG-Zone.

## Compilacao

GitHub Actions gera o pacote quando recebe um push na branch principal ou por execucao manual. O workflow valida a versao, o icone e o logo RGBA incluidos no PKG.

## Compatibilidade e licencas

O projeto compila com OpenOrbis 0.5.4. A fonte Gontserrat esta acompanhada da licenca OFL. A disponibilidade de rede e o suporte a cada pacote dependem do firmware/configuracao do PS4 e do servidor de origem.
