// SDL3 presentation of the windowed renderer.
//
// The renderer's Present (ui::renderer::Present) hands the 16 bpp back buffer (system memory, top row first, RGB555) to PresentBackBuffer after the
// optional fade; it is uploaded to an SDL streaming texture and presented letterboxed. Everything before it (screenshot
// capture, fade, dirty flags) is the renderer's. Without a GPU renderer SDL's software renderer presents the same way.
//
// SDL creates the game window and the game keeps the SDL_Window* as its opaque window handle. The game loop pumps SDL
// events (quit, close, focus); the input adapter consumes the input events.

#include "platform/sdl_present.h"

#include "game/game/game_accessors.h"
#include "core/config.h"
#include "core/cursor_file.h"
#include "platform/display_window.h"
#include "platform/hidden_window.h"
#ifdef OBL_HANDHELD
#include "game/ui/sprite_anim.h"
#include "platform/cursor.h"
#include "platform/frame_share.h"
#include "platform/input.h"
#endif
#ifdef __ANDROID__
#include "platform/arrow_cursor.h"
#include "platform/cursor.h"
#include "platform/input.h"
#include "platform/touch_pad.h"
#include "platform/wide_screen.h"
#include "game/ui/screen_backdrop.h"

#include <dlfcn.h>
#include <string>
#endif
#include "core/log.h"
#include "core/platform_main.h"
#include "openbeach_icon.gen.h"

