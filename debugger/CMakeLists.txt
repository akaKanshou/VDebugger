add_executable(vdb main.c debugger.c cmdline.c registers.c)
target_compile_options(vdb PRIVATE -O0)
target_link_libraries(vdb PRIVATE linenoise)
target_link_libraries(vdb PRIVATE cmap)