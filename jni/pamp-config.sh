#!/bin/bash

# NOTE:
# - it may be required to rebuild mbedtls with the same toolchain before configuring due to possible clang/llvm issues with unmatched builds
# - we can generate multiple non-conflicting configs and select them in compile time via overridden config-pamp.h and other *-pamp.* files

TARGET_CONFIG_SUFFIX=$1

if [[ $1 ==  'arm64' ]] ; then
	echo "Config: arm64"
	echo
	
elif [[ $1 == 'arm32' ]] ; then
	echo "Config: arm32"
	echo

elif [[ $1 ==  'arm64-min' ]] ; then
	echo "Config: arm64-min"
	echo
	
elif [[ $1 == 'arm32-min' ]] ; then
	echo "Config: arm32-min"
	echo

else 
    echo "Usage: pamp-config.sh arm64|arm64-min|arm32|arm32-min"
    echo
    exit 1
fi

FFMPEG_PATH=../FFmpeg
NDK_PATH=/Users/maksimpetrov/Library/Android/sdk/ndk/30.0.14904198
LOCAL_PATH=$PWD
MIN=
NM_PREFIX=llvm-
AR_PREFIX=llvm-
RANLIB_PREFIX=llvm-
STRIP_PREFIX=llvm-
MBEDTLS_PATH=$LOCAL_PATH/../thirdparty/mbedtls
# Ensure we don't pull stuff from the environment
CPP_FLAGS=
LD_FLAGS=
PREBUILT=$NDK_PATH/toolchains/llvm/prebuilt/darwin-x86_64
PLATFORM=$PREBUILT/sysroot
GCC=clang
FF_FLAGS=

echo LOCAL_PATH=$LOCAL_PATH



if [[ $1 ==  'arm64-min' ]] || [[ $1 == 'arm32-min' ]]; then
	MIN=1
fi


if [[ $1 ==  'arm64' ]] || [[ $1 ==  'arm64-min' ]]; then
	FFMPEG_ARCH=aarch64
  GCC_PREFIX=aarch64-linux-android21-

	ARM_FF_FLAGS="-march=armv8-a+simd -O3 -D_NDK_MATH_NO_SOFTFP=1 "

	LDFLAGS=" \
		-Wl,--no-undefined -Wl,-z,noexecstack \
		-llog -lz -lc \
		-Wl,--no-warn-mismatch -lm \
		-L$MBEDTLS_PATH/crypto/build/arm64-v8a \
		-L$MBEDTLS_PATH/build/arm64-v8a \
		"
	
elif [[ $1 == 'arm32' ]] || [[ $1 ==  'arm32-min' ]]; then
	FFMPEG_ARCH=arm
	GCC_PREFIX=armv7a-linux-androideabi21-

	ARM_FF_FLAGS="-march=armv7-a -mcpu=cortex-a9 -mfpu=neon-vfpv3 -Os -mfloat-abi=softfp"
	FF_FLAGS=--cpu=armv7-a
	LDFLAGS="\
		-Wl,--no-whole-archive \
		-Wl,--no-undefined -Wl,-z,noexecstack \
		-llog -lz -lc \
		-Wl,--no-warn-mismatch -lm \
		-L$MBEDTLS_PATH/crypto/build/armeabi-v7a \
    -L$MBEDTLS_PATH/build/armeabi-v7a \
    "
fi

SHARED_CONFIG="\
$FFMPEG_PATH/configure --target-os=linux \
--arch=$FFMPEG_ARCH \
--enable-cross-compile \
--cc=$PREBUILT/bin/${GCC_PREFIX}${GCC} \
--as=$PREBUILT/bin/${GCC_PREFIX}${GCC} \
--cross-prefix=$PREBUILT/bin/${GCC_PREFIX} \
--nm=$PREBUILT/bin/${NM_PREFIX}nm \
--ar=$PREBUILT/bin/${AR_PREFIX}ar \
--ranlib=$PREBUILT/bin/${RANLIB_PREFIX}ranlib \
--strip=$PREBUILT/bin/${STRIP_PREFIX}strip \
--extra-ldflags=\"$LDFLAGS\" \
--enable-small \
--enable-pic \
--enable-zlib \
$FF_FLAGS \
--extra-cflags=\"-DANDROID \
$ARM_FF_FLAGS \
-MMD -MP -fstrict-aliasing -Werror=strict-aliasing -ffunction-sections -funwind-tables -fstack-protector -Wno-psabi -fomit-frame-pointer \
-std=c23 -Wno-sign-compare -Wno-switch -Wno-pointer-sign -ffast-math -Wa,--noexecstack \
-I$MBEDTLS_PATH/include -I$MBEDTLS_PATH/crypto/include\"\
"

