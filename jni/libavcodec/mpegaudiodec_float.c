#include "libavutil/log.h"

#undef AV_LOG_ERROR
#define AV_LOG_ERROR AV_LOG_DEBUG // Reduce log spam

#include "../FFMpeg/libavcodec/mpegaudiodec_float.c"
