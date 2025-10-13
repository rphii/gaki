#include "file-info.h"

VEC_IMPLEMENT_BASE(File_Infos, file_infos, File_Info, BY_REF, file_info_free);
VEC_IMPLEMENT_SORT(File_Infos, file_infos, File_Info, BY_REF, file_info_cmp);

int file_info_cmp(File_Info *a, File_Info *b) {
    return so_cmp_s(a->filename, b->filename);
}

void file_info_free(File_Info *a) {
    so_free(&a->filename);
    so_free(&a->content);
}

size_t file_info_rel(File_Info *info) {
    size_t result;
    off_t sz = info->stats.st_size;
    for(result = 1; sz >= result * 1000; result *= 1000) {}
    return result;
}

double file_info_relsize(File_Info *info) {
    size_t factor = file_info_rel(info);
    double result = (double)info->stats.st_size / (double)factor;
    return result;
}

char *file_info_relcstr(File_Info *info) {
    size_t factor = file_info_rel(info);
    switch(factor) {
        case 1ULL: return "B";
        case 1000ULL: return "KiB";
        case 1000000ULL: return "MiB";
        case 1000000000ULL: return "GiB";
        case 1000000000000ULL: return "TiB";
        case 1000000000000000ULL: return "PiB";
        case 1000000000000000000ULL: return "EiB";
        default: break;
    }
    return "??B";
}
