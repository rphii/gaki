#include <dirent.h> 
#include "file-panel.h"

LUT_IMPLEMENT(T_File_Panel, t_file_panel, So, BY_REF, File_Panel, BY_REF, so_hash_p, so_cmp_p, 0, file_panel_free);

void file_panel_free(File_Panel *panel) {

}

int read_dir(So dirname, File_Panel *panel) {
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


void t_file_panel_ensure_exist(T_File_Panel *t, File_Panel **out, So path) {
    File_Panel *panel = t_file_panel_get(t, &path);
    if(!panel) {
        File_Panel panel_new = {0};
        read_dir(path, &panel_new);
        panel = t_file_panel_once(t, &path, &panel_new)->val;
    }
    *out = panel;
}

