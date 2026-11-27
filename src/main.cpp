#include <cstdio>
#include <cstdint>
#include <sstream>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>

#include "graphics.h"
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

const uint8_t* glyphFor(char c) {
    static const uint8_t blank[7] = {};
    if (c >= 'A' && c <= 'Z') return glyphs[c - 'A'];
    if (c >= '0' && c <= '9') return glyphs[26 + c - '0'];
    return blank;
}

void drawText(Scene2D* scene, const char* text, int x, int y, int scale, Color color) {
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
        const uint8_t* rows = glyphFor(*p);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row] & (1 << (4 - col)))
                    scene->DrawRectangle(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void drawCenteredText(Scene2D* scene, const char* text, int y, int scale, Color color) {
    int width = 0;
    for (const char* p = text; *p; ++p) width += 6 * scale;
    if (width > 0) width -= scale;
    drawText(scene, text, (kWidth - width) / 2, y, scale, color);
}

Color blend(Color a, Color b, int step, int total) {
    return {
        static_cast<uint8_t>((a.r * (total - step) + b.r * step) / total),
        static_cast<uint8_t>((a.g * (total - step) + b.g * step) / total),
        static_cast<uint8_t>((a.b * (total - step) + b.b * step) / total)
    };
}

void drawGradient(Scene2D* scene, int x, int y, int w, int h, Color top, Color bottom) {
    const int bands = 96;
    for (int i = 0; i < bands; ++i) {
        const int y0 = y + (h * i) / bands;
        const int y1 = y + (h * (i + 1)) / bands;
        scene->DrawRectangle(x, y0, w, y1 - y0 + 1, blend(top, bottom, i, bands - 1));
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

    const char* tabs[kPageCount] = {"INICIO", "JOGOS", "APPS", "FAVORITOS"};
    const int xs[kPageCount] = {600, 786, 948, 1098};
    const int widths[kPageCount] = {150, 126, 118, 188};
    for (int i = 0; i < kPageCount; ++i) {
        if (i == state.page) {
            scene->DrawRectangle(xs[i] - 16, 32, widths[i], 58, panelHi);
            scene->DrawRectangle(xs[i] - 16, 86, widths[i], 3, cyan);
        }
        drawText(scene, tabs[i], xs[i], 52, 3, i == state.page ? white : muted);
    }
    scene->DrawRectangle(1470, 45, 2, 42, panelHi);
    scene->DrawRectangle(1500, 58, 10, 10, amber);
    drawText(scene, "CATALOGO OFFLINE", 1525, 55, 2, muted);
    scene->DrawRectangle(64, 122, 1792, 2, panelHi);
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
    scene->DrawRectangle(64, 150, 1792, 416, panel);
    drawHeroArt(scene);
    scene->DrawRectangle(110, 190, 330, 30, {28, 45, 71});
    drawText(scene, "HOMEBREW PARA O SEU PS4", 126, 199, 2, cyan);

    const char* line1 = "SEU PROXIMO APP";
    const char* line2 = "COMECA AQUI";
    if (state.page == 1) { line1 = "JOGOS HOMEBREW"; line2 = "EM UM SO LUGAR"; }
    if (state.page == 2) { line1 = "APLICATIVOS"; line2 = "PARA IR MAIS LONGE"; }
    if (state.page == 3) { line1 = "SUA LISTA"; line2 = "DO SEU JEITO"; }
    drawText(scene, line1, 112, 260, 7, white);
    drawText(scene, line2, 112, 325, 7, white);
    drawText(scene, "PROJETOS DA COMUNIDADE, EM UMA EXPERIENCIA", 116, 410, 3, muted);
    drawText(scene, "FEITA PARA NAVEGAR PELO CONTROLE.", 116, 442, 3, muted);

    const Color button = state.focusCards ? panelHi : {26, 68, 91};
    scene->DrawRectangle(112, 490, 310, 56, button);
    scene->DrawRectangle(112, 490, 4, 56, state.focusCards ? muted : cyan);
    drawText(scene, "X", 132, 508, 3, cyan);
    drawText(scene, state.page == 0 ? "EXPLORAR CATEGORIAS" : "VER DISPONIBILIDADE", 164, 508, 2, white);
}

void drawCategoryCard(Scene2D* scene, int x, int y, int width, int index, int selected) {
    const Color accents[3] = {cyan, violet, mint};
    const Color deep[3] = {{20, 42, 68}, {35, 31, 68}, {20, 52, 52}};
    const char* title[3] = {"JOGOS", "APLICATIVOS", "FERRAMENTAS"};
    const char* subtitle[3] = {"PORTS E EXPERIENCIAS", "NOVAS UTILIDADES", "PROJETOS DA COMUNIDADE"};
    const Color accent = accents[index];
    scene->DrawRectangle(x, y, width, 238, selected ? panelHi : panel);
    scene->DrawRectangle(x, y, width, 2, selected ? accent : panelHi);
    drawGradient(scene, x + width - 192, y + 18, 174, 126, deep[index], panel);

    // Distinct abstract category marks: a play tile, app window and tool orbit.
    if (index == 0) {
        scene->DrawRectangle(x + width - 146, y + 50, 72, 58, {31, 55, 87});
        drawLine(scene, x + width - 119, y + 62, x + width - 119, y + 96, accent, 4);
        drawLine(scene, x + width - 119, y + 62, x + width - 91, y + 79, accent, 4);
        drawLine(scene, x + width - 91, y + 79, x + width - 119, y + 96, accent, 4);
    } else if (index == 1) {
        scene->DrawRectangle(x + width - 145, y + 48, 78, 61, {31, 50, 79});
        scene->DrawRectangle(x + width - 145, y + 48, 78, 9, accent);
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
    drawText(scene, "0 ITENS", x + 24, y + 206, 2, accent);
    if (selected) {
        scene->DrawRectangle(x, y + 234, width, 4, accent);
        scene->DrawRectangle(x + width - 40, y + 202, 20, 20, accent);
    }
}

void drawEmptyPage(Scene2D* scene, const StoreState& state) {
    const char* headline = state.page == 1 ? "NENHUM JOGO NO CATALOGO" :
                           state.page == 2 ? "NENHUM APP NO CATALOGO" : "SUA LISTA ESTA VAZIA";
    const char* description = state.page == 3 ? "SALVE SEUS FAVORITOS PARA ENCONTRAR DEPOIS" :
                             "NOVOS PROJETOS VAO APARECER AQUI QUANDO O CATALOGO FOR CONECTADO";
    const Color accent = state.page == 1 ? cyan : state.page == 2 ? violet : mint;
    scene->DrawRectangle(64, 615, 1792, 316, panel);
    scene->DrawRectangle(64, 615, 5, 316, accent);
    scene->DrawRectangle(138, 676, 204, 204, {22, 34, 56});
    scene->DrawRectangle(177, 716, 126, 126, {29, 48, 75});
    drawLogo(scene, 213, 748, 54);
    drawText(scene, headline, 392, 704, 5, white);
    drawText(scene, description, 396, 770, 2, muted);
    scene->DrawRectangle(392, 820, 276, 52, {24, 47, 67});
    drawText(scene, "CATALOGO OFFLINE", 414, 838, 2, accent);
    drawText(scene, "0 RESULTADOS", 1536, 856, 2, muted);
}

void drawFooter(Scene2D* scene) {
    scene->DrawRectangle(64, 984, 1792, 2, panelHi);
    drawText(scene, "L1 / R1  SECOES", 82, 1012, 2, muted);
    drawText(scene, "D-PAD  NAVEGAR", 378, 1012, 2, muted);
    drawText(scene, "X  SELECIONAR", 674, 1012, 2, muted);
    drawText(scene, "O  VOLTAR", 946, 1012, 2, muted);
    drawText(scene, "AURORA STORE  /  01.04", 1540, 1012, 2, {91, 111, 142});
}

void drawDialog(Scene2D* scene) {
    scene->DrawRectangle(0, 0, kWidth, kHeight, {4, 7, 16});
    scene->DrawRectangle(330, 245, 1260, 590, panel);
    scene->DrawRectangle(330, 245, 1260, 5, cyan);
    drawLogo(scene, 890, 305, 140);
    drawCenteredText(scene, "CATALOGO EM PREPARACAO", 505, 5, white);
    drawCenteredText(scene, "O VISUAL DA LOJA ESTA PRONTO, MAS A LISTA DE ITENS", 590, 2, muted);
    drawCenteredText(scene, "AINDA PRECISA SER LIGADA A UM CATALOGO DE DOWNLOADS.", 620, 2, muted);
    scene->DrawRectangle(732, 710, 456, 58, {25, 56, 77});
    drawCenteredText(scene, "O  FECHAR E VOLTAR", 730, 2, cyan);
}

void renderStore(Scene2D* scene, const StoreState& state) {
    scene->FrameBufferFill(bg);
    drawHeader(scene, state);
    drawHero(scene, state);
    if (state.page == 0) {
        drawText(scene, "EXPLORE POR CATEGORIA", 66, 616, 3, white);
        drawText(scene, "3 CATEGORIAS  /  0 ITENS PUBLICADOS", 1330, 622, 2, muted);
        const int x[3] = {64, 674, 1284};
        for (int i = 0; i < 3; ++i) drawCategoryCard(scene, x[i], 674, 572, i, state.focusCards && state.card == i);
    } else {
        drawEmptyPage(scene, state);
    }
    drawFooter(scene);
    if (state.dialogOpen) drawDialog(scene);
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
    DEBUGLOG << "Aurora Store frontend 01.04";

    Scene2D* screen = new Scene2D(kWidth, kHeight, 4);
    if (!screen->Init(0x1000000, 2)) {
        DEBUGLOG << "Video output initialization failed";
        for (;;) sceKernelUsleep(1000000);
    }

    screen->SetActiveFrameBuffer(0);
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
            if (pressed & kL1) { cyclePage(state, -1); changed = true; }
            else if (pressed & kR1) { cyclePage(state, 1); changed = true; }
            else if (state.dialogOpen && (pressed & (kCircle | kCross))) {
                state.dialogOpen = false; changed = true;
            }
            else if (!state.dialogOpen && (pressed & kCircle)) {
                if (state.page != 0) { state.page = 0; state.focusCards = 0; state.card = 0; changed = true; }
            }
            else if (!state.dialogOpen && (pressed & kUp)) {
                if (state.page == 0 && state.focusCards) { state.focusCards = 0; changed = true; }
            }
            else if (!state.dialogOpen && (pressed & kDown)) {
                if (state.page == 0 && !state.focusCards) { state.focusCards = 1; changed = true; }
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
                    state.page = 1;
                } else {
                    state.dialogOpen = true;
                }
                changed = true;
            }
        }
        if (changed) showFrame(screen, state, frameId);
        sceKernelUsleep(16000);
    }
}