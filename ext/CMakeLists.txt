add_library(linenoise linenoise/linenoise.c)
target_include_directories(linenoise INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/linenoise)