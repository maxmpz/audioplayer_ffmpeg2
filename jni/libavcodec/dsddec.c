/*
 * Direct Stream Digital (DSD) decoder
 * based on BSD licensed dsd2pcm by Sebastian Gesemann
 * Copyright (c) 2009, 2011 Sebastian Gesemann. All rights reserved.
 * Copyright (c) 2014 Peter Ross
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Direct Stream Digital (DSD) decoder
 *
 * PAMP override: adds optional raw DSD passthrough (PA_SAMPLE_FMT_DSD_RAW_32_MSB) when the
 * Poweramp wrapper requests it via AVCodecContext.opaque (see pa_ff_dsd.h). In raw mode the
 * decoder emits interleaved stereo 32-bit DSD words instead of decoding to PCM float; the
 * ffmpeg byte-rate sample_rate is left untouched (wrapper/ffmpeg timing is pts/packet based).
 */

#include "libavutil/mem.h"

#include "avcodec.h"
#include "codec_internal.h"
#include "decode.h"
#include "dsd.h"

#if PAMP_CHANGES
#include "libavutil/channel_layout.h"
#include "pa_ff_dsd.h"
#include "dsd2dsd.c"
#endif

#define DSD_SILENCE 0x69
#define DSD_SILENCE_REVERSED 0x96
/* 0x69 = 01101001
 * This pattern "on repeat" makes a low energy 352.8 kHz tone
 * and a high energy 1.0584 MHz tone which should be filtered
 * out completely by any playback system --> silence
 */

#if PAMP_CHANGES
typedef struct PADSDContext {
    int src_channels;   /**< original channel count (avctx->ch_layout is forced to stereo in raw mode) */
    int raw;            /**< raw DSD passthrough engaged */
    DSDContext ch[];    /**< per-channel dsd2pcm filter state (PCM mode only) */
} PADSDContext;
#endif

