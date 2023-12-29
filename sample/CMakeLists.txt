
# add the path to the button_smf module
list(APPEND EXTRA_ZEPHYR_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../)
set(EXTRA_DTS_DIR ${CMAKE_CURRENT_LIST_DIR}/../zephyr/dts)
message(EXTRA_DTS_DIR=${EXTRA_DTS_DIR})
list(APPEND DTS_ROOT ${EXTRA_DTS_DIR})

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(sample)

target_sources(app PRIVATE src/main.c)
