#!/usr/bin/env python3
"""Builds OpenBeach for Android (docs/build/android.md): libmain.so with the NDK, then an APK with the SDK's own tools, no Gradle.

    python tools/obl_android.py [--install] [--run] [--serial HOST:PORT] [--build-dir DIR] [--type Release] [-- cmake -D options]

Needs the Android SDK (ANDROID_HOME, else the default folder of Android Studio) with an NDK, build-tools and a platform, a JDK
(JAVA_HOME or javac on PATH), cmake and ninja (`pip install cmake ninja`). Result: <build-dir>/OpenBeach.apk (arm64-v8a), signed
with the debug key in ~/.android/debug.keystore, made on the first build. --install puts it on the device (adb), --run starts it.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HERE = ROOT / "tools" / "android"
PACKAGE = "org.openbeach.game"
MIN_SDK = 28  # posix_spawn (old_saves.cpp)
TARGET_SDK = 35
WINDOWS = os.name == "nt"


def newest(directory, pattern="*"):
    """The entry of `directory` with the highest version in its name (35.0.0, android-36, 27.1.12297006)."""
    entries = [path for path in directory.glob(pattern) if path.is_dir()]
    if not entries:
        sys.exit(f"error: nothing in {directory}: install it with the SDK manager")
    return max(entries, key=lambda path: [int(number) for number in re.findall(r"\d+", path.name)])


def sdk_root():
    for name in ("ANDROID_HOME", "ANDROID_SDK_ROOT"):
        if os.environ.get(name):
            return Path(os.environ[name])
    for path in (Path(os.environ.get("LOCALAPPDATA", "")) / "Android" / "Sdk", Path.home() / "Android" / "Sdk",
                 Path.home() / "Library" / "Android" / "sdk"):
        if path.is_dir():
            return path
    sys.exit("error: no Android SDK: set ANDROID_HOME")


def tool(directory, name):
    """A tool of the SDK or JDK; on Windows some are .exe and some .bat."""
    for suffix in ((".exe", ".bat", ".cmd") if WINDOWS else ("",)):
        if (directory / (name + suffix)).is_file():
            return str(directory / (name + suffix))
    found = shutil.which(name)
    if not found:
        sys.exit(f"error: no {name} in {directory} or on PATH")
    return found


def run(*command):
    subprocess.run([str(part) for part in command], check=True)


def version():
    """versionName from version.txt, versionCode from its numbers (1.11.1 -> 11101)."""
    name = (ROOT / "version.txt").read_text().split()[0]
    major, minor, patch = (int(part) for part in name.split("."))
    return name, major * 10000 + minor * 100 + patch


def build_library(args, ndk):
    build = Path(args.build_dir)
    configure = [
        "cmake", "-S", ROOT / "src" / "host_linux", "-B", build, "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={args.type}",
        f"-DCMAKE_TOOLCHAIN_FILE={ndk / 'build' / 'cmake' / 'android.toolchain.cmake'}", "-DANDROID_ABI=arm64-v8a",
        f"-DANDROID_PLATFORM=android-{MIN_SDK}", "-DANDROID_STL=c++_static", "-DOBL_LINUX_WINDOWED=ON",
        # Devices with 16 KB pages need segments aligned to them; the NDK's default is 4 KB.
        "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON", "-DCMAKE_SHARED_LINKER_FLAGS=-s", *args.cmake_args,
    ]
    run(*configure)
    run("cmake", "--build", build, "--target", "OpenBeach")
    return build / "bin" / "libmain.so"


def debug_keystore(jdk_bin):
    keystore = Path.home() / ".android" / "debug.keystore"
    if not keystore.is_file():
        keystore.parent.mkdir(parents=True, exist_ok=True)
        run(tool(jdk_bin, "keytool"), "-genkeypair", "-keystore", keystore, "-storepass", "android", "-alias", "androiddebugkey",
            "-keypass", "android", "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000", "-dname", "CN=Android Debug,O=Android,C=US")
    return keystore


def build_apk(args, sdk, library):
    build = Path(args.build_dir)
    tools = newest(sdk / "build-tools")
    android_jar = newest(sdk / "platforms") / "android.jar"
    jdk_bin = Path(os.environ["JAVA_HOME"]) / "bin" if os.environ.get("JAVA_HOME") else Path(shutil.which("javac") or "javac").parent
    stage = build / "apk"
    shutil.rmtree(stage, ignore_errors=True)

    # Java: SDL's activity from its source tree (FetchContent) and ours on top.
    sdl_java = build / "_deps" / "sdl3-src" / "android-project" / "app" / "src" / "main" / "java"
    sources = sorted(str(path) for path in [*sdl_java.rglob("*.java"), *(HERE / "java").rglob("*.java")])
    classes = stage / "classes"
    classes.mkdir(parents=True)
    run(tool(jdk_bin, "javac"), "-source", "8", "-target", "8", "-bootclasspath", android_jar, "-Xlint:-options", "-nowarn",
        "-encoding", "UTF-8", "-d", classes, *sources)
    jar = stage / "classes.jar"
    with zipfile.ZipFile(jar, "w") as zipped:
        for path in classes.rglob("*.class"):
            zipped.write(path, path.relative_to(classes).as_posix())
    run(tool(tools, "d8"), "--release", "--min-api", MIN_SDK, "--lib", android_jar, "--output", stage, jar)

    # Resources (the launcher icon) and assets (the L0 demo, which the activity copies out on its first start).
    icon = stage / "res" / "mipmap" / "ic_launcher.png"
    icon.parent.mkdir(parents=True)
    shutil.copy2(ROOT / "assets" / "logo" / "openbeach-256.png", icon)
    run(tool(tools, "aapt2"), "compile", "--dir", stage / "res", "-o", stage / "res.zip")
    shutil.copytree(ROOT / "data" / "packs" / "l0", stage / "assets" / "l0" / "packs" / "l0")
    name, code = version()
    unsigned = stage / "unsigned.apk"
    run(tool(tools, "aapt2"), "link", "-o", unsigned, "-I", android_jar, "--manifest", HERE / "AndroidManifest.xml",
        "--min-sdk-version", MIN_SDK, "--target-sdk-version", TARGET_SDK, "--version-name", name, "--version-code", code,
        "-A", stage / "assets", stage / "res.zip")
    with zipfile.ZipFile(unsigned, "a", zipfile.ZIP_DEFLATED) as zipped:
        zipped.write(stage / "classes.dex", "classes.dex")
        zipped.write(library, "lib/arm64-v8a/libmain.so")

    aligned = stage / "aligned.apk"
    run(tool(tools, "zipalign"), "-f", "-p", "4", unsigned, aligned)
    apk = build / "OpenBeach.apk"
    run(tool(tools, "apksigner"), "sign", "--ks", debug_keystore(jdk_bin), "--ks-pass", "pass:android", "--key-pass", "pass:android",
        "--out", apk, aligned)
    return apk


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default=str(ROOT / "build" / "android-arm64"))
    parser.add_argument("--type", default="Release")
    parser.add_argument("--install", action="store_true", help="install the APK with adb")
    parser.add_argument("--run", action="store_true", help="install and start it")
    parser.add_argument("--serial", help="the adb device (adb -s), e.g. 192.168.1.10:5555")
    parser.add_argument("cmake_args", nargs="*")
    args = parser.parse_args()

    # A virtualenv with `pip install cmake ninja` keeps its tools beside the interpreter.
    os.environ["PATH"] = str(Path(sys.executable).parent) + os.pathsep + os.environ.get("PATH", "")
    args.build_dir = str(Path(args.build_dir).resolve())
    sdk = sdk_root()
    ndk = Path(os.environ["ANDROID_NDK_HOME"]) if os.environ.get("ANDROID_NDK_HOME") else newest(sdk / "ndk")
    apk = build_apk(args, sdk, build_library(args, ndk))
    print(f"OpenBeach.apk: {apk}")
    if args.install or args.run:
        adb = [tool(sdk / "platform-tools", "adb"), *(["-s", args.serial] if args.serial else [])]
        run(*adb, "install", "-r", apk)
        if args.run:
            run(*adb, "shell", "am", "start", "-n", f"{PACKAGE}/.OpenBeachActivity")


if __name__ == "__main__":
    main()
