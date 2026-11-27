#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <sys/stat.h>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>

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
    int page = 0;       // Home, games, apps, tools
    int card = 0;
    int focusCards = 0;
    bool dialogOpen = false;
};

enum DownloadState {
    kDownloadIdle = 0,
    kDownloadRunning,
    kDownloadComplete,
    kDownloadFailed,
    kDownloadCancelled
};

std::atomic<int> gDownloadState(kDownloadIdle);
std::atomic<unsigned long long> gDownloadBytes(0);
std::atomic<unsigned long long> gDownloadTotal(0);
std::atomic<int> gDownloadError(0);
std::atomic<int> gDownloadApiError(0);
std::atomic<int> gDownloadNetErrno(-1);
std::atomic<int> gDownloadHttpStatus(0);
std::atomic<uint32_t> gDownloadSslError(0);
std::atomic<bool> gCancelDownload(false);
int gDownloadItemIndex = -1;

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
    if (scale <= 1) return 22;
    if (scale == 2) return 29;
    if (scale == 3) return 37;
    if (scale == 4) return 45;
    if (scale == 5) return 54;
    if (scale == 6) return 63;
    return 72;
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
    scene->DrawRoundedRectangle(x, y, w, h, radius, color);
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
    scene->DrawLine(x0, y0, x1, y1, color, thickness);
}

