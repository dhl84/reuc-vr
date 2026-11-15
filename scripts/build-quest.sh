#!/usr/bin/env bash
# Builds the Quest 3 app (quest/reuc) : the game cross-compiled for Android arm64 as libmain.so, SDL3, the Khronos OpenXR loader, the
# port's shaders as assets, then the APK with Gradle (as gse-recomp's quest/build_vulkan.py does for HOTD4).
#   scripts/build-quest.sh            -> quest/reuc/build/outputs/apk/debug/reuc-debug.apk
# Needs: the translated game in generated/ (scripts/build.sh --translate-only), the Android SDK + NDK 27.3, JDK 17, the OpenXR loader
# AAR unpacked (UC_OPENXR_DIR, default ~/Downloads/VR/deps/oxr), slangc (UC_SLANGC), Vulkan headers newer than the NDK's
# (UC_VULKAN_HEADERS, default Homebrew's vulkan-headers). Settings may come from local.env.
# UC_TITLE=dc builds The Darkside Chronicles (SBDE08, docs/darkside-port.md) from generated-dc/ and DC_INSTALL_DIR, in build-android-dc,
# as its own app (org.uc.darksidechronicles.vr). UC_GRADLE_ONLINE=1 lets Gradle download what its cache does not have yet.
# UC_APP_VERSION / UC_APP_VERSION_CODE: the APK's versionName / versionCode (releases: 0.1.5 -> 105).
# UC_QUEST_BUILD_DIR: another build folder (flag experiments side by side); UC_QUEST_CMAKE_ARGS: extra configure arguments (word-split).
# ThinLTO is on by default (UC_THINLTO=OFF turns it off): with the PGO profile, the fight replay's guest CPU -1.5% more on the Quest.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/scripts/local.sh"
SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
NDK="${ANDROID_NDK_HOME:-$SDK/ndk/27.3.13750724}"
OXR="${UC_OPENXR_DIR:-$HOME/Downloads/VR/deps/oxr}"
VKH="${UC_VULKAN_HEADERS:-/opt/homebrew/opt/vulkan-headers/include}"
JDK="${UC_JAVA_HOME:-/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home}"
SLANGC="${UC_SLANGC:-$(command -v slangc || true)}"
# PGO (CMakeLists.txt): UC_QUEST_PGO=generate builds an instrumented app in build-android-pgo (scripts/quest_pgo.sh records the profile
# with the fight replay); otherwise project/pgo/uc-quest.profdata, when present, is the optimizer's profile. UC_QUEST_PGO=off: neither.
PGO_MODE="${UC_QUEST_PGO:-use}"
PGO_PROFILE="$ROOT/project/pgo/uc-quest.profdata"
TITLE="${UC_TITLE:-uc}"; GEN="$ROOT/generated"; INSTALL="${UC_INSTALL_DIR:-}"; TITLE_ARGS=(); GRADLE_TITLE=(); DEFAULT_BUILD="$ROOT/build-android"
if [ "$TITLE" = dc ]; then
  GEN="$ROOT/generated-dc"; INSTALL="${DC_INSTALL_DIR:-}"; DEFAULT_BUILD="$ROOT/build-android-dc"; PGO_MODE="${UC_QUEST_PGO:-off}"   # no profile of this game yet
  TITLE_ARGS=(-DUC_TITLE=dc -DUC_GENERATED_DIR="$GEN")
  GRADLE_TITLE=(-PappId=org.uc.darksidechronicles.vr "-PappName=Darkside Chronicles VR" -PdiscId=SBDE08 -PgameFolder=DarksideChronicles "-PgameTitle=Resident Evil: The Darkside Chronicles")
fi
BUILD="${UC_QUEST_BUILD_DIR:-$DEFAULT_BUILD}"; PGO_ARGS=(-DUC_PGO_GENERATE=OFF -DUC_PGO_PROFILE="$PGO_PROFILE")
if [ "$PGO_MODE" = generate ]; then BUILD="$ROOT/build-android-pgo"; PGO_ARGS=(-DUC_PGO_GENERATE=ON)
elif [ "$PGO_MODE" = off ]; then PGO_ARGS=(-DUC_PGO_GENERATE=OFF -DUC_PGO_PROFILE="$ROOT/project/pgo/none.profdata"); fi
APP="$ROOT/quest/reuc/src/main"
SHA256="shasum -a 256"; command -v shasum >/dev/null 2>&1 || SHA256=sha256sum   # Git Bash without perl has only sha256sum
for need in "$NDK/build/cmake/android.toolchain.cmake" "$OXR/jni/arm64-v8a/libopenxr_loader.so" "$VKH/vulkan/vulkan_core.h" "$JDK/bin/java" "$SLANGC"; do
  [ -e "$need" ] || { echo "ERROR: missing $need (see the header of $0)" >&2; exit 1; }
