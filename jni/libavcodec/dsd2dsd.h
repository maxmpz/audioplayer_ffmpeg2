/*
 * Raw DSD repacking for the (PAMP-overridden) ffmpeg DSD/DST decoders.
 *
 * Produces PA_SAMPLE_FMT_DSD_RAW_32_MSB: interleaved stereo where each 32-bit word is
 * 4 consecutive DSD bytes of a channel, MSB-first within each byte, laid out as
 *   [L b0][L b1][L b2][L b3][R b0][R b1][R b2][R b3] ...
 * This is exactly the native MSBF DSD byte stream interleaved in 4-byte groups, matching
 * the p2d_cifb.c producer and the usbx output consumer bit-for-bit.
 *
 * This header declares the helpers; dsd2dsd.c defines dsd2dsd_translate(). Both are
 * #included directly by the cloned dsddec.c / dstdec.c (they are not built standalone),
 * so the symbols are static per translation unit.
 *
 * Requires ff_reverse[] (libavutil/reverse.h in 6.x; was libavcodec/mathops.h in 4.2) to be in
 * scope - the including decoder already pulls it in.
 */
#ifndef AVCODEC_DSD2DSD_H
#define AVCODEC_DSD2DSD_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "libavutil/attributes.h"
#include "libavutil/reverse.h" /* ff_reverse[] — moved here from libavcodec/mathops.h in FFmpeg 6.x */

/** MSB-first DSD silence byte (== DSD_SILENCE in dsddec.c, P2D_DSD_SILENCE_BYTE_MSB) */
#define DSD2DSD_SILENCE_BYTE_MSB 0x69

#if defined(__ARM_ACLE)
#include <arm_acle.h>
#endif

/**
 * Reverse the 8 bits within each byte of a 32-bit word, preserving byte order.
 * ARM: RBIT (reverse all 32 bit positions) + REV (byteswap) leaves each byte's bits
 * reversed in place - two single-cycle ops. Portable fallback: SWAR that swaps bits only
 * within each byte (never crossing byte boundaries), so it is endian-independent.
 */
static av_always_inline uint32_t dsd2dsd_rev_bits_in_bytes(uint32_t x)
{
#if defined(__ARM_ACLE)
    return __rev(__rbit(x));
#else
    x = ((x & 0x55555555u) << 1) | ((x & 0xAAAAAAAAu) >> 1);
    x = ((x & 0x33333333u) << 2) | ((x & 0xCCCCCCCCu) >> 2);
    x = ((x & 0x0F0F0F0Fu) << 4) | ((x & 0xF0F0F0F0u) >> 4);
    return x;
#endif
}

/** Normalize one DSD byte to MSB-first ordering (bit-reverse if the source is LSB-first). */
static av_always_inline uint8_t dsd2dsd_msb_byte(uint8_t b, int lsbf)
{
    return lsbf ? ff_reverse[b] : b;
}

/* dsd2dsd_translate() is defined in dsd2dsd.c (included by the decoders). */

#endif /* AVCODEC_DSD2DSD_H */
