#if PAMP_OPTIMIZE_MACROS
#pragma GCC optimize ("Os")
//#pragma GCC optimize ("-funroll-loops")
//#pragma GCC optimize ("-funroll-all-loops")
//#pragma GCC optimize ("-ftree-vectorize")
//#pragma GCC optimize ("-fno-tree-vectorize")
//#pragma GCC optimize ("-ftree-loop-if-convert-stores")
//#pragma GCC optimize ("-fno-tree-loop-if-convert")
#endif

#include "libavutil/avassert.h"
#include "../FFMpeg/libavcodec/mpegaudiodsp_float.c"