#include <SDL3/SDL.h>
#ifdef _WIN32
#define SDL_MAIN_HANDLED // only SDL_RegisterApp: no main() of SDL's here
#include <SDL3/SDL_main.h>
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace obl::platform {

namespace {

SdlPresenterStats g_stats;
bool g_failed;
bool g_verifyRequested;
bool g_created; // g_window is the game window SDL created; it outlives presenter failures
bool g_attached; // the presenter's renderer is set up on g_window
SDL_Window* g_window;
SDL_Renderer* g_renderer;
SDL_Texture* g_texture;
SDL_PixelFormat g_sourceFormat = SDL_PIXELFORMAT_UNKNOWN;

SdlEventCounts g_eventCounts;
bool g_quitRequested; // sticky: every loop that pumps (intro movie, then the game loop) sees it
bool g_closePosted; // PostSdlGameWindowClose: taken by the next pump like the close button
std::vector<SDL_Event> g_postedEvents; // PostSdlEvent: pushed by the next pump
int g_quitCode;
unsigned g_closeRequests; // close requests of the game window (its close button, or a posted WM_CLOSE)
bool g_active; // the game window has keyboard focus (SDL focus events)

bool SDLCALL DropEvent(void*, SDL_Event* event)
{
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: ++g_eventCounts.keyboard; return true;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (g_window && event->window.windowID == SDL_GetWindowID(g_window)) g_active = event->type == SDL_EVENT_WINDOW_FOCUS_GAINED;
        ++g_eventCounts.other;
        return true;
    case SDL_EVENT_QUIT:
    case SDL_EVENT_TERMINATING:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_RENDER_TARGETS_RESET:
    case SDL_EVENT_RENDER_DEVICE_RESET: ++g_eventCounts.other; return true; // taken by PumpSdlEvents
    case SDL_EVENT_WINDOW_MOUSE_LEAVE: ++g_eventCounts.other; return true;
    case SDL_EVENT_MOUSE_MOTION: ++g_eventCounts.mouseMotion; return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: ++g_eventCounts.mouseButtons; return true;
    case SDL_EVENT_MOUSE_WHEEL: ++g_eventCounts.mouseWheel; return true;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_CANCELED: ++g_eventCounts.other; return true; // the touch gestures (input.cpp) read them
    default: // window, device added/removed, focus...
        ++g_eventCounts.other;
        break;
    }
    return false;
}

bool EnsureVideo()
{
    if (SDL_WasInit(SDL_INIT_VIDEO)) return true;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return false;
    SDL_SetEventFilter(&DropEvent, nullptr);
    return true;
}

void Shutdown()
{
#ifdef OBL_HANDHELD
    SharedFrameClose();
#endif
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    g_texture = nullptr;
    g_renderer = nullptr;
    g_attached = false;
    g_stats.active = false;
}

bool Fail(const char* what)
{
    OBL_LOG("sdl presenter: %s failed: %s - frames are dropped", what, SDL_GetError());
    Shutdown();
    g_failed = true;
    return false;
}

bool Attach()
{
    if (g_attached) return true;
    if (!g_window) {
        OBL_LOG("sdl presenter: no SDL game window");
        g_failed = true;
        return false;
    }
    // Hidden runs off Windows draw on SDL's offscreen driver, where nobody sees the output: its GL surface keeps the size the
    // window was created with, so after a resize the read-back no longer matches the window. The software renderer follows it.
    const char* driver = SDL_GetCurrentVideoDriver();
    const bool offscreen = driver && std::strcmp(driver, "offscreen") == 0;
#ifdef __EMSCRIPTEN__
    // A browser composites the canvas over the page, so a canvas with alpha would add the page's colors to the game's frame:
    // the back buffer's unused top bit reaches it as alpha 0 (EnsureTexture).
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
#endif
    g_renderer = SDL_CreateRenderer(g_window, offscreen ? SDL_SOFTWARE_RENDERER : nullptr);
    if (!g_renderer) {
        // No GPU renderer (a remote session without one, a broken driver): SDL draws the same output in software.
        OBL_LOG("sdl presenter: SDL_CreateRenderer failed: %s - trying the software renderer", SDL_GetError());
        g_renderer = SDL_CreateRenderer(g_window, SDL_SOFTWARE_RENDERER);
        if (!g_renderer) return Fail("SDL_CreateRenderer(software)");
        g_stats.software = true;
    }
    // Frame pacing belongs to the game loop and the frame limiter ([Display] MaxFps).
    SDL_SetRenderVSync(g_renderer, 0);

    g_attached = true;
    g_stats.active = true;
    OBL_LOG("sdl presenter: SDL %d.%d.%d, renderer '%s' on the game window", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
            SDL_GetRendererName(g_renderer));
    return true;
}

bool EnsureTexture(SDL_PixelFormat format, int width, int height)
{
    if (g_texture && format == g_sourceFormat && width == g_stats.textureWidth && height == g_stats.textureHeight) return true;
    if (g_texture) SDL_DestroyTexture(g_texture);
    // GPUs sample B5G5R5A1 but have no X1R5G5B5 format, and SDL would convert XRGB1555 on the CPU
    // every frame (~10 FPS lost at 1920x1080). The game's unused top bit is 0; with blending off the
    // alpha it becomes is ignored.
    SDL_PixelFormat textureFormat = format == SDL_PIXELFORMAT_XRGB1555 ? SDL_PIXELFORMAT_ARGB1555 : format;
    g_texture = SDL_CreateTexture(g_renderer, textureFormat, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!g_texture) return Fail("SDL_CreateTexture");
    SDL_SetTextureBlendMode(g_texture, SDL_BLENDMODE_NONE);
#ifdef __ANDROID__
    // Fractional scaling on phone screens breaks pixel fonts under nearest sampling (docs/build/android.md).
    SDL_SetTextureScaleMode(g_texture, SDL_SCALEMODE_LINEAR);
#else
    SDL_SetTextureScaleMode(g_texture, SDL_SCALEMODE_NEAREST);
#endif
    g_sourceFormat = format;
    g_stats.textureWidth = width;
    g_stats.textureHeight = height;
    OBL_LOG("sdl presenter: texture %dx%d %s from %s", width, height, SDL_GetPixelFormatName(textureFormat),
            SDL_GetPixelFormatName(format));
    return true;
}

// Largest rectangle with the source aspect ratio, centred in the output.
SDL_FRect LetterboxRect(int srcW, int srcH, int outW, int outH)
{
    screen::Rect r = ui::ImageRect(srcW, srcH, outW, outH);
    return {static_cast<float>(r.left), static_cast<float>(r.top), static_cast<float>(r.right - r.left),
            static_cast<float>(r.bottom - r.top)};
}

// Compares the rendered output inside `dst` with the source pixels (nearest sampling). GPU and SDL
// expand 5/6-bit channels slightly differently, hence the tolerance.
void Verify(const uint16_t* bits, int pitch, int width, int height, const SDL_FRect& dst)
{
    SDL_Surface* shot = SDL_RenderReadPixels(g_renderer, nullptr);
    SDL_Surface* rgb = shot ? SDL_ConvertSurface(shot, SDL_PIXELFORMAT_XRGB8888) : nullptr;
    SDL_DestroySurface(shot);
    if (!rgb) {
        OBL_LOG("sdl presenter: verify read-back failed: %s", SDL_GetError());
        return;
    }
    const SDL_PixelFormatDetails* src = SDL_GetPixelFormatDetails(g_sourceFormat);
    int x0 = static_cast<int>(dst.x), y0 = static_cast<int>(dst.y);
    int x1 = std::min(rgb->w, static_cast<int>(dst.x + dst.w)), y1 = std::min(rgb->h, static_cast<int>(dst.y + dst.h));
    long long pixels = 0, mismatches = 0;
    long long barPixels = 0, barMismatches = 0;
    for (int y = 0; y < rgb->h; ++y) {
        auto out = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(rgb->pixels) + y * rgb->pitch);
        for (int x = 0; x < rgb->w; ++x) {
            if (x >= x0 && x < x1 && y >= y0 && y < y1) continue;
            ++barPixels;
            if (out[x] & 0xffffff) ++barMismatches;
        }
    }
    // The source pixel(s) nearest to the centre of output pixel `i` of `out` pixels over `in`: at an exact tie (every third
    // pixel at 1.5x) both neighbours are nearest, and renderers differ (SDL's software scaler steps in truncated fixed point).
    struct Nearest {
        int first, last;
    };
    auto nearest = [](int i, int in, int out) {
        const long long num = (2LL * i + 1) * in, den = 2LL * out;
        const int at = static_cast<int>(std::min<long long>(in - 1, num / den));
        return Nearest{num % den == 0 && at > 0 ? at - 1 : at, at};
    };
    const int dstW = static_cast<int>(dst.w), dstH = static_cast<int>(dst.h);
    auto matches = [&](uint16_t source, uint32_t o) {
        Uint8 r, g, b;
        SDL_GetRGB(source, src, nullptr, &r, &g, &b);
        int dr = std::abs(static_cast<int>((o >> 16) & 0xff) - r);
        int dg = std::abs(static_cast<int>((o >> 8) & 0xff) - g);
        int db = std::abs(static_cast<int>(o & 0xff) - b);
        return dr <= 8 && dg <= 8 && db <= 8;
    };
    for (int y = y0; y < y1; ++y) {
        const Nearest sy = nearest(y - y0, height, dstH);
        auto out = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(rgb->pixels) + y * rgb->pitch);
        for (int x = x0; x < x1; ++x) {
            const Nearest sx = nearest(x - x0, width, dstW);
            bool match = false;
            for (int row = sy.first; row <= sy.last && !match; ++row) {
                auto line = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(bits) + row * pitch);
                for (int column = sx.first; column <= sx.last && !match; ++column) match = matches(line[column], out[x]);
            }
            ++pixels;
            if (!match) ++mismatches;
        }
    }
    SDL_DestroySurface(rgb);
    g_stats.lastPixels = pixels;
    g_stats.lastMismatches = mismatches;
    g_stats.lastBarPixels = barPixels;
    g_stats.lastBarMismatches = barMismatches;
    ++g_stats.verifications;
    OBL_LOG("sdl presenter: verify %dx%d -> rect %.0f,%.0f %.0fx%.0f pixels=%lld mismatches=%lld", width, height, dst.x, dst.y, dst.w,
            dst.h, pixels, mismatches);
}

