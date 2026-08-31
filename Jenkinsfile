#!/usr/bin/env groovy
//
// QGroundControl - Android build pipeline (native, no Docker)
//
//   Qt 5.15.2 / android-clang / multi-ABI
//   (the "Android Qt 5.15.2 Clang Multi-Abi" kit)
//
// The toolchain is provisioned onto the agent by
// deploy/jenkins/provision-android-toolchain.sh - Qt, NDK r21e, SDK
// android-34, JDK 11 and GStreamer, all under TOOLCHAIN_ROOT, no root needed.
// The script is idempotent, so after the first build it costs a few seconds.
//
// See deploy/jenkins/README.md for agent prerequisites and job setup.
//

// Agent label for the build node. Kept as 'docker' because that label is
// already applied to the existing node - nothing here needs Docker any more,
// so rename it freely as long as the node's label matches.
def AGENT_LABEL = 'docker'

// Where the toolchain is installed. ~7 GB. Must be writable by the Jenkins
// user and should persist between builds - do not put it in the workspace.
def TOOLCHAIN_ROOT = '/var/lib/jenkins/qgc-android-toolchain'

pipeline {
    agent { label AGENT_LABEL }

    options {
        timestamps()
        disableConcurrentBuilds()
        timeout(time: 5, unit: 'HOURS')
        buildDiscarder(logRotator(numToKeepStr: '20', artifactNumToKeepStr: '10'))
        skipDefaultCheckout(true)
    }

    parameters {
        string(
            name: 'ANDROID_ABIS',
            defaultValue: 'armeabi-v7a arm64-v8a x86',
            description: 'Space separated ABI list baked into the single multi-ABI APK. ' +
                         'QGC supports armeabi-v7a, arm64-v8a and x86 only - x86_64 is ' +
                         'rejected by QGCCommon.pri. Drop to "arm64-v8a" for a much faster build.')
        choice(
            name: 'QMAKE_CONFIG',
            choices: ['release', 'debug'],
            description: 'qmake build configuration.')
        choice(
            name: 'QGC_BUILD_TYPE',
            choices: ['DailyBuild', 'StableBuild'],
            description: 'QGC build flavour (drives version string and feature flags).')
        booleanParam(
            name: 'SIGN_RELEASE',
            defaultValue: false,
            description: 'Sign with android/android_release.keystore. Requires the ' +
                         'ANDROID_KEYSTORE_CREDENTIALS_ID secret text credential.')
        booleanParam(
            name: 'BETA_PACKAGE_NAME',
            defaultValue: false,
            description: 'Rewrite the manifest package to org.mavlink.qgroundcontrolbeta ' +
                         'so the build installs alongside a release build.')
        booleanParam(
            name: 'SKIP_PROVISION',
            defaultValue: false,
            description: 'Skip the toolchain provisioning stage. Only safe once the ' +
                         'toolchain is known good - it normally no-ops in seconds.')
        booleanParam(
            name: 'CLEAN_BUILD',
            defaultValue: false,
            description: 'Wipe the shadow build directory before configuring.')
    }

    environment {
        TOOLCHAIN_ROOT                  = "${TOOLCHAIN_ROOT}"
        TOOLCHAIN_ENV                   = "${TOOLCHAIN_ROOT}/toolchain-env.sh"
        BUILD_DIR                       = "${WORKSPACE}/build/android-multiabi"
        PACKAGE_DIR                     = "${WORKSPACE}/build/android-multiabi/package"
        CCACHE_DIR                      = "${TOOLCHAIN_ROOT}/ccache"
        CCACHE_MAXSIZE                  = '10G'
        // Jenkins credential id - see deploy/jenkins/README.md
        ANDROID_KEYSTORE_CREDENTIALS_ID = 'qgc-android-keystore-password'
    }

    stages {

        stage('Checkout') {
            steps {
                // Let the Git plugin do the whole checkout rather than raw git:
                //  * parentCredentials reuses the job's own credential for the
                //    private Aeronavics submodules (all submodules are HTTPS).
                //  * noTags:false is essential - multibranch clones with
                //    --no-tags, and QGCCommon.pri only derives a real VERSION
                //    when `git describe` matches v#.#.#. Without tags the APK
                //    silently builds as 0.0.0 with a bogus versionCode.
                checkout([
                    $class: 'GitSCM',
                    branches: scm.branches,
                    userRemoteConfigs: scm.userRemoteConfigs,
                    // Extensions are set explicitly rather than merged with
                    // scm.extensions: a CloneOption inherited from the branch
                    // source could otherwise re-assert noTags and quietly undo
                    // the tag fetch.
                    extensions: [
                        [$class: 'CloneOption', noTags: false, shallow: false, depth: 0, honorRefspec: false, timeout: 30],
                        [$class: 'SubmoduleOption', parentCredentials: true, recursiveSubmodules: true,
                         disableSubmodules: false, trackingSubmodules: false, timeout: 60],
                    ],
                ])
                script {
                    env.QGC_VERSION = sh(
                        script: 'git describe --tags --always',
                        returnStdout: true).trim()
                    if (!(env.QGC_VERSION ==~ /^v\d+\.\d+\.\d+.*/)) {
                        unstable("git describe returned '${env.QGC_VERSION}' - no version tag reachable, " +
                                 'so QGC will build as VERSION 0.0.0. Check that tags were fetched.')
                    }
                    echo "Building QGroundControl ${env.QGC_VERSION} for ABIs: ${params.ANDROID_ABIS}"
                }
            }
        }

        stage('Provision toolchain') {
            when { expression { !params.SKIP_PROVISION } }
            steps {
                sh '"${WORKSPACE}/deploy/jenkins/provision-android-toolchain.sh"'
            }
        }

        stage('Prepare workspace') {
            steps {
                sh '''
                    set -eu
                    if [ "${CLEAN_BUILD}" = "true" ]; then
                        rm -rf "${BUILD_DIR}"
                    fi
                    mkdir -p "${BUILD_DIR}" "${PACKAGE_DIR}" "${CCACHE_DIR}"
                '''
                script {
                    if (params.BETA_PACKAGE_NAME) {
                        // Must run from the source root - the script edits
                        // android/AndroidManifest.xml by relative path.
                        sh './tools/update_android_manifest_package.sh'
                    }
                }
            }
        }

        stage('qmake + build') {
            steps {
                sh '''
                    set -eu
                    . "${TOOLCHAIN_ENV}"
                    export CCACHE_BASEDIR="${WORKSPACE}"

                    # QGC looks for GStreamer at <source root>/gstreamer-1.0-android-universal-<ver>.
                    # Link the provisioned copy, unless a real directory has
                    # been placed there by hand.
                    GST_LINK="${WORKSPACE}/gstreamer-1.0-android-universal-${GSTREAMER_VERSION}"
                    if [ ! -e "${GST_LINK}" ] || [ -L "${GST_LINK}" ]; then
                        ln -sfn "${GSTREAMER_ANDROID_ROOT}" "${GST_LINK}"
                    fi

                    cd "${BUILD_DIR}"
                    "${QT_ANDROID}/bin/qmake" "${WORKSPACE}/qgroundcontrol.pro" \
                        -spec android-clang \
                        CONFIG+=${QMAKE_CONFIG} \
                        CONFIG+=${QGC_BUILD_TYPE} \
                        ANDROID_ABIS="${ANDROID_ABIS}"

                    make -j"$(nproc)"
                    ccache -s || true
                '''
            }
        }

        stage('Package APK') {
            steps {
                script {
                    // CONFIG+=installer is deliberately not used: its post-link
                    // `make apk` runs per-ABI, which defeats a multi-ABI build.
                    // androiddeployqt is invoked once, at the top level, with
                    // --android-platform pinned - it otherwise picks the
                    // highest installed SDK, and AGP 7.0.0's aapt2 cannot parse
                    // android-35's android.jar.
                    def deploy = '''
                        set -eu
                        . "${TOOLCHAIN_ENV}"
                        cd "${BUILD_DIR}"
                        make apk_install_target
                    '''
                    if (params.SIGN_RELEASE) {
                        withCredentials([string(credentialsId: env.ANDROID_KEYSTORE_CREDENTIALS_ID,
                                                variable: 'ANDROID_KEYSTORE_PASSWORD')]) {
                            sh deploy + '''
                                "${QT_ANDROID}/bin/androiddeployqt" --verbose \
                                    --input "${BUILD_DIR}/android-QGroundControl-deployment-settings.json" \
                                    --output "${BUILD_DIR}/android-build" \
                                    --android-platform "android-${ANDROID_PLATFORM_VERSION}" \
                                    --jdk "${JAVA_HOME}" \
                                    --release \
                                    --sign "${WORKSPACE}/android/android_release.keystore" QGCAndroidKeyStore \
                                    --storepass "${ANDROID_KEYSTORE_PASSWORD}"

                                cp android-build/build/outputs/apk/release/android-build-release-signed.apk \
                                   "${PACKAGE_DIR}/QGroundControl-${QGC_VERSION}-multiabi-signed.apk"
                            '''
                        }
                    } else {
                        sh deploy + '''
                            "${QT_ANDROID}/bin/androiddeployqt" --verbose \
                                --input "${BUILD_DIR}/android-QGroundControl-deployment-settings.json" \
                                --output "${BUILD_DIR}/android-build" \
                                --android-platform "android-${ANDROID_PLATFORM_VERSION}" \
                                --jdk "${JAVA_HOME}"

                            cp android-build/build/outputs/apk/debug/android-build-debug.apk \
                               "${PACKAGE_DIR}/QGroundControl-${QGC_VERSION}-multiabi.apk"
                        '''
                    }
                }
                sh 'ls -lh "${PACKAGE_DIR}"'
            }
        }

        stage('Archive') {
            steps {
                archiveArtifacts artifacts: 'build/android-multiabi/package/*.apk',
                                 fingerprint: true,
                                 onlyIfSuccessful: true
            }
        }
    }

    post {
        failure {
            echo 'Build failed - check the qmake/androiddeployqt/Gradle output above.'
        }
        cleanup {
            // Drop the GStreamer symlink so a workspace wipe never chases it
            // into the toolchain root. Never touch a real directory.
            sh 'GST_LINK="${WORKSPACE}/gstreamer-1.0-android-universal-1.18.6"; [ -L "${GST_LINK}" ] && rm -f "${GST_LINK}"; true'
        }
    }
}
