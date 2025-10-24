#include "lut2.h"
#include "file-info.h"

LUT2_INCLUDE(T_File_Info, t_file_info, So, BY_VAL, File_Info, BY_REF)
LUT2_IMPLEMENT(T_File_Info, t_file_info, So, BY_VAL, File_Info, BY_REF, so_hash, so_cmp, so_free_v, file_info_free)

int main(void) {
    return 0;
}