#ifdef __ANDROID__
constexpr float kPi = 3.14159265f;

// A filled slice of a circle from angle `from` to `to` (radians, clockwise from the right). 128 edges for a whole circle: at the
// pad's radius of about 200 pixels the polygon stays within a tenth of a pixel of the round edge.
void FillSlice(float x, float y, float radius, float from, float to, SDL_FColor color)
{
    constexpr int kCircleSteps = 128;
    const int steps = std::clamp(static_cast<int>(std::ceil(kCircleSteps * (to - from) / (2 * kPi))), 1, kCircleSteps);
    SDL_Vertex vertices[kCircleSteps + 2];
    int indices[kCircleSteps * 3];
    vertices[0] = {{x, y}, color, {0, 0}};
    for (int i = 0; i <= steps; ++i) {
        const float angle = from + (to - from) * static_cast<float>(i) / static_cast<float>(steps);
        vertices[i + 1] = {{x + radius * std::cos(angle), y + radius * std::sin(angle)}, color, {0, 0}};
    }
    for (int i = 0; i < steps; ++i) {
        indices[3 * i] = 0;
        indices[3 * i + 1] = i + 1;
        indices[3 * i + 2] = i + 2;
    }
    SDL_RenderGeometry(g_renderer, nullptr, vertices, steps + 2, indices, steps * 3);
}

void FillDisc(const DpadCircle& circle, SDL_FColor color)
{
    FillSlice(circle.x, circle.y, circle.radius, 0, 2 * kPi, color);
}

// A convex polygon of up to four corners (an arrow, the magnifier's handle).
void FillConvex(const SDL_FPoint* points, int count, SDL_FColor color)
{
    SDL_Vertex vertices[4];
    int indices[6];
    for (int i = 0; i < count; ++i) vertices[i] = {points[i], color, {0, 0}};
    for (int i = 0; i + 2 < count; ++i) {
        indices[3 * i] = 0;
        indices[3 * i + 1] = i + 1;
        indices[3 * i + 2] = i + 2;
    }
    SDL_RenderGeometry(g_renderer, nullptr, vertices, count, indices, 3 * (count - 2));
}

// A light rim around a dark body, as the D-pad and the magnifier's button share; `lit` turns the body blue.
void DrawControl(const DpadCircle& circle, bool lit)
{
    FillDisc(circle, {0.85f, 0.95f, 1.0f, 0.4f});
    FillDisc({circle.x, circle.y, circle.radius - std::max(3.0f, 0.07f * circle.radius)},
            lit ? SDL_FColor{0.1f, 0.4f, 0.7f, 0.8f} : SDL_FColor{0.04f, 0.09f, 0.15f, 0.6f});
}