void drawLogo(Scene2D* scene, int x, int y, int size) {
    if (scene->DrawRgbaImage(x, y, size, size)) return;
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
    drawRoundedRect(scene, x + 2, y + 3, width - 2, 238, 22, {6, 9, 17});
    drawRoundedRect(scene, x, y, width, 238, 22, selected ? accents[index] : panelHi);
    drawRoundedRect(scene, x + 1, y + 1, width - 2, 236, 21, selected ? panelHi : panel);
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

void drawProductArtwork(Scene2D* scene, const CatalogEntry& item, int itemIndex,
    int x, int y, int w, int h) {
    const Color tops[5] = {{14, 91, 111}, {56, 45, 112}, {24, 104, 103}, {77, 43, 106}, {31, 103, 78}};
    const Color bottoms[5] = {{11, 23, 48}, {16, 23, 57}, {12, 37, 52}, {27, 18, 53}, {11, 42, 39}};
    const Color accents[5] = {{73, 229, 255}, {174, 103, 255}, {87, 236, 194}, {196, 133, 255}, {92, 245, 190}};
    const Color top = tops[itemIndex % 5], bottom = bottoms[itemIndex % 5];
    const Color accent = accents[itemIndex % 5];

    drawRoundedRect(scene, x, y, w, h, 22, {6, 9, 18});
    drawGradient(scene, x + 1, y + 1, w - 2, h - 2, top, bottom);
    drawLine(scene, x + w / 8, y + h - 4, x + w / 2, y + 4, {30, 55, 91}, 2);
    drawLine(scene, x + w / 3, y + h - 4, x + w * 3 / 4, y + 4, {31, 58, 98}, 2);
    drawLine(scene, x + w * 3 / 5, y + h - 4, x + w - 16, y + h / 5, {32, 59, 101}, 2);

    const int cx = x + w * 2 / 3;
    const int cy = y + h / 2 - (h > 300 ? 28 : 0);
    if (itemIndex == 0) {
        drawRoundedRect(scene, cx - 118, cy - 76, 236, 152, 20, {21, 39, 70});
        drawRoundedRect(scene, cx - 101, cy - 59, 202, 118, 14, {28, 57, 89});
        drawLine(scene, cx - 16, cy - 31, cx - 16, cy + 32, accent, 6);
        drawLine(scene, cx - 16, cy - 31, cx + 39, cy, accent, 6);
        drawLine(scene, cx + 39, cy, cx - 16, cy + 32, accent, 6);
    } else if (itemIndex == 1) {
        drawLine(scene, cx - 86, cy - 50, cx + 60, cy - 4, accent, 4);
        drawLine(scene, cx - 86, cy + 53, cx + 60, cy - 4, accent, 4);
        drawLine(scene, cx + 60, cy - 4, cx + 114, cy + 62, accent, 4);
        drawRoundedRect(scene, cx - 111, cy - 75, 50, 50, 25, {31, 57, 98});
        drawRoundedRect(scene, cx - 111, cy + 29, 50, 50, 25, {31, 57, 98});
        drawRoundedRect(scene, cx + 34, cy - 29, 54, 54, 27, {75, 59, 137});
        drawRoundedRect(scene, cx + 89, cy + 37, 50, 50, 25, {31, 57, 98});
    } else if (itemIndex == 2) {
        drawRoundedRect(scene, cx - 106, cy - 74, 92, 130, 16, {31, 62, 92});
        drawRoundedRect(scene, cx + 18, cy - 55, 108, 112, 18, {33, 73, 91});
        drawLine(scene, cx - 12, cy - 5, cx + 15, cy - 5, accent, 7);
        drawLine(scene, cx + 6, cy - 17, cx + 20, cy - 5, accent, 7);
        drawLine(scene, cx + 6, cy + 7, cx + 20, cy - 5, accent, 7);
        drawLine(scene, cx + 10, cy + 74, cx - 15, cy + 74, accent, 7);
    } else if (itemIndex == 3) {
        drawRoundedRect(scene, cx - 99, cy - 79, 198, 158, 28, {39, 36, 86});
        drawRoundedRect(scene, cx - 70, cy - 48, 140, 96, 18, {53, 49, 111});
        drawLine(scene, cx, cy - 58, cx, cy + 59, accent, 4);
        drawLine(scene, cx - 76, cy, cx + 76, cy, accent, 4);
        drawLine(scene, cx - 54, cy - 54, cx + 54, cy + 54, {116, 100, 204}, 3);
        drawLine(scene, cx - 54, cy + 54, cx + 54, cy - 54, {116, 100, 204}, 3);
    } else {
        const int tile = h > 300 ? 55 : 25, gap = h > 300 ? 14 : 7;
        const int startX = cx - tile - gap / 2, startY = cy - tile - gap / 2;
        for (int row = 0; row < 2; ++row) for (int column = 0; column < 2; ++column) {
            const bool active = (row == 0 && column == 1) || (row == 1 && column == 0);
            drawRoundedRect(scene, startX + column * (tile + gap), startY + row * (tile + gap),
                tile, tile, h > 300 ? 14 : 7, active ? accent : Color{28, 67, 67});
        }
    }

    const int logoSize = h > 300 ? 104 : 50;
    drawLogo(scene, x + w - logoSize - 22, y + 18, logoSize);
    if (h > 300) {
        drawText(scene, item.name, x + 26, y + h - 170, 4, white);
        const int gap = 12, tileW = (w - 76 - gap * 2) / 3, tileY = y + h - 104;
        for (int i = 0; i < 3; ++i) {
            const int tileX = x + 26 + i * (tileW + gap);
            drawRoundedRect(scene, tileX, tileY, tileW, 76, 12, {14, 24, 44});
            drawGradient(scene, tileX + 2, tileY + 2, tileW - 4, 72,
                blend(top, accent, i + 1, 5), bottom);
            drawRoundedRect(scene, tileX + 14, tileY + 14, tileW - 28, 46, 8,
                i == 1 ? Color{30, 63, 91} : Color{23, 43, 72});
            drawRoundedRect(scene, tileX + 25, tileY + 23, tileW / 3, 3, 1, accent);
            drawRoundedRect(scene, tileX + 25, tileY + 32, tileW / 2, 3, 1, {104, 137, 174});
            drawRoundedRect(scene, tileX + 25, tileY + 41, tileW / 4, 3, 1, {104, 137, 174});
        }
    }
}

void drawWrappedText(Scene2D* scene, const char* value, int x, int y, int maxWidth,
    int scale, Color color, int maxLines = 3) {
    std::istringstream words(value);
    std::string word, line;
    int lineNumber = 0;
    const int lineHeight = fontSizeForScale(scale) + 8;
    while (words >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && measureTextWidth(candidate.c_str(), scale) > maxWidth) {
            drawText(scene, line.c_str(), x, y + lineNumber * lineHeight, scale, color);
            if (++lineNumber >= maxLines) return;
            line = word;
        } else line = candidate;
    }
    if (!line.empty() && lineNumber < maxLines)
        drawText(scene, line.c_str(), x, y + lineNumber * lineHeight, scale, color);
}

