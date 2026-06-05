#include "id3v2.h"
#if PAMP_CONFIG_NO_TAGS
// Pamp change: in 6.1 the demux-time ff_id3v2_read_dict() call moved from utils.c to demux.c
// (avformat_open_input). Redirect it to ff_id3v2_read_dict2() so PAMP_AVFMT_FLAG_SKIP_TAGS is honored.
#define ff_id3v2_read_dict(...) ff_id3v2_read_dict2(s, __VA_ARGS__)
#endif

#include "../FFmpeg/libavformat/demux.c"
