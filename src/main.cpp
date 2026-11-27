#include <cstdio>
#include <cstdint>
#include <math.h>
#include <sstream>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>

#include "graphics.h"
#include "catalog_data.h"
#include "log.h"

std::stringstream debugLogStream;

namespace {
constexpr int kWidth = 1920;
constexpr int kHeight = 1080;
constexpr int kPageCount = 4;
constexpr uint32_t kCross = ORBIS_PAD_BUTTON_CROSS;
constexpr uint32_t kCircle = ORBIS_PAD_BUTTON_CIRCLE;
constexpr uint32_t kLeft = ORBIS_PAD_BUTTON_LEFT;
constexpr uint32_t kRight = ORBIS_PAD_BUTTON_RIGHT;
constexpr uint32_t kUp = ORBIS_PAD_BUTTON_UP;
constexpr uint32_t kDown = ORBIS_PAD_BUTTON_DOWN;
constexpr uint32_t kL1 = ORBIS_PAD_BUTTON_L1;
constexpr uint32_t kR1 = ORBIS_PAD_BUTTON_R1;

FT_Face gFont = nullptr;

const uint8_t glyphs[36][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};

struct StoreState {
    int page = 0;       // Home, games, apps, favorites
    int card = 0;
    int focusCards = 0;
    bool dialogOpen = false;
};

const Color bg = {7, 10, 21};
const Color panel = {15, 22, 39};
const Color panelHi = {24, 34, 56};
const Color white = {241, 246, 255};
const Color muted = {137, 157, 188};
const Color cyan = {74, 229, 255};
const Color blue = {71, 133, 255};
const Color violet = {174, 103, 255};
const Color mint = {87, 236, 194};
const Color amber = {255, 190, 102};
Color selectedAccent(int page) { return page == 3 ? mint : violet; }

const uint8_t* glyphFor(char c) {
    static const uint8_t blank[7] = {};
    if (c >= 'A' && c <= 'Z') return glyphs[c - 'A'];
    if (c >= '0' && c <= '9') return glyphs[26 + c - '0'];
    return blank;
}

int fontSizeForScale(int scale) {
    if (scale <= 1) return 14;
    if (scale == 2) return 19;
    if (scale == 3) return 28;
    if (scale == 4) return 37;
    if (scale == 5) return 46;
    return 64;
}

void drawPixelText(Scene2D* scene, const char* text, int x, int y, int scale, Color color) {
    for (const char* p = text; *p; ++p, x += 6 * scale) {
        if (*p == ' ') continue;
        if (*p == '.') {
            scene->DrawRectangle(x + scale * 2, y + scale * 6, scale, scale, color);
            continue;
        }
        if (*p == ':') {
            scene->DrawRectangle(x + scale * 2, y + scale * 2, scale, scale, color);
            scene->DrawRectangle(x + scale * 2, y + scale * 5, scale, scale, color);
            continue;
        }
        if (*p == '/') {
            scene->DrawRectangle(x + scale * 3, y, scale, scale * 2, color);
            scene->DrawRectangle(x + scale * 2, y + scale * 2, scale, scale * 2, color);
            scene->DrawRectangle(x + scale, y + scale * 4, scale, scale * 2, color);
            continue;
        }
        if (*p == '-') {
            scene->DrawRectangle(x, y + scale * 3, scale * 5, scale, color);
            continue;
        }
        const char c = (*p >= 'a' && *p <= 'z') ? static_cast<char>(*p - 'a' + 'A') : *p;
        const uint8_t* rows = glyphFor(c);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row] & (1 << (4 - col)))
                    scene->DrawRectangle(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void drawText(Scene2D* scene, const char* text, int x, int y, int scale, Color color) {
    if (gFont != nullptr && FT_Set_Pixel_Sizes(gFont, 0, fontSizeForScale(scale)) == 0) {
        const int ascender = static_cast<int>((gFont->size->metrics.ascender + 63) >> 6);
        scene->DrawText(const_cast<char*>(text), gFont, x, y + ascender, bg, color);
        return;
    }
    drawPixelText(scene, text, x, y, scale, color);
}

int measureTextWidth(const char* text, int scale) {
    if (gFont != nullptr && FT_Set_Pixel_Sizes(gFont, 0, fontSizeForScale(scale)) == 0) {
        int width = 0;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
            if (FT_Load_Char(gFont, *p, FT_LOAD_DEFAULT) == 0)
                width += static_cast<int>(gFont->glyph->advance.x >> 6);
        }
        return width;
    }
    int width = 0;
    for (const char* p = text; *p; ++p) width += 6 * scale;
    return width > 0 ? width - scale : 0;
}

void drawCenteredText(Scene2D* scene, const char* text, int y, int scale, Color color) {
    drawText(scene, text, (kWidth - measureTextWidth(text, scale)) / 2, y, scale, color);
}
Color blend(Color a, Color b, int step, int total) {
    return {
        static_cast<uint8_t>((a.r * (total - step) + b.r * step) / total),
        static_cast<uint8_t>((a.g * (total - step) + b.g * step) / total),
        static_cast<uint8_t>((a.b * (total - step) + b.b * step) / total)
    };
}

int roundedInsetForRow(int row, int height, int radius) {
    if (radius <= 0 || (row >= radius && row < height - radius)) return 0;
    const double dy = row < radius
        ? row + 0.5 - radius
        : row - (height - radius) + 0.5 - radius;
    const double inside = radius * radius - dy * dy;
    if (inside <= 0.0) return radius;
    int inset = static_cast<int>(ceil(radius - sqrt(inside)));
    if (inset < 0) inset = 0;
    if (inset > radius) inset = radius;
    return inset;
}

void drawRoundedRect(Scene2D* scene, int x, int y, int w, int h, int radius, Color color) {
    if (w <= 0 || h <= 0) return;
    if (radius < 0) radius = 0;
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;
    for (int row = 0; row < h; ++row) {
        const int inset = roundedInsetForRow(row, h, radius);
        const int width = w - 2 * inset;
        if (width > 0) scene->DrawRectangle(x + inset, y + row, width, 1, color);
    }
}

void drawGradient(Scene2D* scene, int x, int y, int w, int h, Color top, Color bottom) {
    if (w <= 0 || h <= 0) return;
    const int radius = (h < 28) ? h / 2 : 20;
    for (int row = 0; row < h; ++row) {
        const int inset = roundedInsetForRow(row, h, radius);
        const int width = w - 2 * inset;
        if (width > 0) scene->DrawRectangle(x + inset, y + row, width, 1, blend(top, bottom, row, h - 1));
    }
}
void drawLine(Scene2D* scene, int x0, int y0, int x1, int y1, Color color, int thickness = 4) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    const int half = thickness / 2;
    for (;;) {
        scene->DrawRectangle(x0 - half, y0 - half, thickness, thickness, color);
        if (x0 == x1 && y0 == y1) break;
        const int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

void drawLogo(Scene2D* scene, int x, int y, int size) {
    const int cx = x + size / 2;
    drawLine(scene, cx, y, x + size, y + size, cyan, 5);
    drawLine(scene, x + size, y + size, x, y + size, violet, 5);
    drawLine(scene, x, y + size, cx, y, blue, 5);
    drawLine(scene, x + size / 4, y + size, cx, y + size / 2, blue, 4);
    drawLine(scene, cx, y + size / 2, x + size * 3 / 4, y + size, cyan, 4);
}

void drawHeader(Scene2D* scene, const StoreState& state) {
    scene->DrawRectangle(0, 0, kWidth, 124, bg);
    drawLogo(scene, 62, 34, 48);
    drawText(scene, "AURORA", 132, 35, 4, white);
    drawText(scene, "STORE", 134, 78, 2, muted);

    const char* tabs[kPageCount] = {"INICIO", "JOGOS", "APPS", "FERRAMENTAS"};
    const int xs[kPageCount] = {600, 786, 948, 1052};
    const int widths[kPageCount] = {150, 126, 118, 220};
    for (int i = 0; i < kPageCount; ++i) {
        if (i == state.page) {
            drawRoundedRect(scene, xs[i] - 16, 32, widths[i], 58, 16, panelHi);
            drawRoundedRect(scene, xs[i] - 16, 86, widths[i], 4, 2, cyan);
        }
        drawText(scene, tabs[i], xs[i], 52, 3, i == state.page ? white : muted);
    }
    scene->DrawRectangle(1470, 45, 2, 42, panelHi);
    drawRoundedRect(scene, 1500, 58, 10, 10, 5, amber);
    drawText(scene, "FONTES OFICIAIS", 1525, 55, 2, muted);
    drawRoundedRect(scene, 64, 122, 1792, 2, 1, panelHi);
}

void drawHeroArt(Scene2D* scene) {
    drawGradient(scene, 1198, 152, 658, 414, {20, 31, 66}, {22, 37, 62});
    // Atmospheric diagonal facets behind the Aurora crystal mark.
    drawLine(scene, 1215, 558, 1490, 154, {34, 58, 101}, 2);
    drawLine(scene, 1320, 565, 1598, 154, {34, 58, 101}, 2);
    drawLine(scene, 1428, 565, 1700, 154, {34, 58, 101}, 2);
    drawLine(scene, 1532, 565, 1822, 154, {34, 58, 101}, 2);
    drawLogo(scene, 1435, 215, 184);
    scene->DrawRectangle(1512, 484, 32, 4, cyan);
    scene->DrawRectangle(1550, 484, 32, 4, blue);
    scene->DrawRectangle(1588, 484, 32, 4, violet);
}

void drawHero(Scene2D* scene, const StoreState& state) {
    drawRoundedRect(scene, 64, 150, 1792, 416, 28, panel);
    drawHeroArt(scene);
    drawRoundedRect(scene, 110, 190, 374, 34, 17, {28, 45, 71});
    drawText(scene, "HOMEBREW PARA O SEU PS4", 126, 199, 2, cyan);

    const char* line1 = "SEU PROXIMO APP";
    const char* line2 = "COMECA AQUI";
    if (state.page == 1) { line1 = "JOGOS HOMEBREW"; line2 = "EM UM SO LUGAR"; }
    if (state.page == 2) { line1 = "APPS DE FONTE"; line2 = "ABERTA E OFICIAL"; }
    if (state.page == 3) { line1 = "FERRAMENTAS"; line2 = "PARA O SEU PS4"; }
    drawText(scene, line1, 112, 260, 7, white);
    drawText(scene, line2, 112, 325, 7, white);
    drawText(scene, "PROJETOS DA COMUNIDADE, EM UMA EXPERIENCIA", 116, 410, 3, muted);
    drawText(scene, "FEITA PARA NAVEGAR PELO CONTROLE.", 116, 442, 3, muted);

    const Color button = state.focusCards ? panelHi : Color{26, 68, 91};
    drawRoundedRect(scene, 112, 490, 330, 56, 16, button);
    drawRoundedRect(scene, 112, 490, 5, 56, 2, state.focusCards ? muted : cyan);
    drawText(scene, "X", 132, 508, 3, cyan);
    drawText(scene, state.page == 0 ? "EXPLORAR CATEGORIAS" : "ABRIR CATALOGO", 164, 508, 2, white);
}

void drawCategoryCard(Scene2D* scene, int x, int y, int width, int index, int selected) {
    const Color accents[3] = {cyan, violet, mint};
    const Color deep[3] = {{20, 42, 68}, {35, 31, 68}, {20, 52, 52}};
    const char* title[3] = {"JOGOS", "APLICATIVOS", "FERRAMENTAS"};
    const char* subtitle[3] = {"JOGOS HOMEBREW", "APPS COM FONTE ABERTA", "UTILIDADES PARA O PS4"};
    const Color accent = accents[index];
    drawRoundedRect(scene, x + 3, y + 7, width, 238, 24, {4, 6, 13});
    drawRoundedRect(scene, x, y, width, 238, 24, selected ? accents[index] : panelHi);
    drawRoundedRect(scene, x + 2, y + 2, width - 4, 234, 22, selected ? panelHi : panel);
    drawRoundedRect(scene, x + 20, y, width - 40, 3, 2, selected ? accent : panelHi);
    drawGradient(scene, x + width - 192, y + 18, 174, 126, deep[index], panel);

    // Distinct abstract category marks: a play tile, app window and tool orbit.
    if (index == 0) {
        drawRoundedRect(scene, x + width - 146, y + 50, 72, 58, 14, {31, 55, 87});
        drawLine(scene, x + width - 119, y + 62, x + width - 119, y + 96, accent, 4);
        drawLine(scene, x + width - 119, y + 62, x + width - 91, y + 79, accent, 4);
        drawLine(scene, x + width - 91, y + 79, x + width - 119, y + 96, accent, 4);
    } else if (index == 1) {
        drawRoundedRect(scene, x + width - 145, y + 48, 78, 61, 12, {31, 50, 79});
        drawRoundedRect(scene, x + width - 145, y + 48, 78, 9, 5, accent);
        scene->DrawRectangle(x + width - 132, y + 70, 18, 17, {73, 116, 161});
        scene->DrawRectangle(x + width - 107, y + 72, 28, 5, muted);
        scene->DrawRectangle(x + width - 107, y + 82, 24, 5, muted);
    } else {
        drawLine(scene, x + width - 112, y + 42, x + width - 112, y + 112, accent, 4);
        drawLine(scene, x + width - 146, y + 77, x + width - 78, y + 77, accent, 4);
        drawLine(scene, x + width - 137, y + 52, x + width - 87, y + 102, accent, 3);
        drawLine(scene, x + width - 137, y + 102, x + width - 87, y + 52, accent, 3);
    }

    drawText(scene, index == 0 ? "01 / CATEGORIA" : index == 1 ? "02 / CATEGORIA" : "03 / CATEGORIA", x + 24, y + 26, 2, accent);
    drawText(scene, title[index], x + 24, y + 112, 5, white);
    drawText(scene, subtitle[index], x + 24, y + 170, 2, muted);
    drawText(scene, index == 0 ? "0 JOGOS" : index == 1 ? "5 PROJETOS" : "4 FERRAMENTAS", x + 24, y + 206, 2, accent);
    if (selected) {
        drawRoundedRect(scene, x + 20, y + 234, width - 40, 4, 2, accent);
        drawRoundedRect(scene, x + width - 40, y + 202, 20, 20, 6, accent);
    }
}

void drawEmptyPage(Scene2D* scene, const StoreState& state) {
    drawRoundedRect(scene, 64, 615, 1792, 316, 26, panelHi);
    drawRoundedRect(scene, 68, 619, 1784, 308, 22, panel);
    drawRoundedRect(scene, 138, 676, 204, 204, 24, {22, 34, 56});
    drawRoundedRect(scene, 177, 716, 126, 126, 20, {29, 48, 75});
    drawLogo(scene, 213, 748, 54);
    drawText(scene, "NENHUM JOGO HOMEBREW", 392, 704, 5, white);
    drawText(scene, "O CATALOGO ATUAL PRIORIZA APLICATIVOS E FERRAMENTAS", 396, 770, 2, muted);
    drawText(scene, "DE FONTE ABERTA DOS PROPRIOS DESENVOLVEDORES.", 396, 800, 2, muted);
    drawRoundedRect(scene, 392, 842, 270, 48, 16, {24, 47, 67});
    drawText(scene, "JOGOS  /  0 PROJETOS", 414, 854, 2, cyan);
    (void)state;
}

int catalogCountForPage(int page) {
    if (page == 2) return kCatalogCount;
    if (page == 3) {
        int count = 0;
        for (int i = 0; i < kCatalogCount; ++i) if (kCatalogItems[i].isTool) ++count;
        return count;
    }
    return 0;
}

int catalogIndexForPage(int page, int visibleIndex) {
    if (visibleIndex < 0) return -1;
    int index = 0;
    for (int i = 0; i < kCatalogCount; ++i) {
        if (page == 3 && !kCatalogItems[i].isTool) continue;
        if (index++ == visibleIndex) return i;
    }
    return -1;
}

void drawCatalogPage(Scene2D* scene, const StoreState& state) {
    const bool toolsPage = state.page == 3;
    const int count = catalogCountForPage(state.page);
    const Color accent = toolsPage ? mint : violet;
    drawText(scene, toolsPage ? "FERRAMENTAS E UTILITARIOS" : "APPS HOMEBREW", 66, 616, 3, white);
    drawText(scene, toolsPage ? "4 PROJETOS / GITHUB" : "5 PROJETOS / GITHUB", 1376, 622, 2, muted);

    for (int row = 0; row < count; ++row) {
        const int itemIndex = catalogIndexForPage(state.page, row);
        if (itemIndex < 0) continue;
        const CatalogEntry& item = kCatalogItems[itemIndex];
        const int x = 64;
        const int y = 654 + row * 58;
        const bool selected = row == state.card;
        drawRoundedRect(scene, x + 2, y + 4, 1024, 52, 14, {4, 6, 13});
        drawRoundedRect(scene, x, y, 1024, 52, 14, selected ? panelHi : panel);
        if (selected) drawRoundedRect(scene, x, y + 7, 4, 38, 2, accent);
        drawText(scene, item.name, x + 20, y + 3, 3, white);
        drawText(scene, item.description, x + 20, y + 31, 2, muted);
        drawText(scene, item.version, x + 884, y + 5, 2, accent);
        drawText(scene, item.isTool ? "UTILITARIO" : "MIDIA", x + 884, y + 29, 1, muted);
    }

    const int itemIndex = catalogIndexForPage(state.page, state.card);
    if (itemIndex < 0) return;
    const CatalogEntry& selected = kCatalogItems[itemIndex];
    drawRoundedRect(scene, 1119, 659, 740, 298, 24, {4, 6, 13});
    drawRoundedRect(scene, 1116, 654, 740, 298, 24, panelHi);
    drawRoundedRect(scene, 1119, 657, 734, 292, 21, panel);
    drawRoundedRect(scene, 1144, 676, 190, 30, 15, {24, 48, 65});
    drawText(scene, selected.isTool ? "FERRAMENTA" : "APLICATIVO", 1158, 682, 1, accent);
    drawText(scene, selected.name, 1144, 724, 4, white);
    drawText(scene, selected.description, 1144, 779, 2, muted);
    drawText(scene, "DESENVOLVEDOR", 1144, 824, 1, {112, 133, 163});
    drawText(scene, selected.developer, 1144, 845, 2, white);
    drawText(scene, "VERSAO", 1378, 824, 1, {112, 133, 163});
    drawText(scene, selected.version, 1378, 845, 2, white);
    drawRoundedRect(scene, 1144, 884, 680, 1, 0, panelHi);
    drawText(scene, selected.repository, 1144, 905, 1, cyan);
}


void drawFooter(Scene2D* scene) {
    drawRoundedRect(scene, 64, 984, 1792, 2, 1, panelHi);
    drawText(scene, "L1 / R1  SECOES", 82, 1012, 2, muted);
    drawText(scene, "D-PAD  NAVEGAR", 378, 1012, 2, muted);
    drawText(scene, "X  DETALHES", 674, 1012, 2, muted);
    drawText(scene, "O  VOLTAR", 946, 1012, 2, muted);
    drawText(scene, "AURORA STORE  /  01.06", 1540, 1012, 2, {91, 111, 142});
}

void drawDialog(Scene2D* scene, const StoreState& state) {
    scene->DrawRectangle(0, 0, kWidth, kHeight, {4, 7, 16});
    drawRoundedRect(scene, 326, 238, 1268, 604, 30, panelHi);
    drawRoundedRect(scene, 331, 243, 1258, 594, 26, panel);
    drawRoundedRect(scene, 370, 278, 224, 34, 17, {24, 48, 65});

    const int itemIndex = catalogIndexForPage(state.page, state.card);
    if (itemIndex >= 0) {
        const CatalogEntry& item = kCatalogItems[itemIndex];
        drawText(scene, "FONTE OFICIAL / GITHUB", 386, 286, 1, mint);
        drawText(scene, item.name, 386, 348, 5, white);
        drawText(scene, item.description, 390, 418, 2, muted);
        drawText(scene, "DESENVOLVEDOR", 390, 476, 1, {112, 133, 163});
        drawText(scene, item.developer, 390, 500, 3, white);
        drawText(scene, "VERSAO  /  LICENCA", 760, 476, 1, {112, 133, 163});
        drawText(scene, item.version, 760, 500, 3, white);
        drawText(scene, item.license, 920, 500, 2, selectedAccent(state.page));
        drawText(scene, "REPOSITORIO", 390, 566, 1, {112, 133, 163});
        drawText(scene, item.repository, 390, 590, 2, cyan);
        drawText(scene, "RELEASE OFICIAL", 390, 642, 1, {112, 133, 163});
        drawText(scene, item.releases, 390, 666, 1, muted);
        drawRoundedRect(scene, 730, 724, 460, 58, 18, {24, 53, 72});
        drawCenteredText(scene, "O  FECHAR E VOLTAR", 740, 2, cyan);
    } else {
        drawCenteredText(scene, "NENHUM JOGO NO CATALOGO", 430, 5, white);
        drawCenteredText(scene, "ESTAMOS PRIORIZANDO APLICATIVOS HOMEBREW", 520, 2, muted);
        drawRoundedRect(scene, 730, 680, 460, 58, 18, {24, 53, 72});
        drawCenteredText(scene, "O  FECHAR E VOLTAR", 696, 2, cyan);
    }
}


void renderStore(Scene2D* scene, const StoreState& state) {
    scene->FrameBufferFill(bg);
    drawHeader(scene, state);
    drawHero(scene, state);
    if (state.page == 0) {
        drawText(scene, "EXPLORE POR CATEGORIA", 66, 616, 3, white);
        drawText(scene, "5 PROJETOS / FONTES OFICIAIS", 1330, 622, 2, muted);
        const int x[3] = {64, 674, 1284};
        for (int i = 0; i < 3; ++i)
            drawCategoryCard(scene, x[i], 674, 572, i, state.focusCards && state.card == i);
    } else if (state.page == 1) {
        drawEmptyPage(scene, state);
    } else {
        drawCatalogPage(scene, state);
    }
    drawFooter(scene);
    if (state.dialogOpen) drawDialog(scene, state);
}

void showFrame(Scene2D* scene, const StoreState& state, int& frameId) {
    renderStore(scene, state);
    scene->SubmitFlip(frameId);
    scene->FrameWait(frameId);
    scene->FrameBufferSwap();
    ++frameId;
}

void cyclePage(StoreState& state, int direction) {
    state.page = (state.page + direction + kPageCount) % kPageCount;
    state.card = 0;
    state.focusCards = 0;
}
}

int main(void) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    DEBUGLOG << "Aurora Store frontend 01.06";