void drawCatalogPage(Scene2D* scene, const StoreState& state) {
    const bool toolsPage = state.page == 3;
    const int count = catalogCountForPage(state.page);
    const int itemIndex = catalogIndexForPage(state.page, state.card);
    if (itemIndex < 0) return;
    const CatalogEntry& selected = kCatalogItems[itemIndex];
    const Color accent = toolsPage ? mint : violet;

    drawRoundedRect(scene, 64, 150, 1792, 408, 28, panelHi);
    drawProductArtwork(scene, selected, itemIndex, 76, 162, 920, 384);
    drawRoundedRect(scene, 1008, 162, 828, 384, 22, panel);
    drawRoundedRect(scene, 1040, 184, 220, 34, 17, {24, 48, 65});
    drawText(scene, toolsPage ? "FERRAMENTA HOMEBREW" : "APP HOMEBREW", 1056, 191, 1, accent);
    drawText(scene, selected.name, 1040, 230, 6, white);
    drawWrappedText(scene, selected.description, 1044, 300, 744, 2, muted, 2);

    drawText(scene, "DESENVOLVEDOR", 1044, 365, 1, {112, 133, 163});
    drawText(scene, selected.developer, 1044, 389, 2, white);
    drawText(scene, "VERSAO", 1304, 365, 1, {112, 133, 163});
    drawText(scene, selected.version, 1304, 389, 2, white);
    drawText(scene, "LICENCA", 1470, 365, 1, {112, 133, 163});
    drawText(scene, selected.license, 1470, 389, 1, accent);
    drawRoundedRect(scene, 1044, 428, 720, 1, 0, panelHi);
    drawText(scene, "PACOTE OFICIAL", 1044, 443, 1, {112, 133, 163});
    drawText(scene, selected.downloadSource, 1044, 468, 2, cyan);
    drawRoundedRect(scene, 1510, 450, 286, 62, 18, {24, 72, 93});
    drawRoundedRect(scene, 1510, 450, 4, 62, 2, cyan);
    drawText(scene, "X  VER DETALHES", 1552, 468, 2, white);

    drawText(scene, toolsPage ? "FERRAMENTAS PARA O SEU PS4" : "APPS HOMEBREW", 66, 585, 3, white);
    char countLabel[80];
    std::snprintf(countLabel, sizeof(countLabel), "%d PROJETOS  /  USE O D-PAD", count);
    drawText(scene, countLabel, 1320, 592, 1, muted);

    const int cardY = 624, cardHeight = 326, gap = 20;
    const int cardWidth = (1792 - gap * (count - 1)) / count;
    for (int row = 0; row < count; ++row) {
        const int index = catalogIndexForPage(state.page, row);
        if (index < 0) continue;
        const CatalogEntry& item = kCatalogItems[index];
        const int x = 64 + row * (cardWidth + gap);
        const bool isSelected = row == state.card;
        drawRoundedRect(scene, x, cardY, cardWidth, cardHeight, 20,
            isSelected ? accent : panelHi);
        drawRoundedRect(scene, x + 2, cardY + 2, cardWidth - 4, cardHeight - 4, 18,
            isSelected ? Color{19, 30, 51} : panel);
        drawProductArtwork(scene, item, index, x + 9, cardY + 9, cardWidth - 18, 142);
        drawText(scene, item.name, x + 18, cardY + 166, 2, white);
        drawText(scene, item.category, x + 20, cardY + 204, 1, muted);
        drawText(scene, item.version, x + 20, cardY + 244, 2, accent);
        drawText(scene, isSelected ? "X  ABRIR DETALHES" : "SELECIONAR",
            x + 20, cardY + 286, 1, isSelected ? cyan : Color{104, 125, 155});
        if (isSelected)
            drawRoundedRect(scene, x + 18, cardY + cardHeight - 6, cardWidth - 36, 3, 1, accent);
    }
}

