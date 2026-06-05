#!/bin/bash

echo This is obsolete file, as we use unified ndk build script now running from FFmpeg Android.mk now, cmake is not used anymore
exit 1

pushd ./crypto
./build-arm32.sh
res=$?
popd
echo
if [ $res -ne 0 ]; then
    exit $res
fi

export ANDROID_NDK=/Users/maksimpetrov/Library/Android/sdk/ndk/29.0.14206865
ABI=armeabi-v7a
MINSDKVERSION=21
OTHER_ARGS=

echo Using ANDROID_NDK=$ANDROID_NDK
# NOTE: needed to force cmake to regenerate build scripts
rm -f CMakeCache.txt  
rm -fR build/$ABI
mkdir -p build/$ABI

cmake \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=$ABI \
    -DANDROID_NATIVE_API_LEVEL=$MINSDKVERSION \
    -DENABLE_TESTING=Off \
    -DENABLE_PROGRAMS=Off \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS=-Os \
    
cmake --build . -j8

if [ $? -ne 0 ]; then
    exit $?
fi

mv -v library/*.a build/$ABI/