# SPDX-License-Identifier: GPL-2.0-or-later

#  Copyright (C) 2023 Synopsys, Inc.
#  Shahab Vahedi <shahab@synopsys.com>

source [find cpu/arc/v3_32.tcl]

proc arc_hs5_examine_target { target } {
	# Will set current target for us.
	arc_v3_examine_target $target
}

proc arc_hs5_init_regs { } {
	arc_v3_init_regs

	[target current] configure \
		-event examine-end "arc_hs5_examine_target [target current]"
}

# Scripts in "target" folder should call this function instead of direct
# invocation of arc_common_reset.
proc arc_hs5_reset { {target ""} } {
	arc_v3_reset $target
}
