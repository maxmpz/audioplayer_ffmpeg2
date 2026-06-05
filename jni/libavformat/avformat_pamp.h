#pragma once

// Extra AVFormatContext.flag definition

#if PAMP_CONFIG_NO_TAGS
#define PAMP_AVFMT_FLAG_SKIP_TAGS 0x10000000 ///< Skip as much tags parsing as possible
#endif
