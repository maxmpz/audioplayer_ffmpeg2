#!/bin/bash

# FFmpeg

export ANDROID_NDK=/Users/maksimpetrov/Library/Android/sdk/ndk/30.0.14904198

echo Using ANDROID_NDK=$ANDROID_NDK

#
PARAMS="PA_NDK_VERSION_MAJOR=30 PA_GLOBAL_FLTO=full PA_UNIFIED_BUILD=true APP_ABI=arm64-v8a PA_GLOBAL_ARCH_MODE=arm64"

echo PARAMS=$PARAMS

$ANDROID_NDK/ndk-build -j16 $PARAMS  $*
