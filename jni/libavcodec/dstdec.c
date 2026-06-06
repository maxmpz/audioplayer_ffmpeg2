/*
 * Direct Stream Transfer (DST) decoder
 * Copyright (c) 2014 Peter Ross <pross@xvid.org>
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
 * Direct Stream Transfer (DST) decoder
 * ISO/IEC 14496-3 Part 3 Subpart 10: Technical description of lossless coding of oversampled audio
 *
 * PAMP override: adds optional raw DSD passthrough (PA_SAMPLE_FMT_DSD_RAW_32_MSB) when the
 * Poweramp wrapper requests it via AVCodecContext.opaque (see pa_ff_dsd.h). The DST arithmetic
 * decode runs unchanged over the real (source) channel count into an intermediate DSD buffer;
 * in raw mode channels 0/1 are then packed to interleaved stereo 32-bit DSD words instead of
 * being converted to PCM. avctx->sample_rate is left at the ffmpeg byte rate.
 */

#include "libavutil/intreadwrite.h"
#include "libavutil/mem.h" // PAMP: 8.x include-cleanup dropped the transitive include; explicit for av_mallocz/av_free in the DSD raw additions
#include "libavutil/mem_internal.h"
#include "libavutil/reverse.h"
#include "codec_internal.h"
#include "decode.h"
#include "get_bits.h"
#include "avcodec.h"
#include "golomb.h"
#include "dsd.h"

#if PAMP_CHANGES
#include "libavutil/channel_layout.h"
#include "pa_ff_dsd.h"
#include "dsd2dsd.c"
#endif

#define DST_MAX_CHANNELS 6
#define DST_MAX_ELEMENTS (2 * DST_MAX_CHANNELS)

#define DSD_FS44(sample_rate) (sample_rate * 8LL / 44100)

#define DST_SAMPLES_PER_FRAME(sample_rate) (588 * DSD_FS44(sample_rate))

static const int8_t fsets_code_pred_coeff[3][3] = {
    {  -8 },
    { -16,  8 },
    {  -9, -5, 6 },
};

static const int8_t probs_code_pred_coeff[3][3] = {
    {  -8 },
    { -16,  8 },
    { -24, 24, -8 },
};

typedef struct ArithCoder {
    unsigned int a;
    unsigned int c;
} ArithCoder;

typedef struct Table {
    unsigned int elements;
    unsigned int length[DST_MAX_ELEMENTS];
    int coeff[DST_MAX_ELEMENTS][128];
} Table;

typedef struct DSTContext {
    AVClass *class;

    GetBitContext gb;
    ArithCoder ac;
    Table fsets, probs;
    DECLARE_ALIGNED(16, uint8_t, status)[DST_MAX_CHANNELS][16];
    DECLARE_ALIGNED(16, int16_t, filter)[DST_MAX_ELEMENTS][16][256];
    DSDContext dsdctx[DST_MAX_CHANNELS];

#if PAMP_CHANGES
    int src_channels;   /**< original channel count (avctx->ch_layout is forced to stereo in raw mode) */
    int raw;            /**< raw DSD passthrough engaged */
#endif
} DSTContext;

