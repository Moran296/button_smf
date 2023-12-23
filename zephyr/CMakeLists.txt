# SPDX-License-Identifier: Apache-2.0

if (CONFIG_BUTTON_SMF)

zephyr_library()
zephyr_include_directories(.)
zephyr_library_sources(button_smf.c)

endif() # CONFIG_BUTTON_SMF