void drawFooter(Scene2D* scene, const StoreState& state) {
    drawRoundedRect(scene, 64, 984, 1792, 2, 1, panelHi);
    drawText(scene, "L1 / R1  SECOES", 82, 1012, 2, muted);
    drawText(scene, state.page >= 2 ? "D-PAD  SELECIONAR" : "D-PAD  NAVEGAR", 378, 1012, 2, muted);
    drawText(scene, state.dialogOpen ? "X  BAIXAR PKG" : "X  DETALHES", 674, 1012, 2, muted);
    const int selected = state.page >= 2 ? catalogIndexForPage(state.page, state.card) : -1;
    const bool downloading = state.dialogOpen && selected >= 0 &&
        selected == gDownloadItemIndex && gDownloadState.load() == kDownloadRunning;
    drawText(scene, downloading ? "O  CANCELAR" : "O  VOLTAR", 946, 1012, 2, muted);
    drawText(scene, "AURORA STORE  /  01.08", 1540, 1012, 2, {91, 111, 142});
}

void drawDialog(Scene2D* scene, const StoreState& state) {
    scene->FrameBufferFill(bg);
    drawHeader(scene, state);
    const int itemIndex = catalogIndexForPage(state.page, state.card);
    if (itemIndex < 0) {
        drawRoundedRect(scene, 64, 150, 1792, 796, 28, panel);
        drawLogo(scene, 820, 250, 220);
        drawCenteredText(scene, "NENHUM JOGO HOMEBREW", 540, 5, white);
        drawCenteredText(scene, "O CATALOGO PRIORIZA APPS E FERRAMENTAS", 620, 2, muted);
        drawFooter(scene, state);
        return;
    }

    const CatalogEntry& item = kCatalogItems[itemIndex];
    const Color accent = selectedAccent(state.page);
    const int downloadState = gDownloadItemIndex == itemIndex
        ? gDownloadState.load() : kDownloadIdle;

    drawProductArtwork(scene, item, itemIndex, 64, 150, 1120, 470);
    drawRoundedRect(scene, 1204, 150, 652, 470, 26, panelHi);
    drawRoundedRect(scene, 1207, 153, 646, 464, 23, panel);
    drawRoundedRect(scene, 1244, 176, 226, 34, 17, {24, 48, 65});
    drawText(scene, "FONTE OFICIAL  /  PS4", 1260, 183, 1, mint);
    drawText(scene, item.name, 1244, 222, 4, white);
    drawWrappedText(scene, item.description, 1248, 278, 556, 2, muted, 2);
    drawText(scene, "DESENVOLVEDOR", 1248, 342, 1, {112, 133, 163});
    drawText(scene, item.developer, 1248, 366, 2, white);
    drawText(scene, "VERSAO", 1480, 342, 1, {112, 133, 163});
    drawText(scene, item.version, 1480, 366, 2, white);
    drawText(scene, "LICENCA", 1630, 342, 1, {112, 133, 163});
    drawText(scene, item.license, 1630, 366, 1, accent);
    drawRoundedRect(scene, 1248, 405, 556, 1, 0, panelHi);
    drawText(scene, "PACOTE DISPONIVEL", 1248, 418, 1, {112, 133, 163});
    drawText(scene, item.downloadSource, 1248, 442, 1, accent);
    drawText(scene, item.downloadFilename, 1436, 442, 1, white);

    char status[192];
    const unsigned long long bytes = gDownloadBytes.load(), total = gDownloadTotal.load();
    int percent = 0;
    if (downloadState == kDownloadRunning) {
        if (gCancelDownload.load()) {
            std::snprintf(status, sizeof(status), "CANCELANDO DOWNLOAD...");
        } else if (total > 0) {
            percent = static_cast<int>((bytes * 100) / total);
            if (percent > 99) percent = 99;
            std::snprintf(status, sizeof(status), "BAIXANDO  %d%%  -  %.1f / %.1f MB",
                percent, bytes / 1048576.0, total / 1048576.0);
        } else std::snprintf(status, sizeof(status), "BAIXANDO  %.1f MB", bytes / 1048576.0);
    } else if (downloadState == kDownloadComplete) {
        percent = 100;
        std::snprintf(status, sizeof(status), "DOWNLOAD CONCLUIDO");
    } else if (downloadState == kDownloadFailed) {
        const int httpStatus = gDownloadHttpStatus.load();
        const uint32_t sslError = gDownloadSslError.load();
        const int apiError = gDownloadApiError.load(), netErrno = gDownloadNetErrno.load();
        const int localError = gDownloadError.load();
        if (sslError != 0)
            std::snprintf(status, sizeof(status), "TLS 0x%08X  -  CONFIRA DATA E HORA DO CONSOLE",
                static_cast<unsigned int>(sslError));
        else if (localError == 21)
            std::snprintf(status, sizeof(status), "REDIRECT HTTP NAO ATIVADO  0x%08X",
                static_cast<unsigned int>(apiError));
        else if (httpStatus >= 300 && httpStatus < 400)
            std::snprintf(status, sizeof(status), "REDIRECT HTTP %d NAO CONCLUIDO", httpStatus);
        else if (httpStatus != 0)
            std::snprintf(status, sizeof(status), "HTTP %d  -  PACOTE INDISPONIVEL", httpStatus);
        else if (apiError != 0 && netErrno >= 0)
            std::snprintf(status, sizeof(status), "HTTP 0x%08X  /  REDE %d",
                static_cast<unsigned int>(apiError), netErrno);
        else if (apiError != 0)
            std::snprintf(status, sizeof(status), "HTTP 0x%08X",
                static_cast<unsigned int>(apiError));
        else
            std::snprintf(status, sizeof(status), "FALHA LOCAL %d", gDownloadError.load());
    } else if (downloadState == kDownloadCancelled) {
        std::snprintf(status, sizeof(status), "DOWNLOAD CANCELADO  -  X PARA TENTAR DE NOVO");
    } else std::snprintf(status, sizeof(status), "PRONTO PARA BAIXAR");

    drawText(scene, status, 1248, 474, 1,
        downloadState == kDownloadFailed ? amber :
        downloadState == kDownloadComplete ? mint : white);
    drawRoundedRect(scene, 1248, 500, 556, 10, 5, panelHi);
    if (percent > 0)
        drawRoundedRect(scene, 1248, 500, (556 * percent) / 100, 10, 5,
            downloadState == kDownloadFailed ? amber : mint);

    const char* action = "X  BAIXAR PACOTE";
    Color actionColor = {24, 72, 93};
    if (downloadState == kDownloadRunning) {
        action = "O  CANCELAR DOWNLOAD"; actionColor = {67, 46, 36};
    } else if (downloadState == kDownloadComplete) {
        action = "O  FECHAR DETALHES"; actionColor = {24, 53, 62};
    } else if (downloadState == kDownloadFailed || downloadState == kDownloadCancelled) {
        action = "X  TENTAR NOVAMENTE";
    }
    drawRoundedRect(scene, 1248, 524, 556, 60, 16, actionColor);
    drawRoundedRect(scene, 1248, 524, 4, 60, 2,
        downloadState == kDownloadRunning ? amber : cyan);
    drawText(scene, action, 1294, 542, 2,
        downloadState == kDownloadRunning ? amber : white);
    drawText(scene, "SALVO EM  /DATA/AURORA/DOWNLOADS", 1248, 592, 1, muted);

    drawRoundedRect(scene, 64, 642, 1120, 304, 24, panel);
    drawText(scene, "SOBRE O PROJETO", 92, 668, 4, white);
    drawWrappedText(scene, item.description, 96, 730, 1052, 2, muted, 3);
    drawText(scene, "REPOSITORIO OFICIAL", 96, 842, 1, {112, 133, 163});
    drawText(scene, item.repository, 96, 866, 1, cyan);
    drawText(scene, "VERSAO", 96, 900, 1, {112, 133, 163});
    drawText(scene, item.version, 96, 922, 2, white);
    drawText(scene, "LICENCA", 292, 900, 1, {112, 133, 163});
    drawText(scene, item.license, 292, 922, 2, accent);

    drawRoundedRect(scene, 1204, 642, 652, 304, 24, panel);
    drawText(scene, "MAIS NA AURORA", 1230, 668, 3, white);
    int shown = 0;
    for (int row = 0; row < catalogCountForPage(state.page) && shown < 2; ++row) {
        const int otherIndex = catalogIndexForPage(state.page, row);
        if (otherIndex < 0 || otherIndex == itemIndex) continue;
        const CatalogEntry& other = kCatalogItems[otherIndex];
        const int x = shown == 0 ? 1230 : 1540;
        drawRoundedRect(scene, x, 706, 296, 224, 16, panelHi);
        drawProductArtwork(scene, other, otherIndex, x + 8, 714, 280, 132);
        drawText(scene, other.name, x + 14, 858, 2, white);
        drawText(scene, other.category, x + 16, 896, 1, muted);
        ++shown;
    }
    drawFooter(scene, state);
}