static av_cold int decode_init(AVCodecContext *avctx)
{
#if PAMP_CHANGES
    PADSDContext *c;
    int i;
    uint8_t silence;
    int src_channels = avctx->ch_layout.nb_channels;
    PaFFDsdSideChannel *sc;

    if (!src_channels)
        return AVERROR_INVALIDDATA;

    ff_init_dsd_data();

    c = av_mallocz(sizeof(*c) + (size_t)src_channels * sizeof(DSDContext));
    if (!c)
        return AVERROR(ENOMEM);
    c->src_channels = src_channels;

    silence = avctx->codec_id == AV_CODEC_ID_DSD_LSBF || avctx->codec_id == AV_CODEC_ID_DSD_LSBF_PLANAR ? DSD_SILENCE_REVERSED : DSD_SILENCE;
    for (i = 0; i < src_channels; i++) {
        c->ch[i].pos = 0;
        memset(c->ch[i].buf, silence, sizeof(c->ch[i].buf));
    }

    avctx->priv_data = c;

    sc = avctx->opaque;
    if (sc && sc->magic == PA_FF_DSD_MAGIC && sc->request_dsd_raw) {
        /* Raw DSD passthrough: always interleaved stereo 32-bit DSD words.
         * NOTE: leave avctx->sample_rate at the ffmpeg byte rate - timing is pts/packet
         * based and overriding it would break the wrapper's sample_rate*8 UI calc. */
        c->raw = 1;
        sc->out_sample_rate = -(avctx->sample_rate * 8); /* negative DSD bit rate (PA convention) */
        sc->dsd_raw_active  = 1;
        av_channel_layout_uninit(&avctx->ch_layout);
        avctx->ch_layout    = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
        avctx->sample_fmt   = AV_SAMPLE_FMT_FLT;
    } else {
        avctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
    return 0;
#else
    DSDContext * s;
    int i;
    uint8_t silence;

    if (!avctx->ch_layout.nb_channels)
        return AVERROR_INVALIDDATA;

    ff_init_dsd_data();

    s = av_malloc_array(avctx->ch_layout.nb_channels, sizeof(*s));
    if (!s)
        return AVERROR(ENOMEM);

    silence = avctx->codec_id == AV_CODEC_ID_DSD_LSBF_PLANAR ||
              avctx->codec_id == AV_CODEC_ID_DSD_LSBF ? DSD_SILENCE_REVERSED : DSD_SILENCE;
    for (i = 0; i < avctx->ch_layout.nb_channels; i++) {
        s[i].pos = 0;
        memset(s[i].buf, silence, sizeof(s[i].buf));
    }

    avctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    avctx->priv_data  = s;
    return 0;
#endif
}

typedef struct ThreadData {
    AVFrame *frame;
    const AVPacket *avpkt;
} ThreadData;

static int dsd_channel(AVCodecContext *avctx, void *tdata, int j, int threadnr)
{
    int lsbf = avctx->codec_id == AV_CODEC_ID_DSD_LSBF || avctx->codec_id == AV_CODEC_ID_DSD_LSBF_PLANAR;
#if PAMP_CHANGES
    PADSDContext *c = avctx->priv_data;
    DSDContext *s = c->ch;
    int nb_channels = c->src_channels;
#else
    DSDContext *s = avctx->priv_data;
    int nb_channels = avctx->ch_layout.nb_channels;
#endif
    ThreadData *td = tdata;
    AVFrame *frame = td->frame;
    const AVPacket *avpkt = td->avpkt;
    int src_next, src_stride;
    float *dst = ((float **)frame->extended_data)[j];

    if (avctx->codec_id == AV_CODEC_ID_DSD_LSBF_PLANAR || avctx->codec_id == AV_CODEC_ID_DSD_MSBF_PLANAR) {
        src_next   = frame->nb_samples;
        src_stride = 1;
    } else {
        src_next   = 1;
        src_stride = nb_channels;
    }

    ff_dsd2pcm_translate(&s[j], frame->nb_samples, lsbf,
                         avpkt->data + j * src_next, src_stride,
                         dst, 1);

    return 0;
}

static int decode_frame(AVCodecContext *avctx, AVFrame *frame,
                        int *got_frame_ptr, AVPacket *avpkt)
{
    ThreadData td;
    int ret;
#if PAMP_CHANGES
    PADSDContext *c = avctx->priv_data;
    int src_channels = c->src_channels;
    int bytes_per_ch = avpkt->size / src_channels;

    if (c->raw) {
        /* Interleaved stereo 32-bit DSD words: [L s32][R s32] (PA_SAMPLE_FMT_DSD_RAW_32_MSB). */
        int lsbf = avctx->codec_id == AV_CODEC_ID_DSD_LSBF || avctx->codec_id == AV_CODEC_ID_DSD_LSBF_PLANAR;
        int src_next, src_stride;
        uint8_t *out;
        const unsigned char *rsrc;

        if (avctx->codec_id == AV_CODEC_ID_DSD_LSBF_PLANAR || avctx->codec_id == AV_CODEC_ID_DSD_MSBF_PLANAR) {
            src_next   = bytes_per_ch;
            src_stride = 1;
        } else {
            src_next   = 1;
            src_stride = src_channels;
        }

        frame->nb_samples = (bytes_per_ch + 3) / 4; /* 32-bit DSD words per channel */
        if ((ret = ff_get_buffer(avctx, frame, 0)) < 0) /* stereo, interleaved FLT */
            return ret;

        out = frame->extended_data[0];
        /* Left = channel 0 */
        dsd2dsd_translate(bytes_per_ch, lsbf, avpkt->data + 0 * src_next, src_stride, out + 0, 8);
        /* Right = channel 1, or duplicate channel 0 for mono */
        rsrc = (src_channels >= 2) ? avpkt->data + 1 * src_next : avpkt->data + 0 * src_next;
        dsd2dsd_translate(bytes_per_ch, lsbf, rsrc, src_stride, out + 4, 8);

        *got_frame_ptr = 1;
        return avpkt->size;
    }

    frame->nb_samples = bytes_per_ch;

    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    td.frame = frame;
    td.avpkt = avpkt;
    avctx->execute2(avctx, dsd_channel, &td, NULL, src_channels);

    *got_frame_ptr = 1;
    return frame->nb_samples * src_channels;
#else
    frame->nb_samples = avpkt->size / avctx->ch_layout.nb_channels;

    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    td.frame = frame;
    td.avpkt = avpkt;
    avctx->execute2(avctx, dsd_channel, &td, NULL, avctx->ch_layout.nb_channels);

    *got_frame_ptr = 1;
    return frame->nb_samples * avctx->ch_layout.nb_channels;
#endif
}

#if PAMP_CHANGES
/* raw mode also needs AV_SAMPLE_FMT_FLT (interleaved stereo DSD-as-float frame) */
#define DSD_DECODER(id_, name_, long_name_) \
const FFCodec ff_ ## name_ ## _decoder = { \
    .p.name       = #name_, \
    CODEC_LONG_NAME(long_name_), \
    .p.type       = AVMEDIA_TYPE_AUDIO, \
    .p.id         = AV_CODEC_ID_##id_, \
    .init         = decode_init, \
    FF_CODEC_DECODE_CB(decode_frame), \
    .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_SLICE_THREADS, \
    .p.sample_fmts = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_FLTP, \
                                                   AV_SAMPLE_FMT_FLT, \
                                                   AV_SAMPLE_FMT_NONE }, \
};
#else
#define DSD_DECODER(id_, name_, long_name_) \
const FFCodec ff_ ## name_ ## _decoder = { \
    .p.name       = #name_, \
    CODEC_LONG_NAME(long_name_), \
    .p.type       = AVMEDIA_TYPE_AUDIO, \
    .p.id         = AV_CODEC_ID_##id_, \
    .init         = decode_init, \
    FF_CODEC_DECODE_CB(decode_frame), \
    .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_SLICE_THREADS, \
};
#endif

DSD_DECODER(DSD_LSBF, dsd_lsbf, "DSD (Direct Stream Digital), least significant bit first")
DSD_DECODER(DSD_MSBF, dsd_msbf, "DSD (Direct Stream Digital), most significant bit first")
DSD_DECODER(DSD_MSBF_PLANAR, dsd_msbf_planar, "DSD (Direct Stream Digital), most significant bit first, planar")
DSD_DECODER(DSD_LSBF_PLANAR, dsd_lsbf_planar, "DSD (Direct Stream Digital), least significant bit first, planar")
