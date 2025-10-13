#include <dirent.h> 

#include "panel-file.h"

LUT_IMPLEMENT(T_Panel_File, t_panel_file, So, BY_REF, Panel_File, BY_REF, so_hash_p, so_cmp_p, 0, panel_file_free);

void panel_file_free(Panel_File *panel) {

}

int read_dir(So dirname, Panel_File *panel) {
    DIR *d;
    struct dirent *dir;
    char *cdirname = so_dup(dirname);
    char *cfilename = 0;
    d = opendir(cdirname);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_name[0] == '.') continue;
            File_Info info = {0};
            info.filename = so_clone(so_l(dir->d_name));
            so_path_join(&info.path, dirname, info.filename);
            cfilename = so_dup(info.path);
            stat(cfilename, &info.stats);
            file_infos_push_back(&panel->file_infos, &info);
        }
        closedir(d);
    }
    free(cdirname);
    free(cfilename);
    file_infos_sort(&panel->file_infos);
    return(0);
}


void t_panel_file_ensure_exist(T_Panel_File *t, Panel_File **out, So path) {
    Panel_File *panel = t_panel_file_get(t, &path);
    if(!panel) {
        Panel_File panel_new = {0};
        read_dir(path, &panel_new);
        panel = t_panel_file_once(t, &path, &panel_new)->val;
    }
    *out = panel;
}

void panel_file_render(Tui_Buffer *buffer, Tui_Rect rc, Panel_File *panel_file) {
    if(!panel_file) return;
    So *tmp = &panel_file->tmp;
    /* draw file infos */
    Tui_Color sel_bg = { .type = TUI_COLOR_8, .col8 = 7 };
    Tui_Color sel_fg = { .type = TUI_COLOR_8, .col8 = 0 };
    ssize_t dim_y = rc.dim.y;
    rc.dim.y = 1;
    for(size_t i = panel_file->offset; i < file_infos_length(panel_file->file_infos); ++i) {
        if(i >= panel_file->offset + dim_y) break;
        File_Info *info = file_infos_get_at(&panel_file->file_infos, i);
        Tui_Color *fg = info->selected ? &sel_fg : 0;
        Tui_Color *bg = info->selected ? &sel_bg : 0;
        so_clear(tmp);
        so_extend(tmp, info->filename);
        so_push(tmp, '\n');
        tui_buffer_draw(buffer, rc, fg, bg, 0, *tmp);
        ++rc.anc.y;
    }
}