void renderStore(Scene2D* scene, const StoreState& state) {
    scene->FrameBufferFill(bg);
    drawHeader(scene, state);
    if (state.page >= 2) {
        drawCatalogPage(scene, state);
    } else {
        drawHero(scene, state);
        if (state.page == 0) {
            drawText(scene, "EXPLORE POR CATEGORIA", 66, 616, 3, white);
            drawText(scene, "5 PROJETOS / FONTES OFICIAIS", 1330, 622, 2, muted);
            const int x[3] = {64, 674, 1284};
            for (int i = 0; i < 3; ++i)
                drawCategoryCard(scene, x[i], 674, 572, i, state.focusCards && state.card == i);
        } else {
            drawEmptyPage(scene, state);
        }
        drawFooter(scene, state);
    }
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

int32_t verifyDownloadCertificate(int32_t sslContext, uint32_t verifyError,
    void * const certificates[], int32_t certificateCount, void *userData) {
    (void)sslContext;
    (void)certificates;
    (void)certificateCount;
    (void)userData;
    gDownloadSslError.store(verifyError);
    // Keep certificate validation enabled; expose the reason when it fails.
    return verifyError == 0 ? 1 : 0;
}

void captureHttpError(int requestId, int apiError) {
    gDownloadApiError.store(apiError);
    int32_t netErrno = -1;
    if (requestId >= 0 && sceHttpGetLastErrno(requestId, &netErrno) == 0)
        gDownloadNetErrno.store(netErrno);
}

bool ensureDirectory(const char* path) {
    if (mkdir(path, 0777) == 0) return true;
    return errno == EEXIST;
}

void downloadPackageWorker(int itemIndex) {
    if (itemIndex < 0 || itemIndex >= kCatalogCount) {
        gDownloadError.store(1);
        gDownloadState.store(kDownloadFailed);
        return;
    }
    const CatalogEntry& item = kCatalogItems[itemIndex];
    char finalPath[256];
    char partialPath[272];
    std::snprintf(finalPath, sizeof(finalPath), "/data/aurora/downloads/%s", item.downloadFilename);
    std::snprintf(partialPath, sizeof(partialPath), "%s.part", finalPath);

    if (!ensureDirectory("/data/aurora") ||
        !ensureDirectory("/data/aurora/downloads")) {
        gDownloadError.store(2);
        gDownloadState.store(kDownloadFailed);
        return;
    }

    int netPool = -1;
    int sslContext = -1;
    int httpContext = -1;
    int templateId = -1;
    int connectionId = -1;
    int requestId = -1;
    int32_t sendResult = 0;
    FILE* output = nullptr;
    uint8_t* buffer = nullptr;
    bool success = false;
    bool cancelled = false;
    int errorCode = 0;

    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0 ||
        sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP) < 0 ||
        sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL) < 0) {
        errorCode = 3;
        goto download_cleanup;
    }
    if (sceNetInit() < 0) {
        errorCode = 4;
        goto download_cleanup;
    }
    netPool = sceNetPoolCreate("auroraDownloadNet", 512 * 1024, 0);
    if (netPool < 0) {
        errorCode = 5;
        goto download_cleanup;
    }
    sslContext = sceSslInit(SSL_POOLSIZE);
    if (sslContext < 0) {
        errorCode = 6;
        goto download_cleanup;
    }
    httpContext = sceHttpInit(netPool, sslContext, 512 * 1024);
    if (httpContext < 0) {
        errorCode = 7;
        goto download_cleanup;
    }
    templateId = sceHttpCreateTemplate(httpContext, "AuroraStore/01.08 PS4",
        ORBIS_HTTP_VERSION_1_1, 1);
    if (templateId < 0) {
        errorCode = 8;
        goto download_cleanup;
    }
    if (sceHttpsSetSslCallback(templateId, verifyDownloadCertificate, nullptr) < 0) {
        errorCode = 9;
        goto download_cleanup;
    }
    {
        typedef void (*SetAutoRedirectFunction)(int32_t, int32_t);
        SetAutoRedirectFunction setAutoRedirect =
            reinterpret_cast<SetAutoRedirectFunction>(&sceHttpSetAutoRedirect);
        setAutoRedirect(templateId, 1);
        int32_t redirectState = 0;
        const int32_t redirectQuery = sceHttpGetAutoRedirect(templateId, &redirectState);
        if (redirectQuery < 0 || redirectState != 1) {
            gDownloadApiError.store(redirectQuery < 0 ? redirectQuery : -1);
            errorCode = 21;
            goto download_cleanup;
        }
    }
    sceHttpSetConnectTimeOut(templateId, 30 * 1000 * 1000);
    sceHttpSetResolveTimeOut(templateId, 30 * 1000 * 1000);

    connectionId = sceHttpCreateConnectionWithURL(templateId, item.downloadUrl, 1);
    if (connectionId < 0) {
        gDownloadApiError.store(connectionId);
        errorCode = 10;
        goto download_cleanup;
    }
    requestId = sceHttpCreateRequestWithURL(connectionId, ORBIS_METHOD_GET,
        item.downloadUrl, 0);
    if (requestId < 0) {
        gDownloadApiError.store(requestId);
        errorCode = 11;
        goto download_cleanup;
    }
    sendResult = sceHttpSendRequest(requestId, nullptr, 0);
    if (sendResult < 0) {
        captureHttpError(requestId, sendResult);
        errorCode = 12;
        goto download_cleanup;
    }
    {
        int32_t statusCode = 0;
        const int32_t statusResult = sceHttpGetStatusCode(requestId, &statusCode);
        if (statusResult < 0) {
            captureHttpError(requestId, statusResult);
            errorCode = 13;
            goto download_cleanup;
        }
        gDownloadHttpStatus.store(statusCode);
        if (statusCode != 200) {
            errorCode = 1000 + statusCode;
            goto download_cleanup;
        }
        int contentLengthType = ORBIS_HTTP_CONTENTLEN_NOT_FOUND;
        size_t contentLength = 0;
        if (sceHttpGetResponseContentLength(requestId, &contentLengthType, &contentLength) == 0 &&
            contentLengthType == ORBIS_HTTP_CONTENTLEN_EXIST)
            gDownloadTotal.store(static_cast<unsigned long long>(contentLength));
        else
            gDownloadTotal.store(0);
    }

    output = std::fopen(partialPath, "wb");
    if (output == nullptr) {
        errorCode = 14;
        goto download_cleanup;
    }
    buffer = static_cast<uint8_t *>(std::malloc(64 * 1024));
    if (buffer == nullptr) {
        errorCode = 15;
        goto download_cleanup;
    }

    for (;;) {
        if (gCancelDownload.load()) {
            cancelled = true;
            break;
        }
        const int readCount = sceHttpReadData(requestId, buffer, 64 * 1024);
        if (readCount < 0) {
            captureHttpError(requestId, readCount);
            errorCode = 16;
            break;
        }
        if (readCount == 0) {
            success = true;
            break;
        }
        const size_t writeCount = std::fwrite(buffer, 1,
            static_cast<size_t>(readCount), output);
        if (writeCount != static_cast<size_t>(readCount)) {
            errorCode = 17;
            break;
        }
        gDownloadBytes.fetch_add(static_cast<unsigned long long>(readCount));
    }

    if (output != nullptr) {
        std::fclose(output);
        output = nullptr;
    }
    if (success && gDownloadTotal.load() > 0 &&
        gDownloadBytes.load() != gDownloadTotal.load()) {
        success = false;
        errorCode = 18;
    }
    if (success && std::rename(partialPath, finalPath) != 0) {
        success = false;
        errorCode = 19;
    }

