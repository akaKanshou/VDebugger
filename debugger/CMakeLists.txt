set(SRC_FILES 
    main.c 
    debugger.c
    cmdline.c 
    registers.c 
    dwarf_info.c
)

add_executable(vdb ${SRC_FILES})
target_compile_options(vdb PRIVATE -O0)
target_link_libraries(vdb PRIVATE linenoise)
target_link_libraries(vdb PRIVATE cmap)
target_link_libraries(vdb PRIVATE libdwarf.so)