MIN_CONFIG="\
--disable-autodetect \
--disable-runtime-cpudetect \
--disable-symver \
--disable-pixelutils \
--disable-doc \
--disable-debug \
--disable-programs \
--disable-avdevice \
--disable-swscale \
--disable-avfilter \
--disable-everything \
--disable-network \
--disable-pthreads \
--disable-faan \
--disable-parser=dirac \
--disable-swresample \
--disable-swscale \
\
--enable-version3 \
--enable-avutil \
\
"
# NOTE (6.1): --enable-rdft/--enable-mdct removed — the legacy FFT/MDCT/RDFT selectables no longer
# exist in 6.1; av_rdft_* (used by milk) still works via the avfft.c shim over libavutil/tx, which is
# part of avutil core and always built.

#--sysinclude=$PLATFORM/usr/include \

# NOTE: we include some demuxers, but not codecs - mpc(8), wv - just to be able to probe format

PA_CONFIG="\
--disable-autodetect \
--disable-runtime-cpudetect \
--disable-symver \
--disable-pixelutils \
--disable-doc \
--disable-debug \
--disable-programs \
--disable-avdevice \
--disable-swscale \
--disable-avfilter \
--disable-everything \
--disable-network \
--disable-pthreads \
--disable-faan \
--disable-parser=dirac \
\
--enable-protocol=file \
--enable-protocol=pipe \
--enable-protocol=data \
\
--enable-version3 \
--enable-mbedtls \
--enable-network \
--enable-protocol=http \
--enable-protocol=https \
--enable-protocol=hls \
\
--enable-bsf=null \
\
--enable-decoder=aac \
--enable-decoder=mp3float \
--enable-decoder=mp2float \
--enable-decoder=mp3on4float \
--enable-decoder=mp3adufloat \
--enable-decoder=vorbis \
--enable-decoder=wmav1 \
--enable-decoder=wmav2 \
--enable-decoder=alac \
--enable-decoder=wmapro \
--enable-decoder=wmalossless \
--enable-decoder=ape \
--enable-decoder=tta \
--enable-decoder=flac \
--enable-decoder=opus \
--enable-decoder=tak \
--enable-decoder=dsd_* \
--enable-decoder=dst \
--enable-decoder=als \
--enable-decoder=mlp \
--enable-decoder=truehd \
--enable-decoder=dca \
--enable-decoder=ac3 \
--enable-decoder=eac3 \
\
--enable-parser=opus \
--enable-parser=flac \
--enable-parser=mlp \
--enable-demuxer=flac \
--enable-parser=vorbis \
--enable-parser=tak \
--enable-demuxer=tak \
--enable-demuxer=dsf \
--enable-demuxer=iff \
--enable-demuxer=matroska \
--enable-demuxer=mpegts \
--enable-demuxer=mpegtsraw \
--enable-demuxer=hls \
--enable-demuxer=ac3 \
--enable-demuxer=eac3 \
--enable-parser=ac3 \
--enable-demuxer=wav \
--enable-demuxer=flv \
--enable-demuxer=live_flv \
--enable-demuxer=mp3 \
--enable-demuxer=mov \
--enable-demuxer=asf \
--enable-demuxer=gsm \
--enable-demuxer=aiff \
--enable-demuxer=ape \
--enable-demuxer=aac \
--enable-demuxer=ogg \
--enable-demuxer=tta \
--enable-parser=aac \
--enable-parser=mpegaudio \
--enable-parser=gsm \
--enable-parser=mlp \
--enable-demuxer=dts \
--enable-demuxer=dtshd \
--enable-parser=dca \
--enable-demuxer=wv \
--enable-demuxer=mpc \
--enable-demuxer=mpc8 \
--enable-demuxer=truehd \
--enable-demuxer=mlp \
\
--enable-decoder=pcm_s8 \
--enable-decoder=pcm_s8_planar \
--enable-decoder=pcm_u8 \
--enable-decoder=pcm_s16be \
--enable-decoder=pcm_s16le \
--enable-decoder=pcm_u16be \
--enable-decoder=pcm_u16le \
--enable-decoder=pcm_s16be_planar \
--enable-decoder=pcm_s16le_planar \
--enable-decoder=pcm_f16le \
--enable-decoder=pcm_f24le \
--enable-decoder=pcm_f64le \
--enable-decoder=pcm_f64be \
--enable-decoder=pcm_s24be \
--enable-decoder=pcm_s24daud \
--enable-decoder=pcm_s24le \
--enable-decoder=pcm_s24le_planar \
--enable-decoder=pcm_u24be \
--enable-decoder=pcm_u24le \
--enable-decoder=pcm_s32be \
--enable-decoder=pcm_s32le \
--enable-decoder=pcm_u32be \
--enable-decoder=pcm_u32le \
--enable-decoder=pcm_f32be \
--enable-decoder=pcm_f32le \
--enable-decoder=pcm_s32le_planar \
--enable-decoder=pcm_alaw \
--enable-decoder=pcm_mulaw \
--enable-decoder=pcm_bluray \
--enable-decoder=pcm_lxf \
--enable-decoder=pcm_dvd \
--enable-decoder=pcm_vidc \
\
--enable-decoder=gsm \
--enable-decoder=gsm_ms \
\
--enable-decoder=adpcm_4xm	 \
--enable-decoder=adpcm_adx	 \
--enable-decoder=adpcm_afc	 \
--enable-decoder=adpcm_ct	 \
--enable-decoder=adpcm_dtk	 \
--enable-decoder=adpcm_ea	 \
--enable-decoder=adpcm_ea_maxis_xa \
--enable-decoder=adpcm_ea_r1	 \
--enable-decoder=adpcm_ea_r2	 \
--enable-decoder=adpcm_ea_r3	 \
--enable-decoder=adpcm_ea_xas \
--enable-decoder=adpcm_g722	 \
--enable-decoder=adpcm_g726	 \
--enable-decoder=adpcm_g726le \
--enable-decoder=g723_1 \
--enable-decoder=g729 \
--enable-decoder=adpcm_ima_amv \
--enable-decoder=adpcm_ima_apc \
--enable-decoder=adpcm_ima_dk3 \
--enable-decoder=adpcm_ima_dk4 \
--enable-decoder=adpcm_ima_ea_eacs \
--enable-decoder=adpcm_ima_ea_sead \
--enable-decoder=adpcm_ima_iss \
--enable-decoder=adpcm_ima_oki \
--enable-decoder=adpcm_ima_qt \
--enable-decoder=adpcm_ima_rad \
--enable-decoder=adpcm_ima_smjpeg \
--enable-decoder=adpcm_ima_wav \
--enable-decoder=adpcm_ima_ws \
--enable-decoder=adpcm_ms	 \
--enable-decoder=adpcm_swf	 \
--enable-decoder=adpcm_xa \
--enable-decoder=adpcm_yamaha \
\
--enable-demuxer=pcm_s8 \
--enable-demuxer=pcm_u8 \
--enable-demuxer=pcm_s16be \
--enable-demuxer=pcm_s16le \
--enable-demuxer=pcm_u16be \
--enable-demuxer=pcm_u16le \
--enable-demuxer=pcm_u24be \
--enable-demuxer=pcm_u24le \
--enable-demuxer=pcm_s24be \
--enable-demuxer=pcm_s24le \
--enable-demuxer=pcm_u32be \
--enable-demuxer=pcm_u32le \
--enable-demuxer=pcm_s32be \
--enable-demuxer=pcm_s32le \
--enable-demuxer=pcm_f32be \
--enable-demuxer=pcm_f32le \
--enable-demuxer=pcm_f64be \
--enable-demuxer=pcm_f64le \
--enable-demuxer=pcm_alaw \
--enable-demuxer=pcm_mulaw \
--enable-demuxer=pcm_vidc \
--enable-demuxer=g722 \
--enable-demuxer=g723_1 \
--enable-demuxer=g729 \
\
"