// The magnifier's button: a glass with a plus and its handle, the handle pointing to the lower right.
void DrawMagnifierButton(const DpadCircle& button, bool on)
{
    DrawControl(button, on);
    const SDL_FColor icon = on ? SDL_FColor{1, 1, 1, 0.95f} : SDL_FColor{0.75f, 0.87f, 1.0f, 0.75f};
    const float r = button.radius;
    const DpadCircle glass{button.x - 0.12f * r, button.y - 0.12f * r, 0.42f * r};
    const float d = 0.7071f; // the handle's direction, 45 degrees
    const float from = 0.9f * glass.radius, to = glass.radius + 0.42f * r, half = 0.09f * r;
    const SDL_FPoint handle[4] = {{glass.x + d * (from - half), glass.y + d * (from + half)},
            {glass.x + d * (from + half), glass.y + d * (from - half)}, {glass.x + d * (to + half), glass.y + d * (to - half)},
            {glass.x + d * (to - half), glass.y + d * (to + half)}};
    FillConvex(handle, 4, icon);
    FillDisc(glass, icon);
    // The glass's inside is opaque, so the ring stays a ring over the translucent body.
    FillDisc({glass.x, glass.y, 0.76f * glass.radius}, on ? SDL_FColor{0.1f, 0.4f, 0.7f, 1} : SDL_FColor{0.04f, 0.09f, 0.15f, 1});
    const float arm = 0.48f * glass.radius, thick = 0.07f * r;
    SDL_SetRenderDrawColorFloat(g_renderer, icon.r, icon.g, icon.b, icon.a);
    const SDL_FRect plus[2] = {{glass.x - arm, glass.y - thick, 2 * arm, 2 * thick}, {glass.x - thick, glass.y - arm, 2 * thick, 2 * arm}};
    SDL_RenderFillRects(g_renderer, plus, 2);
}

// The magnifier's zoom: the quarter of the texture around the zoom's centre, which the picture shows twice the size.
SDL_FRect ZoomSource(const SDL_FRect& picture, const TouchPadView& view, int width, int height)
{
    const float kx = static_cast<float>(width) / picture.w, ky = static_cast<float>(height) / picture.h;
    return {(view.zoomX - picture.x - picture.w / 4) * kx, (view.zoomY - picture.y - picture.h / 4) * ky, width / 2.0f, height / 2.0f};
}

// The D-pad and touchpad controls over the frame (touch_pad.h, Android with the touch controls off): the arrow cursor at the picture's scale (Android shows no
// pointer for fingers; zoomed with the picture), a round D-pad whose held arrows and their quarter light up, the magnifier's button.
void DrawTouchPad(int outW, int outH, const SDL_FRect& picture, int gameWidth, const TouchPadView& view)
{
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    int cursorX = 0, cursorY = 0;
    if (HandheldCursor(cursorX, cursorY) && LocalCursorVisible() && gameWidth > 0) {
        float x = static_cast<float>(cursorX), y = static_cast<float>(cursorY), scale = picture.w / static_cast<float>(gameWidth);
        if (view.zoomed) {
            x = picture.x + picture.w / 2 + 2 * (x - view.zoomX);
            y = picture.y + picture.h / 2 + 2 * (y - view.zoomY);
            scale *= 2;
        }
        SDL_FRect outline[17 * 12], fill[17 * 12];
        int outlines = 0, fills = 0;
        for (int row = 0; row < 17; ++row)
            for (int column = 0; column < 12; ++column) {
                const SDL_FRect pixel{x + column * scale, y + row * scale, scale, scale};
                if (kArrow[row][column] == 'X') outline[outlines++] = pixel;
                if (kArrow[row][column] == '.') fill[fills++] = pixel;
            }
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderFillRects(g_renderer, outline, outlines);
        SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
        SDL_RenderFillRects(g_renderer, fill, fills);
    }
    if (view.magnifierButton && view.button.radius > 0) DrawMagnifierButton(view.button, view.magnifier);
    const DpadCircle& pad = view.pad;
    if (pad.radius <= 0) return;
    DrawControl(pad, false);
    const float inner = 0.93f * pad.radius;
    constexpr struct {
        unsigned bit;
        float angle;
    } kArrows[] = {{kDpadRight, 0}, {kDpadDown, kPi / 2}, {kDpadLeft, kPi}, {kDpadUp, -kPi / 2}};
    for (const auto& arrow : kArrows) {
        const bool on = (view.arrows & arrow.bit) != 0;
        if (on) FillSlice(pad.x, pad.y, inner, arrow.angle - kPi / 4, arrow.angle + kPi / 4, {0.6f, 0.85f, 1.0f, 0.3f});
        const float c = std::cos(arrow.angle), s = std::sin(arrow.angle);
        const float tip = 0.8f * pad.radius, base = 0.48f * pad.radius, half = 0.2f * pad.radius;
        const SDL_FPoint triangle[3] = {{pad.x + c * tip, pad.y + s * tip}, {pad.x + c * base - s * half, pad.y + s * base + c * half},
                {pad.x + c * base + s * half, pad.y + s * base - c * half}};
        FillConvex(triangle, 3, on ? SDL_FColor{1, 1, 1, 0.95f} : SDL_FColor{0.75f, 0.87f, 1.0f, 0.65f});
    }
}
#endif