    Scene2D* screen = new Scene2D(kWidth, kHeight, 4);
    if (!screen->Init(0x1000000, 2)) {
        DEBUGLOG << "Video output initialization failed";
        for (;;) sceKernelUsleep(1000000);
    }

    screen->SetActiveFrameBuffer(0);
    FT_Face fontFace = nullptr;
    if (screen->InitFont(&fontFace, "/app0/assets/fonts/Gontserrat-Regular.ttf", 24)) {
        gFont = fontFace;
        DEBUGLOG << "Antialiased font loaded";
    } else {
        DEBUGLOG << "Font unavailable; using safe fallback renderer";
    }
    StoreState state;
    int frameId = 0;
    showFrame(screen, state, frameId);

    int padHandle = -1;
    if (scePadInit() == 0) {
        OrbisUserServiceInitializeParams params = {};
        params.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
        sceUserServiceInitialize(&params);
        int32_t userId = -1;
        if (sceUserServiceGetInitialUser(&userId) == 0)
            padHandle = scePadOpen(userId, 0, 0, nullptr);
    }
    if (padHandle < 0) DEBUGLOG << "Controller unavailable; frontend remains visible";
    else DEBUGLOG << "DualShock controller ready";

    OrbisPadData padData = {};
    uint32_t previousButtons = 0;
    for (;;) {
        bool changed = false;
        if (padHandle >= 0 && scePadReadState(padHandle, &padData) == 0) {
            const uint32_t pressed = padData.buttons & ~previousButtons;
            previousButtons = padData.buttons;
            if (!state.dialogOpen && (pressed & kL1)) { cyclePage(state, -1); changed = true; }
            else if (!state.dialogOpen && (pressed & kR1)) { cyclePage(state, 1); changed = true; }
            else if (state.dialogOpen && (pressed & (kCircle | kCross))) {
                state.dialogOpen = false; changed = true;
            }
            else if (!state.dialogOpen && (pressed & kCircle)) {
                if (state.page != 0) { state.page = 0; state.focusCards = 0; state.card = 0; changed = true; }
            }
            else if (!state.dialogOpen && (pressed & kUp)) {
                if (state.page == 0 && state.focusCards) { state.focusCards = 0; changed = true; }
                else if (state.page >= 2) {
                    const int count = catalogCountForPage(state.page);
                    if (count > 0) { state.card = (state.card + count - 1) % count; changed = true; }
                }
            }
            else if (!state.dialogOpen && (pressed & kDown)) {
                if (state.page == 0 && !state.focusCards) { state.focusCards = 1; changed = true; }
                else if (state.page >= 2) {
                    const int count = catalogCountForPage(state.page);
                    if (count > 0) { state.card = (state.card + 1) % count; changed = true; }
                }
            }
            else if (!state.dialogOpen && (pressed & kLeft)) {
                if (state.page == 0 && state.focusCards) { state.card = (state.card + 2) % 3; changed = true; }
                else if (state.page != 0) { cyclePage(state, -1); changed = true; }
            }
            else if (!state.dialogOpen && (pressed & kRight)) {
                if (state.page == 0 && state.focusCards) { state.card = (state.card + 1) % 3; changed = true; }
                else if (state.page != 0) { cyclePage(state, 1); changed = true; }
            }
            else if (!state.dialogOpen && (pressed & kCross)) {
                if (state.page == 0 && state.focusCards) {
                    state.page = state.card + 1;
                    state.focusCards = 0;
                } else if (state.page == 0) {
                    state.focusCards = 1;
                } else if (state.page == 1 || catalogCountForPage(state.page) > 0) {
                    state.dialogOpen = true;
                }
                changed = true;
            }
        }
        if (changed) showFrame(screen, state, frameId);
        sceKernelUsleep(16000);
    }
}


