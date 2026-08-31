# Jenkins Android build (Qt 5.15.2, android-clang, multi-ABI)

Builds QGroundControl into a single multi-ABI APK, matching Qt Creator's
**Android Qt 5.15.2 Clang Multi-Abi** kit. The build runs **natively on the
agent** — no Docker.

| File | Purpose |
| --- | --- |
| [`../../Jenkinsfile`](../../Jenkinsfile) | The declarative pipeline |
| [`provision-android-toolchain.sh`](provision-android-toolchain.sh) | Installs Qt 5.15.2, NDK r21e, SDK android-34, JDK 11 and GStreamer 1.18.6 under `TOOLCHAIN_ROOT` |
| [`../docker/Dockerfile-build-android`](../docker/Dockerfile-build-android) | The same toolchain as a container, for local reproduction on a machine where Docker works |

## Agent prerequisites

Install once, as root, on the build agent:

```bash
apt-get install -y make curl unzip xz-utils zip git python3 python3-venv ccache
```

That is the whole root-level requirement. Everything else — Qt, the Android
SDK/NDK, JDK 11, GStreamer — is downloaded by the provisioning script into
`TOOLCHAIN_ROOT` (default `/var/lib/jenkins/qgc-android-toolchain`, editable at
the top of the `Jenkinsfile`) and needs no elevated privileges.

Budget roughly **7 GB** for the toolchain plus **~25 GB** for a three-ABI build
tree. The first build downloads the toolchain; later builds skip it in seconds
because every step is guarded by an existence check.

The agent also needs a label matching `AGENT_LABEL` at the top of the
`Jenkinsfile`. Set it in *Manage Jenkins → Nodes → \<node\> → Labels*.

### Jenkins core and plugins

**Minimum core: 2.504.3**, set by the Git plugin. Verified against **LTS
2.568.2**. Required plugins: Pipeline (`workflow-aggregator`), Git,
Credentials Binding, Timestamper — all install from the Update Center on a
current LTS. The Docker Pipeline and SSH Agent plugins are **no longer
needed**.

The controller runs on Java 21 or 25 (a 2.568.x requirement). That is unrelated
to the **JDK 11** this build needs: the provisioner installs its own Temurin 11
under `TOOLCHAIN_ROOT` and the build uses that via `JAVA_HOME`, leaving the
controller's JVM untouched.

### Credentials

| Credential ID (env var in `Jenkinsfile`) | Kind | Needed for |
| --- | --- | --- |
| `qgc-android-keystore-password` (`ANDROID_KEYSTORE_CREDENTIALS_ID`) | Secret text | Only when `SIGN_RELEASE` is ticked; unlocks the committed `android/android_release.keystore` (alias `QGCAndroidKeyStore`) |

No separate Git credential is needed. Every submodule is HTTPS, and the
checkout uses the Git plugin's `parentCredentials` option so the job's own SCM
credential (a GitHub App works fine) is reused for the private `Aeronavics/*`
submodules.

The checkout also sets `noTags: false`. This matters more than it looks:
multibranch jobs clone with `--no-tags`, and
[`QGCCommon.pri`](../../QGCCommon.pri) only derives a real version when
`git describe` matches `v#.#.#`. Without tags the build silently produces an
APK versioned `0.0.0` with a meaningless `ANDROID_VERSION_CODE`, so the
pipeline marks the build UNSTABLE if no version tag is reachable.

## Job setup

Create a **Multibranch Pipeline** (or a Pipeline job with *Pipeline script from
SCM*) pointing at this repository, script path `Jenkinsfile`. Add a *Filter by
name (with regular expression)* behaviour such as `ANV_4\.4\.3|master` so it
does not build every branch.

On a Multibranch job, parameters do not exist until after the first build — the
first run silently uses the defaults and "Build with Parameters" appears
afterwards.

## Parameters

| Parameter | Default | Notes |
| --- | --- | --- |
| `ANDROID_ABIS` | `armeabi-v7a arm64-v8a x86` | All three ABIs land in one APK. Set to `arm64-v8a` alone for ~3x faster builds. **`x86_64` is not supported** — [`QGCCommon.pri:74`](../../QGCCommon.pri) hard-errors with `Unsupported Android architecture: x86_64`. |
| `QMAKE_CONFIG` | `release` | `release` or `debug` |
| `QGC_BUILD_TYPE` | `DailyBuild` | `DailyBuild` or `StableBuild` |
| `SIGN_RELEASE` | off | Produces `…-multiabi-signed.apk` via `androiddeployqt --release --sign` |
| `BETA_PACKAGE_NAME` | off | Runs `tools/update_android_manifest_package.sh` → package `org.mavlink.qgroundcontrolbeta` |
| `PUBLISH_APK` | **on** | On SUCCESS, copy the APK to the release library and prune older APKs from the same branch |
| `SKIP_PROVISION` | off | Skips the toolchain stage (it normally no-ops in seconds) |
| `CLEAN_BUILD` | off | Wipes `build/android-multiabi` first |

