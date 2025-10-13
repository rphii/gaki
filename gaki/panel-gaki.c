#include "panel-gaki.h"

void panel_gaki_select_up(Panel_Gaki *st, size_t n) {
    if(!st->panel_file) return;
    if(!st->panel_file->select) {
        st->panel_file->select = file_infos_length(st->panel_file->file_infos) - 1;
        st->panel_file->offset = st->panel_file->select + 1 > st->rc_files.dim.y ? st->panel_file->select + 1 - st->rc_files.dim.y : 0;
    } else {
        --st->panel_file->select;
        if(st->panel_file->offset) {
            --st->panel_file->offset;
        }
    }
}

void panel_gaki_select_down(Panel_Gaki *st, size_t n) {
    if(!st->panel_file) return;
    ++st->panel_file->select;
    if(st->panel_file->select >= file_infos_length(st->panel_file->file_infos)) {
        st->panel_file->select = 0;
        st->panel_file->offset = 0;
    }
    if(st->panel_file->select >= st->panel_file->offset + st->rc_files.dim.y) {
        ++st->panel_file->offset;
    }
}

void panel_gaki_select_at(Panel_Gaki *st, size_t n) {
}

void panel_gaki_update(Panel_Gaki *st, Action *ac) {

    st->rc_files.anc.x = 0;
    st->rc_files.anc.y = 1;
    st->rc_files.dim.x = st->pt_dimension.x / 2;
    st->rc_files.dim.y = st->pt_dimension.y - 1;
    st->rc_split.anc.x = st->rc_files.dim.x;
    st->rc_split.anc.y = 1;
    st->rc_split.dim.x = 1;
    st->rc_split.dim.y = st->pt_dimension.y - 1;;
    st->rc_preview.anc.x = st->rc_split.anc.x + 1;
    st->rc_preview.anc.y = 1;
    st->rc_preview.dim.x = st->pt_dimension.x - st->rc_files.anc.x;
    st->rc_preview.dim.y = st->pt_dimension.y - 1;;
    st->rc_pwd.anc.x = 0;
    st->rc_pwd.anc.y = 0; //st->pt_dimension.y - 1;
    st->rc_pwd.dim.x = st->pt_dimension.x;
    st->rc_pwd.dim.y = 1;

    if(ac->select_left) {
        So left = so_ensure_dir(so_rsplit_ch(st->pwd, PLATFORM_CH_SUBDIR, 0));
        if(left.len) {
            st->pwd = left;
            st->panel_file = 0;
        } else {
            so_copy(&st->pwd, so("/"));
            st->panel_file = 0;
        }
    }

    if(ac->select_right && st->panel_file) {
        if(st->panel_file->select < file_infos_length(st->panel_file->file_infos)) {
            File_Info *sel = file_infos_get_at(&st->panel_file->file_infos, st->panel_file->select);
            if(S_ISDIR(sel->stats.st_mode)) {
                so_path_join(&st->pwd, st->pwd, sel->filename);
                st->panel_file = 0;
            } else if(S_ISREG(sel->stats.st_mode)) {
                So ed = SO;
                char *ced = 0;
                so_env_get(&ed, so("EDITOR"));
                if(so_len(ed)) {
#if 0
                    so_fmt(&ed, " '%.*s'", SO_F(sel->path));
                    ced = so_dup(ed);
                    tui_exit();
                    system(ced);
                    tui_enter();
#endif
                } else {
                    printff("\rNO EDITOR FOUND");
                    exit(1);
                }
                so_free(&ed);
                free(ced);
            }
        }
    }

    if(!so_len(st->pwd)) {
        so_env_get(&st->pwd, so("PWD"));
    }

    if(!st->panel_file) {
        t_panel_file_ensure_exist(&st->t_file_infos, &st->panel_file, st->pwd);
    }

    if(ac->select_up) {
        panel_gaki_select_up(st, ac->select_up);
    }

    if(ac->select_down) {
        panel_gaki_select_down(st, ac->select_down);
    }

    memset(ac, 0, sizeof(*ac));

}