static av_cold int decode_init(AVCodecContext *avctx)
{
    DSTContext *s = avctx->priv_data;
    int i;
#if PAMP_CHANGES
    PaFFDsdSideChannel *sc;
#endif

    if (avctx->ch_layout.nb_channels > DST_MAX_CHANNELS) {
        avpriv_request_sample(avctx, "Channel count %d", avctx->ch_layout.nb_channels);
        return AVERROR_PATCHWELCOME;
    }

    // the sample rate is only allowed to be 64,128,256 * 44100 by ISO/IEC 14496-3:2005(E)
    // We are a bit more tolerant here, but this check is needed to bound the size and duration
    if (avctx->sample_rate > 512 * 44100)
        return AVERROR_INVALIDDATA;


    if (DST_SAMPLES_PER_FRAME(avctx->sample_rate) & 7) {
        return AVERROR_PATCHWELCOME;
    }

#if PAMP_CHANGES
    s->src_channels = avctx->ch_layout.nb_channels;
#endif

    avctx->sample_fmt = AV_SAMPLE_FMT_FLT;

    for (i = 0; i < avctx->ch_layout.nb_channels; i++)
        memset(s->dsdctx[i].buf, 0x69, sizeof(s->dsdctx[i].buf));

    ff_init_dsd_data();

#if PAMP_CHANGES
    sc = avctx->opaque;
    if (sc && sc->magic == PA_FF_DSD_MAGIC && sc->request_dsd_raw) {
        /* Raw DSD passthrough: always interleaved stereo 32-bit DSD words.
         * sample_rate stays the ffmpeg byte rate (see file header). */
        s->raw = 1;
        sc->out_sample_rate = -(avctx->sample_rate * 8); /* negative DSD bit rate (PA convention) */
        sc->dsd_raw_active  = 1;
        av_channel_layout_uninit(&avctx->ch_layout);
        avctx->ch_layout    = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
        avctx->sample_fmt   = AV_SAMPLE_FMT_FLT;
    }
#endif

    return 0;
}

static int read_map(GetBitContext *gb, Table *t, unsigned int map[DST_MAX_CHANNELS], int channels)
{
    int ch;
    t->elements = 1;
    map[0] = 0;
    if (!get_bits1(gb)) {
        for (ch = 1; ch < channels; ch++) {
            int bits = av_log2(t->elements) + 1;
            map[ch] = get_bits(gb, bits);
            if (map[ch] == t->elements) {
                t->elements++;
                if (t->elements >= DST_MAX_ELEMENTS)
                    return AVERROR_INVALIDDATA;
            } else if (map[ch] > t->elements) {
                return AVERROR_INVALIDDATA;
            }
        }
    } else {
        memset(map, 0, sizeof(*map) * DST_MAX_CHANNELS);
    }
    return 0;
}

static av_always_inline int get_sr_golomb_dst(GetBitContext *gb, unsigned int k)
{
    int v = get_ur_golomb_jpegls(gb, k, get_bits_left(gb), 0);
    if (v && get_bits1(gb))
        v = -v;
    return v;
}

static void read_uncoded_coeff(GetBitContext *gb, int *dst, unsigned int elements,
                               int coeff_bits, int is_signed, int offset)
{
    int i;

    for (i = 0; i < elements; i++) {
        dst[i] = (is_signed ? get_sbits(gb, coeff_bits) : get_bits(gb, coeff_bits)) + offset;
    }
}

static int read_table(GetBitContext *gb, Table *t, const int8_t code_pred_coeff[3][3],
                      int length_bits, int coeff_bits, int is_signed, int offset)
{
    unsigned int i, j, k;
    for (i = 0; i < t->elements; i++) {
        t->length[i] = get_bits(gb, length_bits) + 1;
        if (!get_bits1(gb)) {
            read_uncoded_coeff(gb, t->coeff[i], t->length[i], coeff_bits, is_signed, offset);
        } else {
            int method = get_bits(gb, 2), lsb_size;
            if (method == 3)
                return AVERROR_INVALIDDATA;

            read_uncoded_coeff(gb, t->coeff[i], method + 1, coeff_bits, is_signed, offset);

            lsb_size  = get_bits(gb, 3);
            for (j = method + 1; j < t->length[i]; j++) {
                int c, x = 0;
                for (k = 0; k < method + 1; k++)
                    x += code_pred_coeff[method][k] * (unsigned)t->coeff[i][j - k - 1];
                c = get_sr_golomb_dst(gb, lsb_size);
                if (x >= 0)
                    c -= (x + 4) / 8;
                else
                    c += (-x + 3) / 8;
                if (!is_signed) {
                    if (c < offset || c >= offset + (1<<coeff_bits))
                        return AVERROR_INVALIDDATA;
                }
                t->coeff[i][j] = c;
            }
        }
    }
    return 0;
}

static void ac_init(ArithCoder *ac, GetBitContext *gb)
{
    ac->a = 4095;
    ac->c = get_bits(gb, 12);
}