#ifdef __ANDROID__
// The wide (21:9) versions of the full-screen pictures (wide_screen.h), from the APK's assets (screens_wide/<key>.avif): the image
// loader takes their part under the 800x600 screen instead of the original screen (image.cpp), and the presenter draws the whole
// picture at the same place behind the frame, so it fills the bars beside it; where the frame still shows that part it is drawn in
// the picture's full colours, not the frame's 16 bits (FullColourFrame). Android decodes AVIF (AImageDecoder: API 30, AVIF from
// Android 12); libjnigraphics is opened at run time because the APK starts at API 26. Without it the original screens show with
// black bars, as do gameplay and the intro movie (intro_movie.cpp).
constexpr int kScreenWidth = 800, kScreenHeight = 600; // the screens' size, which the wide part covers
SDL_Texture* g_wide;
std::string g_wideKey; // the screen g_wide shows, or the one that has no wide version
int g_wideWidth, g_wideHeight;
std::vector<uint32_t> g_widePart; // its part under the screen (SampleWide)
SDL_Texture* g_fullColour; // the frame in full colour
std::vector<uint32_t> g_fullColourPixels;

void DropWide()
{
    if (g_wide) SDL_DestroyTexture(g_wide);
    if (g_fullColour) SDL_DestroyTexture(g_fullColour);
    g_wide = g_fullColour = nullptr;
    g_wideKey.clear();
    g_widePart.clear();
}

// RGBA pixels of an AVIF file, or empty.
std::vector<uint8_t> DecodeAvif(const void* data, size_t size, int& width, int& height)
{
    struct Decoder;
    struct Header;
    using Create = int (*)(const void*, size_t, Decoder**);
    using GetHeader = const Header* (*)(const Decoder*);
    using GetSize = int32_t (*)(const Header*);
    using GetStride = size_t (*)(Decoder*);
    using Decode = int (*)(Decoder*, void*, size_t, size_t);
    using Delete = void (*)(Decoder*);
    static void* library = dlopen("libjnigraphics.so", RTLD_NOW);
    if (!library) return {};
    const auto create = reinterpret_cast<Create>(dlsym(library, "AImageDecoder_createFromBuffer"));
    const auto header = reinterpret_cast<GetHeader>(dlsym(library, "AImageDecoder_getHeaderInfo"));
    const auto getWidth = reinterpret_cast<GetSize>(dlsym(library, "AImageDecoderHeaderInfo_getWidth"));
    const auto getHeight = reinterpret_cast<GetSize>(dlsym(library, "AImageDecoderHeaderInfo_getHeight"));
    const auto stride = reinterpret_cast<GetStride>(dlsym(library, "AImageDecoder_getMinimumStride"));
    const auto decode = reinterpret_cast<Decode>(dlsym(library, "AImageDecoder_decodeImage"));
    const auto destroy = reinterpret_cast<Delete>(dlsym(library, "AImageDecoder_delete"));
    Decoder* decoder = nullptr;
    if (!create || !header || !getWidth || !getHeight || !stride || !decode || !destroy || create(data, size, &decoder) != 0) return {};
    width = getWidth(header(decoder));
    height = getHeight(header(decoder));
    const size_t pitch = stride(decoder); // RGBA_8888, the decoder's default
    std::vector<uint8_t> pixels(pitch * static_cast<size_t>(height));
    const bool ok =
            width > 0 && height > 0 && pitch == static_cast<size_t>(width) * 4 && decode(decoder, pixels.data(), pitch, pixels.size()) == 0;
    destroy(decoder);
    if (!ok) pixels.clear();
    return pixels;
}

// The RGBA pixels of a screen's wide version, or empty.
std::vector<uint8_t> LoadWide(const std::string& key, int& width, int& height)
{
    if (!FindWidePlacement(key)) return {};
    size_t size = 0;
    void* file = SDL_LoadFile(("screens_wide/" + key + ".avif").c_str(), &size);
    if (!file) return {};
    std::vector<uint8_t> pixels = DecodeAvif(file, size, width, height);
    SDL_free(file);
    if (pixels.empty()) OBL_LOG("present: screens_wide/%s.avif not decoded, the original screen stays", key.c_str());
    return pixels;
}

// screen_backdrop::g_wideScreen: the wide version's part under the screen.
bool WideScreenPart(const std::string& key, bool rgb555, int width, int height, std::vector<uint16_t>& pixels, int& pictureX, int& pictureY)
{
    int w = 0, h = 0;
    const std::vector<uint8_t> rgba = LoadWide(key, w, h);
    if (rgba.empty()) return false;
    std::vector<uint32_t> part(static_cast<size_t>(width) * static_cast<size_t>(height));
    SampleWide(rgba.data(), w, h, *FindWidePlacement(key), part.data(), width, height);
    pixels.resize(part.size());
    std::transform(part.begin(), part.end(), pixels.begin(), [rgb555](uint32_t colour) { return To16(colour, rgb555); });
    pictureX = FindWidePlacement(key)->pictureX;
    pictureY = FindWidePlacement(key)->pictureY;
    return true;
}

