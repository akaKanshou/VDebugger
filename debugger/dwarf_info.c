#include "dwarf_info.h"
#include "cmdline.h"

// TODO: Add descriptive error handling
// TODO: Remove need to get version, table version etc on line context

#include <linux/limits.h>
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

int get_low_high_pc_from_die(Dwarf_Debug dwarf_dbg, Dwarf_Die die,
                             Dwarf_Addr *low_pc, Dwarf_Addr *high_pc,
                             Dwarf_Error *error) {
    int res = 0;
    *low_pc = 0;
    res = dwarf_lowpc(die, low_pc, error);
    if (res != DW_DLV_OK) {
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
        localhighpc += *low_pc;
    }

    *high_pc = localhighpc;
    return DW_DLV_OK;
}

int die_contains_addr_offset(WORD addr_offset, Dwarf_Debug dwarf_dbg,
                             Dwarf_Die die, bool *contains_addr,
                             Dwarf_Error *error) {
    int res = 0;

    Dwarf_Addr low_pc, high_pc;
    res = get_low_high_pc_from_die(dwarf_dbg, die, &low_pc, &high_pc, error);

    if (low_pc > addr_offset) {
        *contains_addr = false;
        return res;
    }

    if (high_pc <= addr_offset) {
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

int get_line_context(Dwarf_Debug dwarf_dbg, Dwarf_Die cu_die,
                     Dwarf_Line_Context *line_context, Dwarf_Small *table_count,
                     Dwarf_Unsigned *version, Dwarf_Error *error) {
    int res;

    res = dwarf_srclines_b(cu_die, version, table_count, line_context, error);
    if (res != DW_DLV_OK) {
        return res;
    }

    if (*table_count != 1) {
        // Not supporting experimental two-level line table or empty line table.
        dwarf_srclines_dealloc_b(*line_context);
        return DW_DLV_NO_ENTRY;
    }

    return res;
}

int get_src_lines_from_context(Dwarf_Debug dwarf_dbg, Dwarf_Die cu_die,
                               Dwarf_Line_Context line_context,
                               Dwarf_Line **dw_lines, Dwarf_Signed *line_count,
                               Dwarf_Error *error) {

    int res;

    res = dwarf_srclines_from_linecontext(line_context, dw_lines, line_count,
                                          error);

    if (res != DW_DLV_OK) {
        return res;
    }

    return res;
}

int get_line_no_from_addr(Dwarf_Unsigned addr_offset, Dwarf_Debug dwarf_dbg,
                          Dwarf_Unsigned *line_no, Dwarf_Error *error) {
    Dwarf_Die cu_die;
    int res;

    res = get_cu_from_addr(addr_offset, dwarf_dbg, &cu_die, error);
    if (res == DW_DLV_ERROR) {
        return res;
    }

    Line_Iterator line_iterator;

    res = get_line_iterator(dwarf_dbg, cu_die, &line_iterator, error);
    if (res != DW_DLV_OK) {
        dwarf_dealloc_die(cu_die);
        return res;
    }

    attach_cu_die(&line_iterator, cu_die);

    Dwarf_Signed line_index;

    res = search_addr_in_lines(&line_iterator, addr_offset, &line_index, error);
    if (res != DW_DLV_OK) {
        free_line_iterator(&line_iterator);
        return 1;
    }

    res = dwarf_lineno(line_iterator.dw_lines[line_index], line_no, error);
    free_line_iterator(&line_iterator);
    return res;
}

int search_addr_in_lines(Line_Iterator *line_iterator,
                         Dwarf_Unsigned addr_offset, Dwarf_Signed *line_index,
                         Dwarf_Error *error) {

    int res = 0;
    Dwarf_Signed low = 0, high = line_iterator->line_count - 1, mid;

    Dwarf_Unsigned line_addr;
    while (low <= high) {
        mid = (low + high) / 2;

        res = dwarf_lineaddr(line_iterator->dw_lines[mid], &line_addr, error);

        if (res != DW_DLV_OK) {
            return res;
        }

        if (line_addr > addr_offset) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }

    if (high < 0) {
        return DW_DLV_NO_ENTRY;
    }
    *line_index = high;

    return 0;
}

int get_addr_from_source_line(const char *source_file_name,
                              Dwarf_Unsigned line_num, Dwarf_Debug dwarf_dbg,
                              Dwarf_Addr *line_addr, Dwarf_Error *error) {

    int res;
    Dwarf_Die cu_die;

    res = get_cu_from_file_name(source_file_name, dwarf_dbg, &cu_die, error);
    if (res != DW_DLV_OK) {
        return res;
    }

    Line_Iterator line_iterator;

    res = get_line_iterator(dwarf_dbg, cu_die, &line_iterator, error);
    if (res != DW_DLV_OK) {
        dwarf_dealloc_die(cu_die);
        return res;
    }

    attach_cu_die(&line_iterator, cu_die);

    char *file_name;

    for (Dwarf_Signed i = 0; i < line_iterator.line_count; i++) {
        res = dwarf_linesrc(line_iterator.dw_lines[i], &file_name, error);
        if (res != DW_DLV_OK) {
            free_line_iterator(&line_iterator);
            return res;
        }

        if (strcmp(file_name, source_file_name)) {
            continue;
        }

        Dwarf_Unsigned line_no;
        res = dwarf_lineno(line_iterator.dw_lines[i], &line_no, error);
        if (res == DW_DLV_ERROR) {
            return res;
        } else if (line_no < line_num) {
            continue;
        }

        res = dwarf_lineaddr(line_iterator.dw_lines[i], line_addr, error);

        free_line_iterator(&line_iterator);
        return res;
    }

    free_line_iterator(&line_iterator);
    return DW_DLV_NO_ENTRY;
}

int get_cu_from_file_name(const char *source_file_name, Dwarf_Debug dwarf_dbg,
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
    bool found = false, check;

    while (true) {
        Dwarf_Die cu_die = 0;
        Dwarf_Unsigned cu_header_length = 0;

        memset(&signature, 0, sizeof(signature));
        res = dwarf_next_cu_header_e(
            dwarf_dbg, is_info, &cu_die, &cu_header_length, &version_stamp,
            &abbrev_offset, &address_size, &offset_size, &extension_size,
            &signature, &typeoffset, &next_cu_header, &header_cu_type, error);
        if (res == DW_DLV_ERROR) {
            return res;
        } else if (res == DW_DLV_NO_ENTRY) {
            break;
        }

        char *file_name, *dir_path;
        res = dwarf_die_text(cu_die, DW_AT_name, &file_name, error);
        if (res != DW_DLV_OK) {
            dwarf_dealloc_die(cu_die);
            if (res == DW_DLV_ERROR) {
                return res;
            }
            continue;
        }

        res = dwarf_die_text(cu_die, DW_AT_comp_dir, &dir_path, error);
        if (res != DW_DLV_OK) {
            dwarf_dealloc_die(cu_die);
            if (res == DW_DLV_ERROR) {
                return res;
            }
            continue;
        }

        if (file_name[0] == '/') {
            dir_path = "\0"; // if file is already absolute
            file_name++;
        }
        check = merge_and_check_path(source_file_name, dir_path, file_name);

        if (check) {
            *res_cu_die = cu_die;
            found = true;
            continue;
        }

        dwarf_dealloc_die(cu_die);
    }

    if (found) return DW_DLV_OK;
    return DW_DLV_NO_ENTRY;
}

int get_line_iterator(Dwarf_Debug dwarf_dbg, Dwarf_Die cu_die,
                      Line_Iterator *line_iterator, Dwarf_Error *error) {
    int res;

    line_iterator->cu_die_attach = line_iterator->line_context =
        line_iterator->dw_lines = 0;

    res = get_line_context(dwarf_dbg, cu_die, &line_iterator->line_context,
                           &line_iterator->table_count, &line_iterator->version,
                           error);
    if (res != DW_DLV_OK) {
        return res;
    }

    res = get_src_lines_from_context(
        dwarf_dbg, cu_die, line_iterator->line_context,
        &line_iterator->dw_lines, &line_iterator->line_count, error);
    if (res != DW_DLV_OK) {
        dwarf_srclines_dealloc_b(line_iterator->line_context);
        return res;
    }

    return res;
}

int free_line_iterator(Line_Iterator *line_iterator) {
    if (line_iterator->cu_die_attach != 0) {
        dwarf_dealloc_die(line_iterator->cu_die_attach);
    }
    dwarf_srclines_dealloc_b(line_iterator->line_context);
    return 0;
}

int attach_cu_die(Line_Iterator *line_iterator, Dwarf_Die cu_die) {
    line_iterator->cu_die_attach = cu_die;
    return 0;
}