static av_always_inline void ac_get(ArithCoder *ac, GetBitContext *gb, int p, int *e)
{
    unsigned int k = (ac->a >> 8) | ((ac->a >> 7) & 1);
    unsigned int q = k * p;
    unsigned int a_q = ac->a - q;

    *e = ac->c < a_q;
    if (*e) {
        ac->a  = a_q;
    } else {
        ac->a  = q;
        ac->c -= a_q;
    }

    if (ac->a < 2048) {
        int n = 11 - av_log2(ac->a);
        ac->a <<= n;
        ac->c = (ac->c << n) | get_bits(gb, n);
    }
}

static uint8_t prob_dst_x_bit(int c)
{
    return (ff_reverse[c & 127] >> 1) + 1;
}

static int build_filter(int16_t table[DST_MAX_ELEMENTS][16][256], const Table *fsets)
{
    int i, j, k, l;

    for (i = 0; i < fsets->elements; i++) {
        int length = fsets->length[i];

        for (j = 0; j < 16; j++) {
            int total = av_clip(length - j * 8, 0, 8);

            for (k = 0; k < 256; k++) {
                int64_t v = 0;

                for (l = 0; l < total; l++)
                    v += (((k >> l) & 1) * 2 - 1) * fsets->coeff[i][j * 8 + l];
                if ((int16_t)v != v)
                    return AVERROR_INVALIDDATA;
                table[i][j][k] = v;
            }
        }
    }
    return 0;
}

#if PAMP_CHANGES
/*
 * Emit one decoded DST frame's DSD to the output AVFrame.
 *
 * DSD byte p of channel ch is read from base + ch*ch_mul + p*stride. Two source layouts are fed
 * in: the compressed arithmetic decoder fills an "expanded" buffer (one DSD byte per 4 bytes, so
 * ch_mul=4, stride=channels*4), while an uncompressed frame is packed byte-interleaved straight
 * from the packet (ch_mul=1, stride=channels).
 *
 * Raw mode: pack channels 0/1 (mono duplicated to both) to interleaved stereo 32-bit DSD words.
 * PCM mode: convert each channel to interleaved float via the dsd2pcm filter.
 * DST DSD is always MSB-first (lsbf = 0).
 */
static void dst_emit(DSTContext *s, AVFrame *frame, int channels,
                     const uint8_t *base, ptrdiff_t ch_mul, ptrdiff_t stride, int conv_nb)
{
    if (s->raw) {
        uint8_t *out = frame->extended_data[0];
        const uint8_t *r = base + (channels >= 2 ? 1 : 0) * ch_mul;
        dsd2dsd_translate(conv_nb, 0, base, stride, out + 0, 8);
        dsd2dsd_translate(conv_nb, 0, r,    stride, out + 4, 8);
    } else {
        float *pcm = (float *)frame->data[0];
        int i;
        for (i = 0; i < channels; i++)
            ff_dsd2pcm_translate(&s->dsdctx[i], conv_nb, 0, base + i * ch_mul, stride, pcm + i, channels);
    }
}
#endif