[[maybe_unused]] const bool g_wideScreenSet = (ui::screen_backdrop::g_wideScreen = &WideScreenPart, true);

// The wide version of the screen on show behind the picture (`frameWidth` game pixels wide), where its part in the frame lies; the
// rows a picture leaves above or below repeat its edge rows; only while the frame shows that part (ShowsWidePart: the menus and
// the loading screens, not the game, even while the world loads). Answers the frame in full colour, or null to show the frame as it
// is.
SDL_Texture* DrawWideBackdrop(const SDL_FRect& picture, const uint16_t* bits, int pitch, int frameWidth, int frameHeight, bool rgb555)
{
    const std::string& key = ui::screen_backdrop::Current();
    if (key != g_wideKey) {
        DropWide();
        g_wideKey = key;
        int w = 0, h = 0;
        const std::vector<uint8_t> pixels = LoadWide(key, w, h);
        if (!pixels.empty()) {
            g_wide = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
            if (g_wide) {
                SDL_SetTextureScaleMode(g_wide, SDL_SCALEMODE_LINEAR);
                SDL_SetTextureBlendMode(g_wide, SDL_BLENDMODE_NONE);
                SDL_UpdateTexture(g_wide, nullptr, pixels.data(), w * 4);
                g_wideWidth = w;
                g_wideHeight = h;
                g_widePart.resize(static_cast<size_t>(kScreenWidth) * kScreenHeight);
                SampleWide(pixels.data(), w, h, *FindWidePlacement(key), g_widePart.data(), kScreenWidth, kScreenHeight);
            }
        }
    }
    if (!g_wide || frameWidth != kScreenWidth || frameHeight != kScreenHeight) return nullptr;
    if (!ShowsWidePart(bits, pitch / 2, g_widePart.data(), rgb555, kScreenWidth, kScreenHeight)) return nullptr;
    const WidePlacement& place = *FindWidePlacement(key);
    const float k = picture.w / static_cast<float>(frameWidth); // client pixels per game pixel
    const SDL_FRect back{picture.x + place.x * k, picture.y + place.y * k, static_cast<float>(g_wideWidth) * place.scale * k,
            static_cast<float>(g_wideHeight) * place.scale * k};
    SDL_RenderTexture(g_renderer, g_wide, nullptr, &back);
    const SDL_FRect topRow{0, 0, static_cast<float>(g_wideWidth), 1}, bottomRow{0, static_cast<float>(g_wideHeight - 1), topRow.w, 1};
    if (back.y > picture.y) {
        const SDL_FRect above{back.x, picture.y, back.w, back.y - picture.y};
        SDL_RenderTexture(g_renderer, g_wide, &topRow, &above);
    }
    if (back.y + back.h < picture.y + picture.h) {
        const SDL_FRect below{back.x, back.y + back.h, back.w, picture.y + picture.h - back.y - back.h};
        SDL_RenderTexture(g_renderer, g_wide, &bottomRow, &below);
    }
    if (!g_fullColour) {
        g_fullColour = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kScreenWidth, kScreenHeight);
        if (!g_fullColour) return nullptr;
        SDL_SetTextureBlendMode(g_fullColour, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(g_fullColour, SDL_SCALEMODE_LINEAR);
    }
    g_fullColourPixels.resize(g_widePart.size());
    FullColourFrame(bits, pitch / 2, g_widePart.data(), rgb555, kScreenWidth, kScreenHeight, g_fullColourPixels.data());
    SDL_UpdateTexture(g_fullColour, nullptr, g_fullColourPixels.data(), kScreenWidth * 4);
    return g_fullColour;
}
#endif

bool Present(const uint16_t* bits, int pitch, int width, int height, SDL_PixelFormat format)
{
    if (!Attach() || !EnsureTexture(format, width, height)) return false;
    if (!SDL_UpdateTexture(g_texture, nullptr, bits, pitch)) return Fail("SDL_UpdateTexture");
    int outW = 0, outH = 0;
    SDL_GetCurrentRenderOutputSize(g_renderer, &outW, &outH);
    SDL_FRect dst = LetterboxRect(width, height, std::max(outW, 1), std::max(outH, 1));
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    const SDL_FRect* source = nullptr; // the whole frame
    SDL_Texture* frame = g_texture;
#ifdef __ANDROID__
    const TouchPadView view = CurrentTouchPadView();
    const SDL_FRect zoom = ZoomSource(dst, view, width, height);
    if (view.zoomed) source = &zoom;
    if (!view.zoomed && dst.x >= 1.0f)
        if (SDL_Texture* fullColour = DrawWideBackdrop(dst, bits, pitch, width, height, format == SDL_PIXELFORMAT_XRGB1555))
            frame = fullColour;
#endif
    SDL_RenderTexture(g_renderer, frame, source, &dst);
#ifdef __ANDROID__
    DrawTouchPad(outW, outH, dst, width, view);
#endif
    if (g_verifyRequested) {
        g_verifyRequested = false;
        Verify(bits, pitch, width, height, dst);
    }
    if (!SDL_RenderPresent(g_renderer)) return Fail("SDL_RenderPresent");
    ++g_stats.presents;
    return true;
}

