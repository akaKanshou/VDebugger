#ifndef DWARF_INFO_H
#define DWARF_INFO_H

// TODO: compile submodule
#include "libdwarf-2/dwarf.h"
#include "libdwarf-2/libdwarf.h"

#include <stdbool.h>

typedef long long int WORD;
typedef unsigned long long int UWORD;

// Initialize a Dwarf_Debug object for libdwarf API
// Returns 0 on success, -1 on error.
// If successfully initialized, it is the caller's responsibility to deallocate
// the Dwarf_Debug object with dwarf_finish.
int init_dwarf(const char *path, Dwarf_Debug *dwarf_dbg);

// Returns the DWARF Compilation Unit whose low_pc and high_pc contain
// addr_offset.
// Returns DW_DLV_OK on success, DW_DLV_NO_ENTRY if CU is not found.
// Returns DW_DLV_ERROR on fail.
// If a suitable CU is not found then res_cu_die pointer is NULL.
int get_cu_from_addr(WORD addr_offset, Dwarf_Debug dwarf_dbg,
                     Dwarf_Die *res_cu_die, Dwarf_Error *error);

// Returns the DIE of the sub-program whose low_pc and high_pc contain
// addr_offset.
// Returns DW_DLV_OK on success, DW_DLV_NO_ENTRY if a suitable
// function is not found. Returns DW_DLV_ERROR on fail. If a suitable
// sub-program is not found then sub_prog_die pointer is NULL.
int get_sub_prog_die_from_addr(WORD addr_offset, Dwarf_Debug dwarf_dbg,
                               Dwarf_Die *sub_prog_die, Dwarf_Error *error);

// Returns the DIE of the sub-program whose low_pc and high_pc contain
// addr_offset in the given CU.
// Returns DW_DLV_OK on success, DW_DLV_NO_ENTRY if a suitable
// function is not found. Returns DW_DLV_ERROR on fail. If a suitable
// sub-program is not found then sub_prog_die pointer is NULL.
int get_sub_prog_die_in_die_from_addr(WORD addr_offset, Dwarf_Debug dwarf_dbg,
                                      Dwarf_Die die, Dwarf_Die *sub_prog_die,
                                      Dwarf_Error *error);

// Checks whether given die contains the given addr_offset within its low_pc and
// high_pc.
// Returns DW_DLV_OK on success and populates contains_addr based on the result.
int die_contains_addr_offset(WORD addr_offset, Dwarf_Debug dwarf_dbg,
                             Dwarf_Die die, bool *contains_addr,
                             Dwarf_Error *error);

// Gets the DW_AT_name attribute of the given DIE
// Returns DW_DLV_OK etc and populates dw_at_name_str
// Returns DW_DLV_NO_ENTRY if DIE does not have a DW_AT_name attr.
// Do NOT free the resultant string.
int get_sub_program_name(Dwarf_Debug dwarf_dbg, Dwarf_Die die,
                         char **dw_at_name_str, Dwarf_Error *error);

// Gets the Dwarf Line Context of the given CU.
// Returns DW_DLV_OK etc and populates dw_line_context passed on success.
int get_line_context(Dwarf_Debug dwarf_dbg, Dwarf_Die cu_die,
                     Dwarf_Line_Context *line_context, Dwarf_Small *table_count,
                     Dwarf_Unsigned *version, Dwarf_Error *error);

// Gets the lines from a line context.
// Returns DW_DLV_OK etc. Populates dw_lines on success.
int get_src_lines_from_context(Dwarf_Debug dwarf_dbg, Dwarf_Die cu_die,
                               Dwarf_Line_Context line_context,
                               Dwarf_Line **dw_lines, Dwarf_Signed *line_count,
                               Dwarf_Error *error);

// Get the line number in the source file of the given addr.
// Returns DW_DLV_OK etc.
int get_line_no_from_addr(Dwarf_Unsigned addr_offset, Dwarf_Debug dwarf_dbg,
                          Dwarf_Unsigned *line_no, Dwarf_Error *error);

#endif