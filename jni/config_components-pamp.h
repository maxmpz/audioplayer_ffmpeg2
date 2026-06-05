// NOTE: config_components.h is overwritten by pamp-config.sh from config_components-pamp.h
// - config_components-pamp.h is the template file which should be edited
//
// New in FFmpeg 6.x: component CONFIG_*_DECODER/_DEMUXER/_PARSER/_BSF/etc. were split out of config.h
// into config_components.h. This dispatcher selects the per-flavor captured file at compile time,
// mirroring config-pamp.h exactly so config.h and config_components.h always stay in lockstep.
// HAVE_ARMV8 / HAVE_NEON / HAVE_PA_MIN_MODE are provided as -D flags by Android.mk.

#if HAVE_ARMV8
#	if HAVE_PA_MIN_MODE
#		include "config_components-arm64-min.h"
#	else
#		include "config_components-arm64.h"
#	endif
#elif HAVE_NEON
#	include "config_components-arm32.h"
#else
	#error
#endif