// The OpenBeach icon, built into the program (assets/logo/openbeach.ico): the same with an imported copy and with the L0
// pack, and the same file the Windows executable shows in Explorer (src/launcher/openbeach.rc.in).
bool SetGameIcon(SDL_Window* window)
{
    cursor_file::Image image;
    if (!cursor_file::DecodeIcon(kOpenBeachIcon, sizeof(kOpenBeachIcon), image)) return false;
    SDL_Surface* surface = SDL_CreateSurfaceFrom(image.width, image.height, SDL_PIXELFORMAT_ARGB8888, image.argb.data(), image.width * 4);
    const bool ok = surface && SDL_SetWindowIcon(window, surface);
    SDL_DestroySurface(surface);
    return ok;
}

} // namespace

SDL_Window* CreateSdlGameWindow(void* instance, int clientWidth, int clientHeight, const char* title)
{
#ifdef _WIN32
    // The class SDL registers for its windows is the game's: tools find the window by its name.
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        OBL_LOG("sdl window: SDL video already running, the window class keeps SDL's name");
    } else {
        if (!SDL_RegisterApp("Beach Life", CS_BYTEALIGNCLIENT | CS_OWNDC, instance))
            OBL_LOG("sdl window: SDL_RegisterApp failed: %s", SDL_GetError());
    }
#else
    (void)instance;
#endif
#ifndef _WIN32
    // The desktop matches the window to the release's openbeach.desktop by this id (Wayland app_id, X11 WM_CLASS), which
    // gives the dock and the task switcher the OpenBeach icon too.
    SDL_SetHint(SDL_HINT_APP_ID, "openbeach");
#endif
    if (!EnsureVideo()) {
        OBL_LOG("sdl window: SDL_InitSubSystem(VIDEO) failed: %s", SDL_GetError());
        return nullptr;
    }
    // Borderless is the game's monitor-covering mode: a plain WS_POPUP, which Windows treats as full screen (taskbar
    // below it). SDL's default borderless style keeps WS_CAPTION hidden behind its WM_NCCALCSIZE and the taskbar on top.
    SDL_SetHint("SDL_BORDERLESS_WINDOWED_STYLE", "0");
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, clientWidth);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, clientHeight);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
#ifdef __EMSCRIPTEN__
    // A resizable window follows the canvas's CSS size, which the page changes with the tab: the back buffer would then
    // be stretched by the presenter (nearest, at a fractional factor) instead of by the browser.
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, false);
#elif defined(__ANDROID__)
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, false);
#else
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
#endif
    SDL_Window* window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!window) {
        OBL_LOG("sdl window: SDL_CreateWindowWithProperties failed: %s", SDL_GetError());
        return nullptr;
    }
    g_window = window;
    g_created = true;
    g_stats.created = true;
#ifndef __ANDROID__
    g_stats.icon = SetGameIcon(window);
#endif
#ifdef OBL_HANDHELD
    // The A53 takes 150-375 ms to decode a guest's sheet: animations decode in the background before they are drawn.
    ui::EnableSpritePrefetch();
#endif
#ifndef __ANDROID__
    if (!g_stats.icon) OBL_LOG("sdl window: the OpenBeach icon was not set: %s", SDL_GetError());
#endif
    // SDL's own window procedure stays: close, focus and input reach the game as SDL events (PumpSdlEvents, input.cpp) and the
    // local cursor is an SDL cursor (cursor.cpp).
#ifdef _WIN32
    AdoptHiddenGameWindow(GetWin32Window(window));
#endif
    OBL_LOG("sdl window: SDL created the game window (%dx%d client, class 'Beach Life')", clientWidth, clientHeight);
    return window;
}

void ShowSdlGameWindow(int showCommand)
{
    if (!g_created) return;
    const platform_main::ShowAction action = platform_main::SdlShowAction(showCommand);
    if (action != platform_main::ShowAction::Hide) SDL_ShowWindow(g_window);
    if (action == platform_main::ShowAction::ShowMinimized) SDL_MinimizeWindow(g_window);
}

void DestroySdlGameWindow()
{
    if (!g_created) return;
    g_created = false;
    g_stats.created = false;
    Shutdown();
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
    OBL_LOG("sdl window: game window destroyed");
}

SDL_Window* SdlGameWindow(void* window)
{
    return g_created && window && window == g_window ? g_window : nullptr;
}

