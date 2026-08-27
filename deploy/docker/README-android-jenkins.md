# Jenkins Android build (Qt 5.15.2, android-clang, multi-ABI)

Builds QGroundControl into a single multi-ABI APK entirely inside a container,
matching Qt Creator's **Android Qt 5.15.2 Clang Multi-Abi** kit.

| File | Purpose |
| --- | --- |
| [`Dockerfile-build-android`](Dockerfile-build-android) | The toolchain image: Qt 5.15.2 (android), NDK r21e, SDK android-34, JDK 11, GStreamer 1.18.6 android-universal |
| [`../../Jenkinsfile`](../../Jenkinsfile) | The declarative pipeline |

## Jenkins job setup

1. Create a **Multibranch Pipeline** (or a Pipeline job with *Pipeline script from
   SCM*) pointing at this repository; script path `Jenkinsfile`.
2. The agent needs a label matching `AGENT_LABEL` at the top of the `Jenkinsfile`
   (default `docker`), a working Docker daemon, and roughly **40 GB** of free
   disk — the image alone is ~17.5 GB on disk / 4.3 GB compressed (Qt, NDK and
   the 2 GB extracted GStreamer universal), before any build output.
3. Plugins: **Docker Pipeline**, **SSH Agent**, **Credentials Binding**,
   **Timestamper**, plus the usual Pipeline/Git set.

### Jenkins core version

**Minimum core: 2.541.3**, set by the Docker Pipeline plugin. Verified against
**LTS 2.568.2**, where every plugin below installs at its current release
straight from the Update Center:

| Plugin | Version | Requires core |
| --- | --- | --- |
| Docker Pipeline (`docker-workflow`) | 653.v2f2c08eff0ec | 2.541.3 |
| SSH Agent (`ssh-agent`) | 427.v5818150c4b_c6 | 2.479.3 |
| Credentials Binding | 728.v902a_273b_8947 | 2.479.3 |
| Timestamper | 1.30 | 2.479.3 |
| Pipeline (`workflow-aggregator`) | 608.v67378e9d3db_1 | 2.479.3 |
| Git | 5.10.1 | 2.504.3 |

Note that Jenkins' update site only publishes metadata for releases up to about
a year old, so a controller much older than that is offered nothing
installable and needs `.hpi` files placed by hand.

The controller itself runs on **Java 21 or 25** (a 2.568.x requirement). That
is unrelated to the **JDK 11** this build needs — the build JDK lives inside
the container and never touches the controller's JVM.

### Credentials

| Credential ID (env var in `Jenkinsfile`) | Kind | Needed for |
| --- | --- | --- |
| `aeronavics-github-ssh` (`GIT_SSH_CREDENTIALS_ID`) | SSH private key | The `libs/mavlink/include/mavlink/v2.0` submodule uses an `git@github.com:` remote |
| `qgc-android-keystore-password` (`ANDROID_KEYSTORE_CREDENTIALS_ID`) | Secret text | Only when `SIGN_RELEASE` is ticked; unlocks the committed `android/android_release.keystore` (alias `QGCAndroidKeyStore`) |

## Parameters

| Parameter | Default | Notes |
| --- | --- | --- |
| `ANDROID_ABIS` | `armeabi-v7a arm64-v8a x86` | All three ABIs land in one APK. Set to `arm64-v8a` alone for ~3x faster builds. **`x86_64` is not supported** — [`QGCCommon.pri:74`](../../QGCCommon.pri) hard-errors with `Unsupported Android architecture: x86_64`. |
| `QMAKE_CONFIG` | `release` | `release` or `debug` |
| `QGC_BUILD_TYPE` | `DailyBuild` | `DailyBuild` or `StableBuild` |
| `SIGN_RELEASE` | off | Produces `…-multiabi-signed.apk` via `androiddeployqt --release --sign` |
| `BETA_PACKAGE_NAME` | off | Runs `tools/update_android_manifest_package.sh` → package `org.mavlink.qgroundcontrolbeta` |
| `REBUILD_IMAGE` | off | `docker build --pull --no-cache` |
| `CLEAN_BUILD` | off | Wipes `build/android-multiabi` first |

Artifact: `build/android-multiabi/package/QGroundControl-<git describe>-multiabi[-signed].apk`

## Why these versions are pinned

`androiddeployqt` from Qt 5.15.2 emits an **Android Gradle Plugin 7.0.0 /
Gradle 7.3** project, which constrains the rest of the toolchain:

* **JDK 11.** D8 dexing crashes with a `NullPointerException` in
  `D8DexArchiveBuilder` under JDK 17/21.
* **Only `platforms;android-34` is installed.** `androiddeployqt` picks the
  highest installed SDK platform for `compileSdk`; AGP 7.0.0's `aapt2` cannot
  parse android-35's `android.jar` (`RES_TABLE_TYPE_TYPE entry offsets overlap
  actual entry data`). The image build fails fast if anything ≥ 35 appears.
* **NDK r21e** is the NDK Qt 5.15.2 was built against.

## Design notes

* **`CONFIG+=installer` is not used.** Its `QMAKE_POST_LINK` hook in
  [`QGCPostLinkInstaller.pri`](../../QGCPostLinkInstaller.pri) runs `make apk`
  per-ABI, which defeats a multi-ABI build. The pipeline instead runs
  `make apk` (or `make apk_install_target` + an explicit `androiddeployqt
  --release --sign`) once at the top level of the shadow build directory.
* **GStreamer lives in the image, not the workspace.**
  [`src/VideoReceiver/VideoReceiver.pri`](../../src/VideoReceiver/VideoReceiver.pri)
  resolves it by relative path from the source root, so the pipeline symlinks
  `/opt/gstreamer-1.0-android-universal-1.18.6` into the workspace. The path is
  already in `.gitignore`.
* **Caches** are Docker named volumes (`qgc-android-ccache`,
  `qgc-android-gradle`, `qgc-android-home`) mounted under `/cache`, which the
  image makes world-writable so they work regardless of the uid Jenkins runs
  the container as. Gradle 7.3 (~120 MB) is downloaded once per volume.
* The pipeline re-exports `PATH` inside every `sh` block because Jenkins
  injects the agent's `PATH` into the container, shadowing the image's.

## Running the same build locally

```bash
docker build -t qgc-android-build:qt5.15.2 -f deploy/docker/Dockerfile-build-android deploy/docker

docker run --rm -it -v "$PWD:/project" -w /project qgc-android-build:qt5.15.2 bash -c '
  ln -sfn "$GSTREAMER_ANDROID_ROOT" /project/gstreamer-1.0-android-universal-1.18.6
  mkdir -p /project/build/android-multiabi && cd /project/build/android-multiabi
  qmake /project/qgroundcontrol.pro -spec android-clang CONFIG+=release CONFIG+=DailyBuild \
      ANDROID_ABIS="armeabi-v7a arm64-v8a x86"
  make -j"$(nproc)" && make apk'
```