#--enable-protocol=cache \ # NOTE: cache requires file_open.c modification with TMPDIR support + appropirate TMPDIR env setting prior lib loading
#--enable-protocol=async \ # NOTE: doesn't work with hack for neon-hard (requires some work with pthreads inclusion)
# --env='async_protocol_deps=\"\"'\ hack, forcing async without threads dependency (which it doesn't actually require)

rm -f config.h

if [[ "$MIN" == 1 ]]; then
	FULL_CONFIG="$SHARED_CONFIG $MIN_CONFIG"
	#exit 1
	eval $FULL_CONFIG
else
	FULL_CONFIG="$SHARED_CONFIG $PA_CONFIG"
	#exit 1
	eval $FULL_CONFIG
fi


if [ $? -ne 0 ]; then
	exit 1
fi

# Move copy all the generated files
mv ffbuild/config.mak ffbuild/config-${TARGET_CONFIG_SUFFIX}.mak
mv config.h config-${TARGET_CONFIG_SUFFIX}.h
mv config_components.h config_components-${TARGET_CONFIG_SUFFIX}.h # NOTE (6.x): components split out of config.h
mv libavutil/avconfig.h libavutil/avconfig-${TARGET_CONFIG_SUFFIX}.h
rm -f libavfilter/filter_list.c # NOTE: avfilter --disable'd (CONFIG_AVFILTER=0, not built); filter_list unused — drop configure's stray output instead of curating it
mv libavcodec/codec_list.c libavcodec/codec_list-${TARGET_CONFIG_SUFFIX}.c
mv libavcodec/parser_list.c libavcodec/parser_list-${TARGET_CONFIG_SUFFIX}.c
mv libavcodec/bsf_list.c libavcodec/bsf_list-${TARGET_CONFIG_SUFFIX}.c
mv libavformat/demuxer_list.c libavformat/demuxer_list-${TARGET_CONFIG_SUFFIX}.c
#mv libavformat/muxer_list.c libavformat/muxer_list-${TARGET_CONFIG_SUFFIX}.c # NOTE: always nulls now
#mv libavdevice/indev_list.c libavdevice/indev_list-${TARGET_CONFIG_SUFFIX}.c # NOTE: always nulls now
#mv libavdevice/outdev_list.c libavdevice/outdev_list-${TARGET_CONFIG_SUFFIX}.c # NOTE: always nulls now
mv libavformat/protocol_list.c libavformat/protocol_list-${TARGET_CONFIG_SUFFIX}.c