Artifact: `build/android-multiabi/package/QGroundControl-<git describe>-multiabi[-signed].apk`

## Publishing to the release library

On a **successful** build the APK is copied to
`/home/releaseLibrary/AC-16/Hand Controller/APKs/QGroundControl-<branch>-<commit>.apk`
(the path is a constant at the top of the `Jenkinsfile`), and older APKs from
the *same branch* are deleted. Other branches are never touched.

Deliberate choices, because this step both writes outside the workspace and
deletes files:

* **SUCCESS only, not UNSTABLE.** An untagged build is versioned `0.0.0`; it
  should not reach the release library.
* **The destination is validated in an early stage**, before the compile, so a
  permissions or mount problem costs seconds rather than a finished
  multi-hour build.
* **Copy first, prune second**, via a temporary `.part` name. A failed or
  interrupted copy therefore leaves the previous APK intact and prunes nothing.
* **The prune is narrow**: `-maxdepth 1`, regular files only, matching
  `QGroundControl-<branch>-[0-9a-f]*.apk` and explicitly excluding the file
  just written. The hex constraint stops a branch named `feature` from
  matching — and deleting — artifacts of `feature-x`.
* **An empty branch name aborts the step**, since it would otherwise widen the
  prune to every branch's APKs.

The Jenkins user needs write access to that directory. Branch names containing
`/` are flattened to `-` for the filename.

## Why these versions are pinned

`androiddeployqt` from Qt 5.15.2 emits an **Android Gradle Plugin 7.0.0 /
Gradle 7.3** project, which constrains the rest of the toolchain:

* **JDK 11.** D8 dexing crashes with a `NullPointerException` in
  `D8DexArchiveBuilder` under JDK 17/21. The provisioner installs Temurin 11
  rather than relying on the agent's default JDK, which on a modern Jenkins
  agent is 21 or newer.
* **`--android-platform android-34` is passed explicitly.** Left alone,
  `androiddeployqt` uses "the highest available version" for `compileSdk`, and
  AGP 7.0.0's `aapt2` cannot parse android-35's `android.jar`
  (`RES_TABLE_TYPE_TYPE entry offsets overlap actual entry data`). The
  provisioner warns if newer platforms are present in a shared SDK.
* **NDK r21e** is the NDK Qt 5.15.2 was built against.

## Design notes

* **`CONFIG+=installer` is not used.** Its `QMAKE_POST_LINK` hook in
  [`QGCPostLinkInstaller.pri`](../../QGCPostLinkInstaller.pri) runs `make apk`
  per-ABI, which defeats a multi-ABI build. The pipeline instead runs
  `make apk_install_target` followed by a single top-level `androiddeployqt`.
* **GStreamer lives in the toolchain root, not the workspace.**
  [`src/VideoReceiver/VideoReceiver.pri`](../../src/VideoReceiver/VideoReceiver.pri)
  resolves it by relative path from the source root, so the pipeline symlinks
  it in. The path is already in `.gitignore`.
* **ccache** persists at `$TOOLCHAIN_ROOT/ccache` with `CCACHE_BASEDIR` set to
  the workspace so hits survive across builds.

## Running the same build by hand

```bash
TOOLCHAIN_ROOT=$HOME/qgc-android-toolchain ./deploy/jenkins/provision-android-toolchain.sh
```

```bash
. $HOME/qgc-android-toolchain/toolchain-env.sh && ln -sfn "$GSTREAMER_ANDROID_ROOT" ./gstreamer-1.0-android-universal-1.18.6 && mkdir -p build/android-multiabi && cd build/android-multiabi && qmake ../../qgroundcontrol.pro -spec android-clang CONFIG+=release CONFIG+=DailyBuild ANDROID_ABIS="armeabi-v7a arm64-v8a x86" && make -j"$(nproc)" && make apk_install_target && androiddeployqt --input $PWD/android-QGroundControl-deployment-settings.json --output $PWD/android-build --android-platform android-34 --jdk "$JAVA_HOME"
```
