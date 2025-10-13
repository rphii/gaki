#include "gaki-state.h"

void gaki_state_select_up(Gaki_State *st, size_t n) {
    if(!st->file_panel) return;
    if(!st->file_panel->select) {
        st->file_panel->select = file_infos_length(st->file_panel->file_infos) - 1;
        st->file_panel->offset = st->file_panel->select + 1 > st->rc_files.dim.y ? st->file_panel->select + 1 - st->rc_files.dim.y : 0;
    } else {
        --st->file_panel->select;
        if(st->file_panel->offset) {
            --st->file_panel->offset;
        }
    }
}

void gaki_state_select_down(Gaki_State *st, size_t n) {
    if(!st->file_panel) return;
    ++st->file_panel->select;
    if(st->file_panel->select >= file_infos_length(st->file_panel->file_infos)) {
        st->file_panel->select = 0;
        st->file_panel->offset = 0;
    }
    if(st->file_panel->select >= st->file_panel->offset + st->rc_files.dim.y) {
        ++st->file_panel->offset;
    }
}

void gaki_state_select_at(Gaki_State *st, size_t n) {
}

void gaki_state_update(Gaki_State *st, Action *ac) {

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
            st->file_panel = 0;
        } else {
            so_copy(&st->pwd, so("/"));
            st->file_panel = 0;
        }
    }

    if(ac->select_right && st->file_panel) {
        if(st->file_panel->select < file_infos_length(st->file_panel->file_infos)) {
            File_Info *sel = file_infos_get_at(&st->file_panel->file_infos, st->file_panel->select);
            if(S_ISDIR(sel->stats.st_mode)) {
                so_path_join(&st->pwd, st->pwd, sel->filename);
                st->file_panel = 0;
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

    if(!st->file_panel) {
        t_file_panel_ensure_exist(&st->t_file_infos, &st->file_panel, st->pwd);
    }

    if(ac->select_up) {
        gaki_state_select_up(st, ac->select_up);
    }

    if(ac->select_down) {
        gaki_state_select_down(st, ac->select_down);
    }

    memset(ac, 0, sizeof(*ac));

}


