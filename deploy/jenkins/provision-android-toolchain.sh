#!/usr/bin/env bash
#
# Provision the QGroundControl Android toolchain natively, without Docker.
#
#   Qt 5.15.2 (android) + NDK r21e + SDK android-34 + JDK 11 + GStreamer 1.18.6
#
# Idempotent: every component is skipped if already present, so re-running on
# each build costs a few seconds. Needs no root - everything lands under
# TOOLCHAIN_ROOT, which only has to be writable by the Jenkins user.
#
# On success it writes $TOOLCHAIN_ROOT/toolchain-env.sh, which the Jenkinsfile
# sources to pick up JAVA_HOME/QT_ANDROID/ANDROID_*/GSTREAMER_ANDROID_ROOT.
#
# Usage:  TOOLCHAIN_ROOT=/var/lib/jenkins/qgc-android-toolchain ./provision-android-toolchain.sh
#
set -euo pipefail

TOOLCHAIN_ROOT="${TOOLCHAIN_ROOT:-${HOME:-/tmp}/qgc-android-toolchain}"

QT_VERSION="${QT_VERSION:-5.15.2}"
QT_ANDROID_ARCH="${QT_ANDROID_ARCH:-android}"
QT_MODULES="${QT_MODULES:-qtcharts}"
AQTINSTALL_VERSION="${AQTINSTALL_VERSION:-3.1.18}"
ANDROID_NDK_VERSION="${ANDROID_NDK_VERSION:-21.4.7075529}"          # r21e
ANDROID_PLATFORM_VERSION="${ANDROID_PLATFORM_VERSION:-34}"
ANDROID_BUILD_TOOLS_VERSION="${ANDROID_BUILD_TOOLS_VERSION:-34.0.0}"
ANDROID_CMDLINE_TOOLS_BUILD="${ANDROID_CMDLINE_TOOLS_BUILD:-9477386}" # 9.0, runs on JDK 11
GSTREAMER_VERSION="${GSTREAMER_VERSION:-1.18.6}"

JDK_DIR="${TOOLCHAIN_ROOT}/jdk11"
QT_PATH="${TOOLCHAIN_ROOT}/Qt"
QT_ANDROID="${QT_PATH}/${QT_VERSION}/${QT_ANDROID_ARCH}"
SDK_DIR="${TOOLCHAIN_ROOT}/android-sdk"
NDK_DIR="${SDK_DIR}/ndk/${ANDROID_NDK_VERSION}"
GST_DIR="${TOOLCHAIN_ROOT}/gstreamer-1.0-android-universal-${GSTREAMER_VERSION}"
VENV_DIR="${TOOLCHAIN_ROOT}/aqt-venv"

log() { echo "[provision] $*"; }

# ---------------------------------------------------------------------------
# Preflight: fail with one clear message rather than halfway through a download
# ---------------------------------------------------------------------------
missing=()
for c in curl unzip tar xz make git python3; do
    command -v "$c" >/dev/null 2>&1 || missing+=("$c")
