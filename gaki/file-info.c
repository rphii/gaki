#include "file-info.h"
#include "gaki-sync.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize2.h>

//VEC_IMPLEMENT_BASE(File_Infos, file_infos, File_Info, BY_REF, 0);
//VEC_IMPLEMENT_SORT(File_Infos, file_infos, File_Info, BY_REF, file_info_cmp);
LUT_IMPLEMENT(T_File_Info, t_file_info, So, BY_VAL, File_Info, BY_REF, so_hash, so_cmp, 0, file_info_free);

bool file_info_image_thumb(File_Info *info) {
    /* load original image */
    bool result = false;
    char *cpath = so_dup(info->path);
    File_Image img = {0};
    img.data = stbi_load(cpath, &img.w, &img.h, &img.ch, 0);
    if(!img.data) return result;
    if(!img.h || !img.w) return result;
#if true
    /* make thumbnail */
    File_Image rez = { .ch = img.ch };
    rez.w = 200;
    rez.h = round((double)(img.h) / (double)(img.w) * (double)rez.w);
    //printff("\r%u x %u -> %u x %u",img.w,img.h,rez.w,rez.h);usleep(1e6);
    rez.data = malloc(rez.w * rez.h * rez.ch);
    stbir_resize_uint8_linear(img.data, img.w, img.h, 0, rez.data, rez.w, rez.h, 0, rez.ch);
    info->content.graphic.thumb = rez;
    /* free up original image */
    free(img.data);
#else
    info->content.graphic.thumb = img;
#endif
    result = true;
    return result;
}

int file_info_cmp(File_Info *a, File_Info *b) {
    return so_cmp_s(a->path, b->path);
}

void file_info_free(File_Info *a) {
    so_free(&a->path);
    switch(a->stats.st_mode & S_IFMT) {
        case S_IFREG: so_free(&a->content.text); break;
        case S_IFDIR: array_free(a->content.files); break;
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
    path = so_ensure_dir(path);
    File_Info *info = t_file_info_get(&sync->t_file_info, path);
    if(!info) {
        File_Info info_new = {0};
        info_new.path = so_clone(path);
        char *cpath = so_dup(info_new.path);
        info_new.exists = !stat(cpath, &info_new.stats);
        free(cpath);
        if(S_ISREG(info_new.stats.st_mode)) {
            so_filesig(path, &info_new.signature_unsure, &info_new.signature_id);
        }
        T_File_InfoKV *kv = t_file_info_once(&sync->t_file_info, info_new.path, &info_new);
        if(!kv) {
            usleep(1e5);
            printff("unreachable error, lookup table expected: [%.*s] but nothing got", SO_F(info_new.path));
            tui_die("see above error.");
        }
        info = kv->val;
    }
    pthread_rwlock_unlock(&sync->rwl);
    return info;
}


typedef struct Task_File_Info_Image_Cvt {
    File_Info *info;
    Tui_Point dim;
    Tui_Sync_Main *sync_m;
    double ratio_xy;
} Task_File_Info_Image_Cvt;

void *task_file_info_image_cvt_async(Pw *pw, bool *quit, void *void_task) {
    ASSERT_ARG(pw);
    ASSERT_ARG(void_task);

    Task_File_Info_Image_Cvt *task = void_task;

    pthread_mutex_lock(&task->info->mtx);

    File_Graphic *gfx = &task->info->content.graphic;
    tui_buffer_resize(&gfx->cvt_buf, task->dim);
    tui_buffer_clear(&gfx->cvt_buf);

    double ratio_xy = task->ratio_xy ? task->ratio_xy : 1.0;
    Tui_Point dim_rez = task->dim;

    dim_rez.y *= 2;
    if(!dim_rez.y || !dim_rez.x) goto quit;

    File_Image *thumb = &gfx->thumb;
    File_Image rez = {0};
    rez.ch = thumb->ch;
    rez.w = dim_rez.x;
    rez.h = round((double)thumb->h / (double)thumb->w * (double)rez.w * ratio_xy);
    if(rez.h >= dim_rez.y) {
        rez.h = dim_rez.y;
        rez.w = round((double)thumb->w / (double)thumb->h * (double)rez.h / ratio_xy);
    }
    rez.data = malloc(rez.w * rez.h * rez.ch);

    //printff("\r%u x %u -> %u x %u",thumb->w,thumb->h,rez.w,rez.h);usleep(1e6);
    //goto quit;
    stbir_resize_uint8_linear(thumb->data, thumb->w, thumb->h, 0, rez.data, rez.w, rez.h, 0, rez.ch);

    Tui_Buffer_Cache tbc = {0};
    tbc.rect.dim = task->dim;
    tbc.rect.dim.y = 1;
    //tbc.rect.dim.y = 1;
    So pix = so("▀");
    for(size_t y = 0; y < rez.h; y += 2) {
        for(size_t x = 0; x < rez.w; ++x) {
            tbc.bg = 0;
            tbc.fg = 0;
            Tui_Color fg = { .type = TUI_COLOR_RGB };
            Tui_Color bg = { .type = TUI_COLOR_RGB };
            for(size_t ch = 0; ch < rez.ch; ++ch) {
                unsigned char byte = rez.data[(y * rez.w + x) * rez.ch + ch];
                if(ch == 0) fg.r = byte;
                if(ch == 1) fg.g = byte;
                if(ch == 2) fg.b = byte;
                tbc.fg = &fg;
            }
            if(y + 1 < rez.h) {
                for(size_t ch = 0; ch < rez.ch; ++ch) {
                    unsigned char byte = rez.data[((y + 1) * rez.w + x) * rez.ch + ch];
                    if(ch == 0) bg.r = byte;
                    if(ch == 1) bg.g = byte;
                    if(ch == 2) bg.b = byte;
                }
                tbc.bg = &bg;
            }
            tui_buffer_draw_cache(&gfx->cvt_buf, &tbc, pix);
        }
        //tui_buffer_draw_cache(&gfx->cvt_buf, &tbc, so("\n"));
        ++tbc.rect.anc.y;
        tbc.pt.x = 0;
        tbc.pt.y = 0;
    }

    //usleep(1e5);
    //tui_write_so(out);
    //usleep(1e6);

quit:
    gfx->cvt_dim = task->dim;
    pthread_mutex_unlock(&task->info->mtx);

    tui_sync_main_render(task->sync_m);

    free(task);
    return 0;
}

void task_file_info_image_cvt_dispatch(Pw *pw, File_Info *info, Tui_Point dim, Tui_Sync_Main *sync_m, double ratio_cell_xy) {
    ASSERT_ARG(pw);
    ASSERT_ARG(info);

    if(!tui_point_cmp(dim, info->content.graphic.cvt_dim)) return;
    if(!info->content.graphic.thumb.h || !info->content.graphic.thumb.w) return;

    Task_File_Info_Image_Cvt *task;
    NEW(Task_File_Info_Image_Cvt, task);
    task->info = info;
    task->dim = dim;
    task->sync_m = sync_m;
    task->ratio_xy = ratio_cell_xy;
    pw_queue(pw, task_file_info_image_cvt_async, task);
    //usleep(1e5);printff("\rqueue..");usleep(1e5);
}