done
[ -f "$GEN/game_sources.cmake" ] || { echo "ERROR: $GEN is empty: translate the game first (scripts/build.sh --translate-only, or scripts/bringup-dc.bat for UC_TITLE=dc)" >&2; exit 1; }

DOL_SHA="$($SHA256 "${INSTALL:?set UC_INSTALL_DIR (or DC_INSTALL_DIR for UC_TITLE=dc) in local.env}/game/DATA/sys/main.dol" | cut -d' ' -f1)"
# The Wii common key for reading the player's disc image in place: by default the player puts common-key.bin next to the image.
# UC_WII_COMMON_KEY_FILE (a 16-byte file on this machine, e.g. in local.env) builds it into this APK instead; it never enters the sources.
KEY_ARGS=(-DUC_WII_COMMON_KEY_FILE="${UC_WII_COMMON_KEY_FILE:-}"); KEY_EMBEDDED=0; [ -n "${UC_WII_COMMON_KEY_FILE:-}" ] && KEY_EMBEDDED=1

echo "[1/4] Configure and build libmain.so (arm64-v8a)"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DUC_SLANGC="$SLANGC" -DUC_THINLTO="${UC_THINLTO:-ON}" \
  -DUC_TARGET_CPU="${UC_TARGET_CPU:-cortex-a78c}" -DVulkan_INCLUDE_DIR="$VKH" -DUC_OPENXR_DIR="$OXR" -DUC_DOL_SHA256="$DOL_SHA" ${TITLE_ARGS[@]+"${TITLE_ARGS[@]}"} "${KEY_ARGS[@]}" "${PGO_ARGS[@]}" ${UC_QUEST_CMAKE_ARGS:-} > "$BUILD.configure.log" 2>&1 \
  || { echo "ERROR: configure failed (see $BUILD.configure.log)" >&2; tail -20 "$BUILD.configure.log" >&2; exit 1; }
cmake --build "$BUILD" --target uc || { echo "ERROR: build failed" >&2; exit 1; }
grep -h "PGO profile" "$BUILD.configure.log" || echo "PGO: $PGO_MODE (no profile in use)"

echo "[2/4] Stage libraries, assets and SDL's Java sources"
rm -rf "$APP/libs" "$APP/assets" "$APP/sdl-java"
mkdir -p "$APP/libs/arm64-v8a" "$APP/assets"
HOST_TAG="$(ls "$NDK/toolchains/llvm/prebuilt" | head -1)"   # darwin-x86_64, linux-x86_64 or windows-x86_64
"$NDK/toolchains/llvm/prebuilt/$HOST_TAG/bin/llvm-strip" -o "$APP/libs/arm64-v8a/libmain.so" "$BUILD/game/libmain.so"
cp "$BUILD/game/libSDL3.so" "$OXR/jni/arm64-v8a/libopenxr_loader.so" "$APP/libs/arm64-v8a/"
cp -R "$BUILD/game/data" "$APP/assets/data"
( cd "$APP/assets" && find data -type f | LC_ALL=C sort > data.files && cat data.files | xargs $SHA256 | $SHA256 | cut -c1-16 > data.stamp )
cp -R "$BUILD/_deps/sdl3-src/android-project/app/src/main/java" "$APP/sdl-java"

echo "[3/4] Gradle (offline)"
echo "sdk.dir=$SDK" > "$ROOT/quest/local.properties"
# Debug by default, as gse-recomp's Vulkan Quest builds: run-as (the runner's private game folder) needs a debuggable app. The native code is
# the optimised build either way. UC_APK_TYPE=release for the signed release variant.
TYPE="${UC_APK_TYPE:-debug}"; TASK="assemble$(echo "${TYPE:0:1}" | tr a-z A-Z)${TYPE:1}"
OFFLINE=--offline; [ -n "${UC_GRADLE_ONLINE:-}" ] && OFFLINE=
( cd "$ROOT/quest" && JAVA_HOME="$JDK" ./gradlew $OFFLINE -q ":reuc:$TASK" -PdolSha256="$DOL_SHA" -PcommonKeyEmbedded="$KEY_EMBEDDED" ${GRADLE_TITLE[@]+"${GRADLE_TITLE[@]}"} \
  ${UC_APP_VERSION:+-PappVersionName="$UC_APP_VERSION"} ${UC_APP_VERSION_CODE:+-PappVersionCode="$UC_APP_VERSION_CODE"} )

APK="$ROOT/quest/reuc/build/outputs/apk/$TYPE/reuc-$TYPE.apk"
echo "[4/4] $APK ($(du -h "$APK" | cut -f1))"