void GameWindowClientSize(void* window, int& width, int& height)
{
    width = height = 0;
    if (SDL_Window* sdl = SdlGameWindow(window)) SDL_GetWindowSizeInPixels(sdl, &width, &height);
}

#ifdef _WIN32
HWND GetWin32Window(void* window)
{
    SDL_Window* sdl = SdlGameWindow(window);
    return sdl ? static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(sdl), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr))
               : nullptr;
}
#endif

bool PresentBackBuffer(const uint16_t* bits, int pitch, int width, int height, bool rgb555)
{
    if (g_failed || !bits || width <= 0 || height <= 0) return false;
#ifdef OBL_HANDHELD
    // No video driver for SDL3: the frame goes to the panel thread (frame_share.cpp). The window SDL's offscreen driver made
    // only carries events; the cursor position is in its client pixels, which is the game's when the window has the game's size.
    SharedCursor cursor;
    int clientW = 0, clientH = 0;
    int x = 0, y = 0;
    if (g_window) SDL_GetWindowSizeInPixels(g_window, &clientW, &clientH);
    if (HandheldCursor(x, y) && clientW > 0 && clientH > 0) {
        cursor.visible = LocalCursorVisible();
        cursor.x = x * width / clientW;
        cursor.y = y * height / clientH;
    }
    if (!SharedFramePresent(bits, pitch, width, height, rgb555, cursor)) return false;
    ++g_stats.presents;
    return true;
#endif
    return Present(bits, pitch, width, height, rgb555 ? SDL_PIXELFORMAT_XRGB1555 : SDL_PIXELFORMAT_RGB565);
}

SDL_Renderer* AttachSdlRenderer(void* window)
{
    if (g_failed || !SdlGameWindow(window) || !Attach()) return nullptr;
    return g_renderer;
}

const SdlPresenterStats& GetSdlPresenterStats()
{
    g_stats.windowWidth = g_stats.windowHeight = 0;
    if (g_window) SDL_GetWindowSize(g_window, &g_stats.windowWidth, &g_stats.windowHeight);
    g_stats.wrapped = g_window && game::GetGameWindow() == g_window;
    return g_stats;
}

SdlPumpResult PumpSdlEvents()
{
    SdlPumpResult result;
    if (!EnsureVideo()) return result;
    // A failed presenter only stops presenting; events still come through SDL.
    if (SdlGameWindow(game::GetGameWindow()) && !g_failed) Attach();
    SDL_PumpEvents();
    for (const SDL_Event& posted : g_postedEvents) {
        SDL_Event copy = posted;
        if (!SDL_PushEvent(&copy)) OBL_LOG("sdl window: posted event %u lost: %s", copy.type, SDL_GetError());
    }
    g_postedEvents.clear();
    // Taken here, before the input adapter drains its range (which contains the window events), so both the intro movie
    // and the game loop see them. The close button hides the window SDL owns; DestroySdlGameWindow ends it after the game.
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_QUIT, SDL_EVENT_TERMINATING) > 0) g_quitRequested = true;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_RENDER_TARGETS_RESET, SDL_EVENT_RENDER_DEVICE_RESET) > 0) {
        // Android can recreate the GLES surface after background/resume; drop the texture so the next frame recreates it.
        if (g_texture) {
            SDL_DestroyTexture(g_texture);
            g_texture = nullptr;
            g_sourceFormat = SDL_PIXELFORMAT_UNKNOWN;
        }
#ifdef __ANDROID__
        DropWide();
#endif
    }
    while (g_closePosted ||
            SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_CLOSE_REQUESTED, SDL_EVENT_WINDOW_CLOSE_REQUESTED) > 0) {
        if (g_closePosted) {
            g_closePosted = false;
        } else if (event.window.windowID != SdlGameWindowId()) {
            continue;
        }
        ++g_closeRequests;
        OBL_LOG("sdl window: close requested (%u), quitting", g_closeRequests);
        SDL_HideWindow(g_window);
        g_quitRequested = true;
    }
    result.pumped = true;
    result.quit = g_quitRequested;
    result.quitCode = g_quitCode;
    result.active = g_active;
    result.closeRequests = g_closeRequests;
    return result;
}

void RequestSdlQuit(int code)
{
    g_quitRequested = true;
    g_quitCode = code;
}

void PostSdlGameWindowClose()
{
    // A flag, not a pushed SDL event: the input adapter drains the window events during the frame, before the next pump.
    g_closePosted = true;
}

void PostSdlEvent(const SDL_Event& event)
{
    g_postedEvents.push_back(event);
}

uint32_t SdlGameWindowId()
{
    return g_window ? SDL_GetWindowID(g_window) : 0;
}

SdlEventCounts GetSdlEventCounts()
{
    return g_eventCounts;
}

void RequestSdlPresentVerify()
{
    g_verifyRequested = true;
}

} // namespace obl::platform
