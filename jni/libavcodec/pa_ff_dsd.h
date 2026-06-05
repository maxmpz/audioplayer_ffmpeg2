/*
 * Poweramp raw-DSD side channel for the (PAMP-overridden) ffmpeg DSD/DST decoders.
 *
 * Passed via AVCodecContext.opaque to:
 *  - request raw DSD passthrough from dsddec.c / dstdec.c (instead of decoding to PCM), and
 *  - read back the resulting status (whether raw mode engaged + the DSD sample rate).
 *
 * Plain C, no ffmpeg/PA type coupling, so it is shared verbatim between the ffmpeg
 * decoders (libavcodec) and the Poweramp ffmpeg wrapper (ffmpegdecoder2.c). Lives under
 * audioplayer_ffmpeg/jni/libavcodec, which is on the include path of both sides.
 */
#ifndef AVCODEC_PA_FF_DSD_H
#define AVCODEC_PA_FF_DSD_H

#include <stdint.h>
#include <stdbool.h>

/** 'PADS' - guards against a stray (non-ours) non-NULL AVCodecContext.opaque */
#define PA_FF_DSD_MAGIC 0x50414453u

typedef struct PaFFDsdSideChannel {
    uint32_t magic;          /**< must be PA_FF_DSD_MAGIC */

    /* request (wrapper -> decoder), set before avcodec_open2 */
    bool request_dsd_raw;    /**< wrapper can accept raw DSD for this track */

    /* response (decoder -> wrapper), filled in decode_init when raw mode engages */
    bool dsd_raw_active;     /**< decoder switched to raw DSD passthrough (stereo DSD_RAW_32_MSB) */
    int  out_sample_rate;    /**< negative DSD bit rate (PA convention), == -(ffmpeg byte_rate * 8) */
} PaFFDsdSideChannel;

#endif /* AVCODEC_PA_FF_DSD_H */
