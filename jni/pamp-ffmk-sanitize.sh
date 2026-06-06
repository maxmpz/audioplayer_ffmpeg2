#!/bin/bash
# Sanitize an FFmpeg lib Makefile so ndk-build (av.mk) can `include` it for the OBJS lists.
# FFmpeg 8.x split libs into per-subdir Makefiles (e.g. libavcodec/{aac,opus,bsf}/Makefile,
# libavfilter/{dnn,vulkan}/Makefile) that the parent lib Makefile `include`s. Each subdir Makefile
# begins with a `clean::` (double-colon) rule, which collides with ndk-build's single-colon `clean:`
# (GNU make forbids mixing `:` and `::` for one target -> "target file 'clean' has both : and :: entries").
#
# This emits the lib Makefile with:
#   - the `include $(SRC_PATH)/<lib>/<sub>/Makefile` lines inlined (so the subdir OBJS-$(CONFIG_*) survive)
#   - all `clean::` rules (rule line + its tab-indented recipe) stripped
# The subdir Makefiles are flat (no nested includes), but inlining is recursive just in case.
#
# Usage: pamp-ffmk-sanitize.sh <lib_makefile> <SRC_PATH> <ARCH>
MK="$1"; SRC="$2"; ARCH="$3"
awk -v SRC="$SRC" -v ARCH="$ARCH" '
	function emit(f,  l){ while((getline l < f) > 0) handle(l); close(f) }
	function handle(line) {
		# skip the tab-indented recipe lines following a clean:: rule
		if (skip) { if (line ~ /^\t/ || line == "") return; skip=0 }
		if (line ~ /^clean::/) { skip=1; return }
		if (line ~ /^-?include \$\(SRC_PATH\)\//) {
			p=line; sub(/^-?include[ \t]+/, "", p);
			gsub(/\$\(SRC_PATH\)/, SRC, p); gsub(/\$\(ARCH\)/, ARCH, p);
			emit(p); return
		}
		print line
	}
	{ handle($0) }
' "$MK"
