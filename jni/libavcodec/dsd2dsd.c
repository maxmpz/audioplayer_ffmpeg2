/*
 * Raw DSD repacking - see dsd2dsd.h. Stateless. #included by the cloned dsddec.c / dstdec.c.
 */
#include "dsd2dsd.h"

/**
 * Pack `bytes` DSD bytes of one channel (read from src at src_stride) into MSB-first 32-bit
 * words at dst: 4 contiguous bytes per word, advancing dst by dst_stride bytes per word
 * (dst_stride == 8 for an interleaved stereo [L s32][R s32] frame). The bytes are normalized
 * to MSB-first ordering (bit-reversed when lsbf != 0). A trailing partial word is padded with
 * DSD silence (0x69). No per-channel state.
 */
static void dsd2dsd_translate(size_t bytes, int lsbf,
                              const unsigned char *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride)
{
    size_t full_words = bytes >> 2;   /* complete 4-byte words */
    size_t rem        = bytes & 3u;   /* leftover bytes (0..3) */
    size_t w          = 0;

    if (src_stride == 1) {
        /* Fast path: contiguous (planar) source, 4 bytes at a time. Endian-independent:
         * the load + per-byte-reverse + store round-trips byte positions, and the no-reverse
         * case is a verbatim byte copy. RBIT/REV on ARM, SWAR elsewhere. */
        for (; w < full_words; w++) {
            uint32_t v;
            memcpy(&v, src, 4);
            src += 4;
            if (lsbf)
                v = dsd2dsd_rev_bits_in_bytes(v);
            memcpy(dst + (ptrdiff_t)w * dst_stride, &v, 4);
        }
    } else {
        for (; w < full_words; w++) {
            uint8_t *d = dst + (ptrdiff_t)w * dst_stride;
            d[0] = dsd2dsd_msb_byte(src[0 * src_stride], lsbf);
            d[1] = dsd2dsd_msb_byte(src[1 * src_stride], lsbf);
            d[2] = dsd2dsd_msb_byte(src[2 * src_stride], lsbf);
            d[3] = dsd2dsd_msb_byte(src[3 * src_stride], lsbf);
            src += 4 * src_stride;
        }
    }

    if (rem) {
        uint8_t *d = dst + (ptrdiff_t)w * dst_stride;
        int k;
        for (k = 0; k < 4; k++) {
            if ((size_t)k < rem) {
                d[k] = dsd2dsd_msb_byte(*src, lsbf);
                src += src_stride;
            } else {
                d[k] = DSD2DSD_SILENCE_BYTE_MSB;
            }
        }
    }
}