done
# Check ensurepip, not venv: the venv module imports fine on Debian/Ubuntu even
# when python3-venv is absent, and creation then fails halfway through a build.
python3 -c 'import ensurepip' >/dev/null 2>&1 || missing+=("python3-venv (ensurepip)")
if [ ${#missing[@]} -gt 0 ]; then
    echo "ERROR: missing prerequisites: ${missing[*]}" >&2
    echo "Install them once on this agent, as root:" >&2
    echo "  apt-get install -y make curl unzip xz-utils zip git python3 python3-venv ccache" >&2
    exit 1
fi
command -v ccache >/dev/null 2>&1 || log "NOTE: ccache not found - builds will be slower but will work"

mkdir -p "${TOOLCHAIN_ROOT}"
log "toolchain root: ${TOOLCHAIN_ROOT}"

# ---------------------------------------------------------------------------
# JDK 11. Qt 5.15.2 emits an AGP 7.0.0 project; D8 crashes on newer JDKs, and
# the agent's own JDK is whatever Jenkins needs (21+), so ship our own.
# ---------------------------------------------------------------------------
if [ ! -x "${JDK_DIR}/bin/javac" ]; then
    log "installing Temurin JDK 11 -> ${JDK_DIR}"
    rm -rf "${JDK_DIR}" "${JDK_DIR}.tmp"
    mkdir -p "${JDK_DIR}.tmp"
    curl -fsSL "https://api.adoptium.net/v3/binary/latest/11/ga/linux/x64/jdk/hotspot/normal/eclipse" \
        | tar xz -C "${JDK_DIR}.tmp" --strip-components=1
    mv "${JDK_DIR}.tmp" "${JDK_DIR}"
else
    log "JDK 11 present"
fi
export JAVA_HOME="${JDK_DIR}"
export PATH="${JAVA_HOME}/bin:${PATH}"

# ---------------------------------------------------------------------------
# Qt 5.15.2 for Android. Since Qt 5.14 a single "android" arch carries every
# ABI - this is Qt Creator's "Android Qt 5.15.2 Clang Multi-Abi" kit.
# aqtinstall lives in its own venv so nothing touches system site-packages.
# ---------------------------------------------------------------------------
if [ ! -x "${QT_ANDROID}/bin/qmake" ]; then
    log "installing Qt ${QT_VERSION} (${QT_ANDROID_ARCH}) -> ${QT_PATH}"
    if [ ! -x "${VENV_DIR}/bin/aqt" ]; then
        rm -rf "${VENV_DIR}"
        python3 -m venv "${VENV_DIR}"
        "${VENV_DIR}/bin/pip" install --quiet --upgrade pip
        "${VENV_DIR}/bin/pip" install --quiet "aqtinstall==${AQTINSTALL_VERSION}"
    fi
    "${VENV_DIR}/bin/aqt" install-qt linux android "${QT_VERSION}" "${QT_ANDROID_ARCH}" \
        -O "${QT_PATH}" -m ${QT_MODULES}
else
    log "Qt present"
fi
[ -x "${QT_ANDROID}/bin/qmake" ] || { echo "ERROR: qmake missing after Qt install" >&2; exit 1; }
[ -x "${QT_ANDROID}/bin/androiddeployqt" ] || { echo "ERROR: androiddeployqt missing" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Android SDK + NDK
# ---------------------------------------------------------------------------
if [ ! -x "${SDK_DIR}/cmdline-tools/latest/bin/sdkmanager" ]; then
    log "installing Android command line tools -> ${SDK_DIR}"
    mkdir -p "${SDK_DIR}/cmdline-tools"
    tmpzip="$(mktemp -d)"
    curl -fsSL -o "${tmpzip}/cmdline-tools.zip" \
        "https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_CMDLINE_TOOLS_BUILD}_latest.zip"
    unzip -q "${tmpzip}/cmdline-tools.zip" -d "${tmpzip}"
    rm -rf "${SDK_DIR}/cmdline-tools/latest"
    mv "${tmpzip}/cmdline-tools" "${SDK_DIR}/cmdline-tools/latest"
    rm -rf "${tmpzip}"
fi

export ANDROID_SDK_ROOT="${SDK_DIR}"
export ANDROID_HOME="${SDK_DIR}"
SDKMANAGER="${SDK_DIR}/cmdline-tools/latest/bin/sdkmanager"

if [ ! -d "${SDK_DIR}/platforms/android-${ANDROID_PLATFORM_VERSION}" ] \
   || [ ! -d "${NDK_DIR}" ]; then
    log "installing SDK platform ${ANDROID_PLATFORM_VERSION}, build-tools and NDK ${ANDROID_NDK_VERSION}"
    yes | "${SDKMANAGER}" --licenses > /dev/null 2>&1 || true
    "${SDKMANAGER}" --install \
        "platform-tools" \
        "platforms;android-${ANDROID_PLATFORM_VERSION}" \
        "build-tools;${ANDROID_BUILD_TOOLS_VERSION}" \
        "build-tools;30.0.2" \
        "ndk;${ANDROID_NDK_VERSION}" > /dev/null
else
    log "Android SDK/NDK present"
fi
[ -d "${NDK_DIR}/toolchains/llvm/prebuilt/linux-x86_64" ] \
    || { echo "ERROR: NDK ${ANDROID_NDK_VERSION} incomplete at ${NDK_DIR}" >&2; exit 1; }

# androiddeployqt otherwise picks the highest installed platform for
# compileSdk. AGP 7.0.0's aapt2 cannot parse android-35's android.jar, so warn
# loudly - the Jenkinsfile pins --android-platform explicitly to compensate.
newer="$(ls "${SDK_DIR}/platforms" 2>/dev/null | grep -E 'android-(3[5-9]|[4-9][0-9])' || true)"
if [ -n "${newer}" ]; then
    log "WARNING: SDK platforms newer than 34 are installed (${newer//$'\n'/ })."
    log "         The build pins --android-platform android-${ANDROID_PLATFORM_VERSION}; do not remove that flag."
fi

# ---------------------------------------------------------------------------
# GStreamer android universal. QGC resolves this by relative path from the
# source root, so the Jenkinsfile symlinks it into the workspace.
# ---------------------------------------------------------------------------
if [ ! -d "${GST_DIR}/arm64" ]; then
    log "installing GStreamer ${GSTREAMER_VERSION} android universal (~2 GB) -> ${GST_DIR}"
    rm -rf "${GST_DIR}" "${GST_DIR}.tmp"
    mkdir -p "${GST_DIR}.tmp"
    curl -fsSL "https://gstreamer.freedesktop.org/data/pkg/android/${GSTREAMER_VERSION}/gstreamer-1.0-android-universal-${GSTREAMER_VERSION}.tar.xz" \
        | tar xJ -C "${GST_DIR}.tmp"
    mv "${GST_DIR}.tmp" "${GST_DIR}"
else
    log "GStreamer present"
fi
for abi in arm64 armv7 x86 x86_64; do
    [ -d "${GST_DIR}/${abi}" ] || { echo "ERROR: GStreamer ${abi} missing" >&2; exit 1; }
done

# ---------------------------------------------------------------------------
# Emit the environment the build stages source
# ---------------------------------------------------------------------------
cat > "${TOOLCHAIN_ROOT}/toolchain-env.sh" <<ENVEOF
# Generated by provision-android-toolchain.sh - do not edit
export JAVA_HOME="${JDK_DIR}"
export QT_ANDROID="${QT_ANDROID}"
export ANDROID_SDK_ROOT="${SDK_DIR}"
export ANDROID_HOME="${SDK_DIR}"
export ANDROID_NDK_ROOT="${NDK_DIR}"
export ANDROID_NDK_HOME="${NDK_DIR}"
export ANDROID_NDK_LATEST_HOME="${NDK_DIR}"
export ANDROID_NDK="${NDK_DIR}"
export ANDROID_PLATFORM_VERSION="${ANDROID_PLATFORM_VERSION}"
export GSTREAMER_VERSION="${GSTREAMER_VERSION}"
export GSTREAMER_ANDROID_ROOT="${GST_DIR}"
export PATH="\${JAVA_HOME}/bin:${QT_ANDROID}/bin:${SDK_DIR}/platform-tools:\${PATH}"
ENVEOF

log "wrote ${TOOLCHAIN_ROOT}/toolchain-env.sh"
log "provisioning complete"
