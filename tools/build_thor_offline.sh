#!/usr/bin/env bash
# Prepared only; run after the widescreen adaptation is complete.
set -euo pipefail
if [[ ${ALFP_WIDESCREEN_READY:-0} != 1 ]]; then
    printf '%s\n' 'Build disabled: adaptation completion must be confirmed first.' >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export ANDROID_HOME=/home/plex-rober/Android/Sdk
export ANDROID_SDK_ROOT=$ANDROID_HOME
export CMAKE_BUILD_PARALLEL_LEVEL=1
gradle=/home/plex-rober/.gradle/wrapper/dists/gradle-8.7-bin/bhs2wmbdwecv87pi65oeuq5iu/gradle-8.7/bin/gradle
for needed in "$JAVA_HOME/bin/java" "$gradle" "$ANDROID_HOME/platforms/android-35/android.jar" \
              "$ANDROID_HOME/ndk/27.1.12297006/build/cmake/android.toolchain.cmake"; do
    [[ -f $needed ]] || { printf 'Missing cached dependency: %s\n' "$needed" >&2; exit 1; }
done
cd -- "$project/android"
nice -n 15 ionice -c 3 "$gradle" --offline --no-daemon --max-workers=1 --console=plain \
    -PgbaNativeJobs=1 -PgbaAbis=arm64-v8a :app:assembleDebug
cd -- "$project"
python3 tools/check_thor_apk.py android/app/build/outputs/apk/debug/app-debug.apk
