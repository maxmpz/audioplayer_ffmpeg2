// NOTE: copied/moved by pamp-config.sh

#if HAVE_ARMV8
#	if HAVE_PA_MIN_MODE
#		include "protocol_list-arm64-min.c"
#	else
#		include "protocol_list-arm64.c"
#	endif
#elif HAVE_NEON
#	if HAVE_PA_MIN_MODE
#		include "protocol_list-arm32-min.c"
#	else
#		include "protocol_list-arm32.c"
#	endif
#else
#	error
#endif