download_cleanup:
    if (output != nullptr) std::fclose(output);
    if (buffer != nullptr) std::free(buffer);
    if (requestId >= 0) sceHttpDeleteRequest(requestId);
    if (connectionId >= 0) sceHttpDeleteConnection(connectionId);
    if (templateId >= 0) sceHttpDeleteTemplate(templateId);
    if (httpContext >= 0) sceHttpTerm(httpContext);
    if (sslContext >= 0) sceSslTerm();
    if (netPool >= 0) sceNetPoolDestroy(netPool);

    if (!success && partialPath[0] != '\0') std::remove(partialPath);
    if (cancelled) {
        gDownloadState.store(kDownloadCancelled);
    } else if (success) {
        gDownloadState.store(kDownloadComplete);
    } else {
        gDownloadError.store(errorCode);
        gDownloadState.store(kDownloadFailed);
    }
}

void beginDownload(int itemIndex) {
    if (itemIndex < 0 || itemIndex >= kCatalogCount) return;
    const CatalogEntry& item = kCatalogItems[itemIndex];
    if (item.downloadUrl[0] == '\0' || gDownloadState.load() == kDownloadRunning) return;
    gDownloadItemIndex = itemIndex;
    gDownloadBytes.store(0);
    gDownloadTotal.store(0);
    gDownloadError.store(0);
    gDownloadApiError.store(0);
    gDownloadNetErrno.store(-1);
    gDownloadHttpStatus.store(0);
    gDownloadSslError.store(0);
    gCancelDownload.store(false);
    gDownloadState.store(kDownloadRunning);
    try {
        std::thread(downloadPackageWorker, itemIndex).detach();
    } catch (...) {
        gDownloadError.store(20);
        gDownloadState.store(kDownloadFailed);
    }
}

}

