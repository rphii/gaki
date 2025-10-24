#include "file-info.h"
#include "gaki-sync.h"

VEC_IMPLEMENT_BASE(File_Infos, file_infos, File_Info, BY_REF, 0);
VEC_IMPLEMENT_SORT(File_Infos, file_infos, File_Info, BY_REF, file_info_cmp);
LUT_IMPLEMENT(T_File_Info, t_file_info, So, BY_VAL, File_Info, BY_REF, so_hash, so_cmp, 0, file_info_free);

int file_info_cmp(File_Info *a, File_Info *b) {
    return so_cmp_s(a->path, b->path);
}

void file_info_free(File_Info *a) {
    so_free(&a->path);
    switch(a->stats.st_mode & S_IFMT) {
        case S_IFREG: so_free(&a->content.text); break;
        case S_IFDIR: file_infos_free(a->content.files); break;
        default: break;
    }
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

File_Info *file_info_ensure(Gaki_Sync_T_File_Info *sync, So path) {
    pthread_rwlock_wrlock(&sync->rwl);
    File_Info *info = t_file_info_get(&sync->t_file_info, path);
    if(!info) {
        File_Info info_new = {0};
        info_new.path = so_clone(path);
        char *cpath = so_dup(path);
        stat(cpath, &info_new.stats);
        free(cpath);
        info = t_file_info_once(&sync->t_file_info, info_new.path, &info_new)->val;
    }
    pthread_rwlock_unlock(&sync->rwl);
    return info;
}