static int decode_frame(AVCodecContext *avctx, AVFrame *frame,
                        int *got_frame_ptr, AVPacket *avpkt)
{
    unsigned samples_per_frame = DST_SAMPLES_PER_FRAME(avctx->sample_rate);
    unsigned map_ch_to_felem[DST_MAX_CHANNELS];
    unsigned map_ch_to_pelem[DST_MAX_CHANNELS];
    unsigned i, ch, same_map, dst_x_bit;
    unsigned half_prob[DST_MAX_CHANNELS];
    DSTContext *s = avctx->priv_data;
#if PAMP_CHANGES
    const int channels = s->src_channels; /* real DSD channel count (avctx->ch_layout is forced to stereo in raw mode) */
#else
    const int channels = avctx->ch_layout.nb_channels;
#endif
    GetBitContext *gb = &s->gb;
    ArithCoder *ac = &s->ac;
    uint8_t *dsd;
#if PAMP_CHANGES
    uint8_t *dsd_tmp = NULL;               /* raw-mode intermediate for the compressed-decode buffer */
    int nb = samples_per_frame / 8;        /* DSD bytes per channel */
#else
    float *pcm;
#endif
    int ret;

    if (avpkt->size <= 1)
        return AVERROR_INVALIDDATA;

#if PAMP_CHANGES
    if (s->raw) {
        frame->nb_samples = (nb + 3) / 4;          /* 32-bit DSD words per channel (stereo output) */
        if ((ret = ff_get_buffer(avctx, frame, 0)) < 0) /* stereo, interleaved FLT */
            return ret;
        dsd = NULL;                                /* compressed path allocates dsd_tmp lazily, after validations */
    } else {
        frame->nb_samples = nb;
        if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
            return ret;
        dsd = frame->data[0];
    }
#else
    frame->nb_samples = samples_per_frame / 8;
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;
    dsd = frame->data[0];
    pcm = (float *)frame->data[0];
#endif

    if ((ret = init_get_bits8(gb, avpkt->data, avpkt->size)) < 0)
        return ret;

    if (!get_bits1(gb)) {
#if PAMP_CHANGES
        /* Uncompressed frame: raw DSD follows the 1-byte header, packed byte-interleaved across
         * channels (DSDIFF order: byte p of channel ch at data[1 + p*channels + ch]).
         *
         * FIX: upstream FFmpeg memcpy'd this *packed* data into the *expanded* (stride-4) buffer
         * and then read it back with the expanded stride, which mismatches the packed source
         * (reads every 4th position, mostly past the copied bytes). Instead convert/pack directly
         * from the packet using the packed stride (= channels), which is correct for both PCM and
         * raw DSD output, and needs no intermediate buffer. */
        size_t avail;
        int per_ch;
        const uint8_t *src = avpkt->data + 1;

        skip_bits1(gb);
        if (get_bits(gb, 6))
            return AVERROR_INVALIDDATA;

        avail  = FFMIN((size_t)avpkt->size - 1, (size_t)nb * channels);
        per_ch = (int)(avail / channels);

        if (s->raw) {
            /* Pre-fill the whole stereo output with DSD silence so a short/truncated packet
             * leaves a clean silent tail beyond the per_ch words we actually pack. */
            memset(frame->extended_data[0], DSD2DSD_SILENCE_BYTE_MSB,
                   (size_t)frame->nb_samples * 2 * 4);
            dst_emit(s, frame, channels, src, 1, channels, per_ch);
        } else {
            dst_emit(s, frame, channels, src, 1, channels, per_ch);
            frame->nb_samples = per_ch; /* only per_ch samples are valid for a short packet */
        }

        *got_frame_ptr = 1;
        return avpkt->size;
#else
        skip_bits1(gb);
        if (get_bits(gb, 6))
            return AVERROR_INVALIDDATA;
        memcpy(frame->data[0], avpkt->data + 1, FFMIN(avpkt->size - 1, frame->nb_samples * channels));
        goto dsd;
#endif
    }

    /* Segmentation (10.4, 10.5, 10.6) */

    if (!get_bits1(gb)) {
        avpriv_request_sample(avctx, "Not Same Segmentation");
        return AVERROR_PATCHWELCOME;
    }

    if (!get_bits1(gb)) {
        avpriv_request_sample(avctx, "Not Same Segmentation For All Channels");
        return AVERROR_PATCHWELCOME;
    }

    if (!get_bits1(gb)) {
        avpriv_request_sample(avctx, "Not End Of Channel Segmentation");
        return AVERROR_PATCHWELCOME;
    }

    /* Mapping (10.7, 10.8, 10.9) */

    same_map = get_bits1(gb);

    if ((ret = read_map(gb, &s->fsets, map_ch_to_felem, channels)) < 0)
        return ret;

    if (same_map) {
        s->probs.elements = s->fsets.elements;
        memcpy(map_ch_to_pelem, map_ch_to_felem, sizeof(map_ch_to_felem));
    } else {
        avpriv_request_sample(avctx, "Not Same Mapping");
        if ((ret = read_map(gb, &s->probs, map_ch_to_pelem, channels)) < 0)
            return ret;
    }

    /* Half Probability (10.10) */

    for (ch = 0; ch < channels; ch++)
        half_prob[ch] = get_bits1(gb);

    /* Filter Coef Sets (10.12) */

    ret = read_table(gb, &s->fsets, fsets_code_pred_coeff, 7, 9, 1, 0);
    if (ret < 0)
        return ret;

    /* Probability Tables (10.13) */

    ret = read_table(gb, &s->probs, probs_code_pred_coeff, 6, 7, 0, 1);
    if (ret < 0)
        return ret;

    /* Arithmetic Coded Data (10.11) */

    if (get_bits1(gb))
        return AVERROR_INVALIDDATA;
    ac_init(ac, gb);

    ret = build_filter(s->filter, &s->fsets);
    if (ret < 0)
        return ret;

#if PAMP_CHANGES
    /* raw mode: all early-return validations passed, allocate the intermediate DSD buffer now */
    if (s->raw) {
        dsd = dsd_tmp = av_mallocz((size_t)nb * channels * 4);
        if (!dsd)
            return AVERROR(ENOMEM);
    }

    memset(s->status, 0xAA, sizeof(s->status));
    memset(dsd, 0, (size_t)nb * 4 * channels);
#else
    memset(s->status, 0xAA, sizeof(s->status));
    memset(dsd, 0, frame->nb_samples * 4 * channels);
#endif

    ac_get(ac, gb, prob_dst_x_bit(s->fsets.coeff[0][0]), &dst_x_bit);

    for (i = 0; i < samples_per_frame; i++) {
        for (ch = 0; ch < channels; ch++) {
            const unsigned felem = map_ch_to_felem[ch];
            int16_t (*filter)[256] = s->filter[felem];
            uint8_t *status = s->status[ch];
            int prob, residual, v;

#define F(x) filter[(x)][status[(x)]]
            const int16_t predict = F( 0) + F( 1) + F( 2) + F( 3) +
                                    F( 4) + F( 5) + F( 6) + F( 7) +
                                    F( 8) + F( 9) + F(10) + F(11) +
                                    F(12) + F(13) + F(14) + F(15);
#undef F

            if (!half_prob[ch] || i >= s->fsets.length[felem]) {
                unsigned pelem = map_ch_to_pelem[ch];
                unsigned index = FFABS(predict) >> 3;
                prob = s->probs.coeff[pelem][FFMIN(index, s->probs.length[pelem] - 1)];
            } else {
                prob = 128;
            }

            ac_get(ac, gb, prob, &residual);
            v = ((predict >> 15) ^ residual) & 1;
            dsd[((i >> 3) * channels + ch) << 2] |= v << (7 - (i & 0x7 ));

            AV_WL64A(status + 8, (AV_RL64A(status + 8) << 1) | ((AV_RL64A(status) >> 63) & 1));
            AV_WL64A(status, (AV_RL64A(status) << 1) | v);
        }
    }

#if PAMP_CHANGES
    /* Compressed frame: decoded into the expanded buffer (one DSD byte per 4 bytes). */
    dst_emit(s, frame, channels, dsd, 4, (ptrdiff_t)channels * 4, nb);
    av_free(dsd_tmp); /* NULL in PCM mode (av_free is a no-op) */

    *got_frame_ptr = 1;

    return avpkt->size;
#else
dsd:
    for (i = 0; i < channels; i++) {
        ff_dsd2pcm_translate(&s->dsdctx[i], frame->nb_samples, 0,
                             frame->data[0] + i * 4,
                             channels * 4, pcm + i, channels);
    }

    *got_frame_ptr = 1;

    return avpkt->size;
#endif
}

const FFCodec ff_dst_decoder = {
    .p.name         = "dst",
    CODEC_LONG_NAME("DST (Digital Stream Transfer)"),
    .p.type         = AVMEDIA_TYPE_AUDIO,
    .p.id           = AV_CODEC_ID_DST,
    .priv_data_size = sizeof(DSTContext),
    .init           = decode_init,
    FF_CODEC_DECODE_CB(decode_frame),
    .p.capabilities = AV_CODEC_CAP_DR1,
};