int main(void) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    DEBUGLOG << "Aurora Store frontend 01.08";

    Scene2D* screen = new Scene2D(kWidth, kHeight, 4);
    if (!screen->Init(0x1000000, 2)) {
        DEBUGLOG << "Video output initialization failed";
        for (;;) sceKernelUsleep(1000000);
    }

    screen->SetActiveFrameBuffer(0);
    if (screen->LoadRgbaImage("/app0/assets/aurora-logo.rgba", 256, 256))
        DEBUGLOG << "Aurora logo loaded";
    else
        DEBUGLOG << "Aurora logo unavailable; using vector fallback";
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
    int lastRenderedDownloadState = gDownloadState.load();
    int progressUiTicks = 0;
    for (;;) {
        bool changed = false;
        if (padHandle >= 0 && scePadReadState(padHandle, &padData) == 0) {
            const uint32_t pressed = padData.buttons & ~previousButtons;
            previousButtons = padData.buttons;
            if (!state.dialogOpen && (pressed & kL1)) { cyclePage(state, -1); changed = true; }
            else if (!state.dialogOpen && (pressed & kR1)) { cyclePage(state, 1); changed = true; }
            else if (state.dialogOpen && (pressed & kCircle)) {
                const int itemIndex = catalogIndexForPage(state.page, state.card);
                if (itemIndex >= 0 && gDownloadItemIndex == itemIndex &&
                    gDownloadState.load() == kDownloadRunning) {
                    gCancelDownload.store(true);
                } else {
                    state.dialogOpen = false;
                }
                changed = true;
            }
            else if (state.dialogOpen && (pressed & kCross)) {
                const int itemIndex = catalogIndexForPage(state.page, state.card);
                if (itemIndex >= 0) beginDownload(itemIndex);
                changed = true;
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
                if (state.page == 0 && state.focusCards) {
                    state.card = (state.card + 2) % 3;
                    changed = true;
                } else if (state.page >= 2) {
                    const int count = catalogCountForPage(state.page);
                    if (count > 0) { state.card = (state.card + count - 1) % count; changed = true; }
                } else if (state.page == 1) {
                    cyclePage(state, -1);
                    changed = true;
                }
            }
            else if (!state.dialogOpen && (pressed & kRight)) {
                if (state.page == 0 && state.focusCards) {
                    state.card = (state.card + 1) % 3;
                    changed = true;
                } else if (state.page >= 2) {
                    const int count = catalogCountForPage(state.page);
                    if (count > 0) { state.card = (state.card + 1) % count; changed = true; }
                } else if (state.page == 1) {
                    cyclePage(state, 1);
                    changed = true;
                }
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
        const int downloadState = gDownloadState.load();
        if (state.dialogOpen && downloadState != lastRenderedDownloadState)
            changed = true;
        if (state.dialogOpen && downloadState == kDownloadRunning) {
            if (++progressUiTicks >= 6) {
                changed = true;
                progressUiTicks = 0;
            }
        } else {
            progressUiTicks = 0;
        }
        if (changed) showFrame(screen, state, frameId);
        lastRenderedDownloadState = downloadState;
        sceKernelUsleep(16000);
    }
}