cp config-pamp.h config.h
cp config_components-pamp.h config_components.h # NOTE (6.x): dispatcher for the split-out component config
cp config-pamp.mak ffbuild/config.mak
cp libavutil/avconfig-pamp.h libavutil/avconfig.h
#cp libavfilter/filter_list-pamp.c libavfilter/filter_list.c # NOTE: avfilter disabled — filter_list unused, not curated (see rm above)
cp libavcodec/codec_list-pamp.c libavcodec/codec_list.c
cp libavcodec/parser_list-pamp.c libavcodec/parser_list.c
cp libavcodec/bsf_list-pamp.c libavcodec/bsf_list.c
cp libavformat/demuxer_list-pamp.c libavformat/demuxer_list.c
#cp libavformat/muxer_list-pamp.c libavformat/muxer_list.c
#cp libavdevice/indev_list-pamp.c libavdevice/indev_list.c
#cp libavdevice/outdev_list-pamp.c libavdevice/outdev_list.c
cp libavformat/protocol_list-pamp.c libavformat/protocol_list.c

#mv libavutil/avconfig.h $FFMPEG_PATH/libavutil/

$FFMPEG_PATH/ffbuild/version.sh $FFMPEG_PATH libavutil/ffversion.h # NOTE: creates in jni/libavutil/  

#rm -f "./-ffunction-sections" # remove some trash after configure
#rm -f "./config.h" # remove some trash after configure
#rm -f "./config.mak" # remove some trash after configure
rm -f src

