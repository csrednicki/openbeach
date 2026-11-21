// OpenBeach on Android (docs/build/android.md). SDL's Java activity loads libmain.so and calls SDL_main with --game-dir
// (and --pack l0 for the bundled L0 demo). One process: no desktop settings window and no old-save conversion.

#include "launch_options.h"

#include "app/init.h"
#include "core/log.h"
#include "platform/platform_main.h"

#include <SDL3/SDL_main.h>

#include <android/log.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <unistd.h>

namespace {

using namespace obl;

constexpr int kExitInitFailed = 4;

struct AndroidOptions : launcher::GameOptions {
    std::string pack; // --pack <id>: selects the pack (OBL_PACK), e.g. l0 for the demo
};

bool ParseArgs(int argc, char** argv, AndroidOptions& o)
{
    for (int i = 1; i < argc; ++i) {
        if (launcher::ParseGameOption(argc, argv, i, o)) continue;
        const std::string_view arg = argv[i];
        if (arg == "--pack" && i + 1 < argc) {
            o.pack = argv[++i];
        } else if (arg == "--no-settings") {
            continue;
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "OpenBeach", "unknown argument: %s", argv[i]);
            return false;
        }
    }
    return !o.gameDir.empty();
}

void LogToAndroid(const char* line)
{
    __android_log_print(ANDROID_LOG_INFO, "OpenBeach", "%s", line);
}

} // namespace

int main(int argc, char* argv[])
{
    AndroidOptions o;
    if (!ParseArgs(argc, argv, o)) return 1;
    launcher::SetGameEnvironment(o);
    if (!o.pack.empty()) setenv("OBL_PACK", o.pack.c_str(), 1);
    log::SetDebugSink(&LogToAndroid);
    // The game opens its data by relative paths.
    if (chdir(o.gameDir.c_str()) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "OpenBeach", "cannot enter the game directory %s", o.gameDir.c_str());
        return kExitInitFailed;
    }
    std::string dir = o.gameDir;
    if (dir.back() != '/') dir += '/';
    const int result = OpenGame(dir);
    __android_log_print(ANDROID_LOG_INFO, "OpenBeach", "start-up returned %d", result);
    if (result != 0) return kExitInitFailed;
    std::fflush(stdout);
    return platform::RunGame();
}
