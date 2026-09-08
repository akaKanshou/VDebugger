#include "dwarf_info.h"

// TODO: Add descriptive error handling

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "libdwarf-2/dwarf.h"
#include "libdwarf-2/libdwarf.h"

int init_dwarf(const char *path, Dwarf_Debug *dwarf_dbg) {
    static char true_path[FILENAME_MAX];
    unsigned int true_pathlen = FILENAME_MAX;
    Dwarf_Handler errhand = 0;
    Dwarf_Ptr errarg = 0;
    Dwarf_Error error = 0;
    int res = 0;

    res = dwarf_init_path(path, true_path, true_pathlen, DW_GROUPNUMBER_ANY,
                          errhand, errarg, dwarf_dbg, &error);

    if (res == DW_DLV_ERROR) {
        dwarf_dealloc_error(*dwarf_dbg, error);
        return -1;
    } else if (res == DW_DLV_NO_ENTRY) {
        return -1;
    }

    return 0;
}

int die_contains_addr_offset(WORD addr_offset, Dwarf_Debug dwarf_dbg,
                             Dwarf_Die die, bool *contains_addr,
                             Dwarf_Error *error) {

    int res = 0;
    Dwarf_Addr low_pc = 0;
    res = dwarf_lowpc(die, &low_pc, error);
    if (res != DW_DLV_OK) {
        return res;
    }

    if (low_pc > addr_offset) {
        *contains_addr = false;
        return res;
    }

    Dwarf_Addr localhighpc = 0;
    Dwarf_Half form = 0;
    enum Dwarf_Form_Class formclass = DW_FORM_CLASS_UNKNOWN;

    res = dwarf_highpc_b(die, &localhighpc, &form, &formclass, error);
    if (res != DW_DLV_OK) {
        return res;
    }

    if (form != DW_FORM_addr && !dwarf_addr_form_is_indexed(form)) {
        localhighpc += low_pc;
    }

    if (localhighpc <= addr_offset) {
        *contains_addr = false;
        return res;
    }

    *contains_addr = true;
    return res;
}

int get_cu_from_addr(WORD addr_offset, Dwarf_Debug dwarf_dbg,
                     Dwarf_Die *res_cu_die, Dwarf_Error *error) {
    Dwarf_Unsigned abbrev_offset = 0;
    Dwarf_Half address_size = 0;
    Dwarf_Half version_stamp = 0;
    Dwarf_Half offset_size = 0;
    Dwarf_Half extension_size = 0;
    Dwarf_Sig8 signature;
    Dwarf_Unsigned typeoffset = 0;
    Dwarf_Unsigned next_cu_header = 0;
    Dwarf_Half header_cu_type = 0;
    Dwarf_Bool is_info = true;
    int res = 0;

    *res_cu_die = NULL;
    bool contains_addr = false;

    while (true) {
        Dwarf_Die cu_die = 0;
        Dwarf_Unsigned cu_header_length = 0;

        memset(&signature, 0, sizeof(signature));
        res = dwarf_next_cu_header_e(
            dwarf_dbg, is_info, &cu_die, &cu_header_length, &version_stamp,
            &abbrev_offset, &address_size, &offset_size, &extension_size,
            &signature, &typeoffset, &next_cu_header, &header_cu_type, error);
        if (res != DW_DLV_OK) {
            return res;
        }

        res = die_contains_addr_offset(addr_offset, dwarf_dbg, cu_die,
                                       &contains_addr, error);

        if (res == DW_DLV_ERROR) {
            dwarf_dealloc_die(cu_die);
            return res;
        } else if (res == DW_DLV_OK && contains_addr) {
            *res_cu_die = cu_die;
            continue;
        }

        dwarf_dealloc_die(cu_die);
    }

    return res;
}

int get_sub_prog_die_from_addr(WORD addr_offset, Dwarf_Debug dwarf_dbg,
                               Dwarf_Die *sub_prog_die, Dwarf_Error *error) {

    int res = 0;
    *sub_prog_die = 0;

    Dwarf_Die cu_die;
    res = get_cu_from_addr(addr_offset, dwarf_dbg, &cu_die, error);
    if (res == DW_DLV_ERROR) return res;

    res = get_sub_prog_die_in_die_from_addr(addr_offset, dwarf_dbg, cu_die,
                                            sub_prog_die, error);

    dwarf_dealloc_die(cu_die);
    return res;
}

int get_sub_prog_die_in_die_from_addr(WORD addr_offset, Dwarf_Debug dwarf_dbg,
                                      Dwarf_Die die, Dwarf_Die *sub_prog_die,
                                      Dwarf_Error *error) {

    int res = 0;
    Dwarf_Die cur_die = 0;
    res = dwarf_child(die, &cur_die, error);
    if (res != DW_DLV_OK) {
        // Error or DIE has no children
        return res;
    }

    bool contains_addr;
    Dwarf_Half dw_tag;
    Dwarf_Die next_die;
    while (true) {
        res = die_contains_addr_offset(addr_offset, dwarf_dbg, cur_die,
                                       &contains_addr, error);

        if (res == DW_DLV_ERROR) {
            dwarf_dealloc_die(cur_die);
            return res;
        }

        if (res == DW_DLV_OK && contains_addr) {
            res = dwarf_tag(cur_die, &dw_tag, error);

            if (res == DW_DLV_ERROR) {
                dwarf_dealloc_die(cur_die);
                return res;
            }

            if (res == DW_DLV_OK && dw_tag == DW_TAG_subprogram) {
                *sub_prog_die = cur_die;
            }
        }

        res = dwarf_child(cur_die, &next_die, error);
        if (res == DW_DLV_ERROR) {
            dwarf_dealloc_die(cur_die);
            return res;
        }

        if (res == DW_DLV_OK) {
            res = get_sub_prog_die_in_die_from_addr(
                addr_offset, dwarf_dbg, next_die, sub_prog_die, error);

            if (res == DW_DLV_ERROR) {
                dwarf_dealloc_die(cur_die);
                return res;
            }
            dwarf_dealloc_die(next_die);
        }

        res = dwarf_siblingof_c(cur_die, &next_die, error);
        if (*sub_prog_die != cur_die) dwarf_dealloc_die(cur_die);
        cur_die = next_die;

        if (res == DW_DLV_ERROR) {
            return res;
        }

        if (res == DW_DLV_NO_ENTRY) {
            break;
        }
    }
    return res;
}

int get_sub_program_name(Dwarf_Debug dwarf_dbg, Dwarf_Die die,
                         char **dw_at_name_str, Dwarf_Error *error) {

    int res;
    res = dwarf_diename(die, dw_at_name_str, error);
    return res;
}