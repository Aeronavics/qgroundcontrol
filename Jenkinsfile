#!/usr/bin/env groovy
//
// QGroundControl - Android build pipeline
//
//   Qt 5.15.2 / android-clang / multi-ABI
//   (the "Android Qt 5.15.2 Clang Multi-Abi" kit, as a container build)
//
// Everything is compiled inside deploy/docker/Dockerfile-build-android, so the
// only thing the Jenkins agent needs is a working Docker daemon.
//
// See deploy/docker/README-android-jenkins.md for job/credential setup.
//

// Agent label that has Docker available.
def AGENT_LABEL = 'docker'

// Persistent named volumes keep ccache and the Gradle distribution/cache warm
// across builds. /cache is world-writable in the image, so these work no
// matter which uid Jenkins runs the container as.
def DOCKER_RUN_ARGS = [
    '-v qgc-android-ccache:/cache/ccache',
    '-v qgc-android-gradle:/cache/gradle',
    '-v qgc-android-home:/cache/home',
    '-e HOME=/cache/home',
    '-e CCACHE_DIR=/cache/ccache',
    '-e GRADLE_USER_HOME=/cache/gradle',
].join(' ')

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
            name: 'REBUILD_IMAGE',
            defaultValue: false,
            description: 'Rebuild the toolchain image from scratch (--pull --no-cache).')
        booleanParam(
            name: 'CLEAN_BUILD',
            defaultValue: false,
            description: 'Wipe the shadow build directory before configuring.')
    }

    environment {
        IMAGE_TAG                       = 'qgc-android-build:qt5.15.2'
        QT_ANDROID                      = '/opt/Qt/5.15.2/android'
        GSTREAMER_VERSION               = '1.18.6'
        GSTREAMER_ANDROID_ROOT          = '/opt/gstreamer-1.0-android-universal-1.18.6'
        BUILD_DIR                       = "${WORKSPACE}/build/android-multiabi"
        PACKAGE_DIR                     = "${WORKSPACE}/build/android-multiabi/package"
        // Jenkins credential id - see README-android-jenkins.md
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

        stage('Build toolchain image') {
            steps {
                script {
                    def opts = params.REBUILD_IMAGE ? '--pull --no-cache' : ''
                    // Context is deploy/docker so the 2 GB source tree is not
                    // shipped to the Docker daemon.
                    docker.build(
                        env.IMAGE_TAG,
                        "${opts} -f ${WORKSPACE}/deploy/docker/Dockerfile-build-android ${WORKSPACE}/deploy/docker")
                }
            }
        }

        stage('Prepare workspace') {
            steps {
                sh '''
                    set -eu
                    if [ "${CLEAN_BUILD}" = "true" ]; then
                        rm -rf "${BUILD_DIR}"
                    fi
                    mkdir -p "${BUILD_DIR}" "${PACKAGE_DIR}"
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
                script {
                    docker.image(env.IMAGE_TAG).inside(DOCKER_RUN_ARGS) {
                        sh '''
                            set -eu
                            # Jenkins injects its own PATH into the container, so
                            # put the Qt host tools back in front.
                            export PATH="${QT_ANDROID}/bin:${PATH}"
                            export CCACHE_BASEDIR="${WORKSPACE}"

                            # QGC looks for GStreamer at <source root>/gstreamer-1.0-android-universal-<ver>.
                            # Link the copy baked into the image, unless a real
                            # directory has been placed there by hand.
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
            }
        }

        stage('Package APK') {
            steps {
                script {
                    // CONFIG+=installer is deliberately not used: its post-link
                    // `make apk` runs per-ABI, which defeats a multi-ABI build.
                    // Package once, from the top level, instead.
                    if (params.SIGN_RELEASE) {
                        withCredentials([string(credentialsId: env.ANDROID_KEYSTORE_CREDENTIALS_ID,
                                                variable: 'ANDROID_KEYSTORE_PASSWORD')]) {
                            docker.image(env.IMAGE_TAG).inside(DOCKER_RUN_ARGS) {
                                sh '''
                                    set -eu
                                    export PATH="${QT_ANDROID}/bin:${PATH}"
                                    cd "${BUILD_DIR}"

                                    # The target installs into ${BUILD_DIR}/android-build itself.
                                    make apk_install_target

                                    "${QT_ANDROID}/bin/androiddeployqt" --verbose \
                                        --input "${BUILD_DIR}/android-QGroundControl-deployment-settings.json" \
                                        --output "${BUILD_DIR}/android-build" \
                                        --gradle \
                                        --release \
                                        --sign "${WORKSPACE}/android/android_release.keystore" QGCAndroidKeyStore \
                                        --storepass "${ANDROID_KEYSTORE_PASSWORD}"

                                    cp android-build/build/outputs/apk/release/android-build-release-signed.apk \
                                       "${PACKAGE_DIR}/QGroundControl-${QGC_VERSION}-multiabi-signed.apk"
                                '''
                            }
                        }
                    } else {
                        docker.image(env.IMAGE_TAG).inside(DOCKER_RUN_ARGS) {
                            sh '''
                                set -eu
                                export PATH="${QT_ANDROID}/bin:${PATH}"
                                cd "${BUILD_DIR}"

                                make apk

                                cp android-build/build/outputs/apk/debug/android-build-debug.apk \
                                   "${PACKAGE_DIR}/QGroundControl-${QGC_VERSION}-multiabi.apk"
                            '''
                        }
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
            echo 'Build failed - check the androiddeployqt/Gradle output above.'
        }
        cleanup {
            // Drop the GStreamer symlink so a workspace wipe never chases it
            // into /opt. Never touch a real directory.
            sh 'GST_LINK="${WORKSPACE}/gstreamer-1.0-android-universal-${GSTREAMER_VERSION}"; [ -L "${GST_LINK}" ] && rm -f "${GST_LINK}"; true'
        }
    }
}
