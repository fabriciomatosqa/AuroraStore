#include <cstdio>
#include <cstdint>
#include <sstream>
#include <orbis/libkernel.h>

#include "graphics.h"
#include "log.h"

std::stringstream debugLogStream;

namespace {
constexpr int kWidth = 1920;
constexpr int kHeight = 1080;

const uint8_t glyphs[40][7] = {
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
    {14,17,17,15,1,1,14}, {14,17,19,21,25,17,14}, {31,16,16,16,16,16,31},
    {31,17,17,31,17,17,31}, {31,17,17,31,1,1,31}
};

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

void drawLine(Scene2D* scene, int x0, int y0, int x1, int y1, Color color) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        scene->DrawRectangle(x0 - 2, y0 - 2, 5, 5, color);
        if (x0 == x1 && y0 == y1) break;
        const int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

void drawStartupScreen(Scene2D* scene) {
    const Color bg = {8, 12, 28};
    const Color panel = {18, 27, 52};
    const Color cyan = {67, 232, 255};
    const Color blue = {54, 126, 255};
    const Color violet = {172, 104, 255};
    const Color white = {237, 244, 255};
    const Color muted = {150, 170, 202};

    scene->FrameBufferFill(bg);
    scene->DrawRectangle(250, 804, 1420, 2, panel);
    scene->DrawRectangle(330, 826, 1260, 174, panel);

    // A crystal A mark drawn with the same cyan, blue and violet colors as the icon.
    drawLine(scene, 960, 130, 1135, 430, cyan);
    drawLine(scene, 1135, 430, 785, 430, violet);
    drawLine(scene, 785, 430, 960, 130, blue);
    drawLine(scene, 872, 430, 960, 280, blue);
    drawLine(scene, 960, 280, 1048, 430, cyan);
    drawLine(scene, 872, 430, 1048, 430, violet);

    drawCenteredText(scene, "AURORA STORE", 515, 10, white);
    drawCenteredText(scene, "BUILD 01.03", 640, 6, cyan);
    drawCenteredText(scene, "TESTE VISUAL: APP ABRIU", 860, 5, white);
    drawCenteredText(scene, "CATALOGO E DOWNLOADS AINDA NAO IMPLEMENTADOS", 934, 3, muted);
}
}

int main(void) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    DEBUGLOG << "Aurora Store startup check 01.03";

    // OpenOrbis Scene2D needs two 1080p RGBA buffers and a 2 MiB-aligned allocation.
    Scene2D* screen = new Scene2D(kWidth, kHeight, 4);
    if (!screen->Init(0x1000000, 2)) {
        DEBUGLOG << "Video output initialization failed";
        for (;;) sceKernelUsleep(1000000);
    }

    screen->SetActiveFrameBuffer(0);
    drawStartupScreen(screen);
    screen->SubmitFlip(0);
    screen->FrameWait(0);

    DEBUGLOG << "Startup screen submitted";
    for (;;) sceKernelUsleep(1000000);
}