add_library(linenoise linenoise/linenoise.c)
target_include_directories(linenoise INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/linenoise)

add_library(cmap hashmap/hashmap.c)
target_include_directories(cmap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/hashmap)