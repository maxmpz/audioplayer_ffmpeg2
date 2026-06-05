#!/bin/bash

export ANDROID_NDK=/Users/maksimpetrov/Library/Android/sdk/ndk/30.0.14904198

echo Using ANDROID_NDK=$ANDROID_NDK

PARAMS="PA_NDK_VERSION_MAJOR=30 PA_GLOBAL_FLTO=full PA_UNIFIED_BUILD=true APP_ABI=armeabi-v7a PA_GLOBAL_ARCH_MODE=arm32"

$ANDROID_NDK/ndk-build -j16 $PARAMS  $*
