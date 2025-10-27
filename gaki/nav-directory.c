#include "nav-directory.h"
#include "gaki-sync.h"

typedef struct Task_Nav_Directory_Register {
    Gaki_Sync_Panel *sync;
    Tui_Sync_Main *sync_m;
    Gaki_Sync_T_File_Info *sync_t;
    So path;
    size_t current_register;
} Task_Nav_Directory_Register;

typedef struct Task_Nav_Directory_Readreg {
    Gaki_Sync_Panel *sync;
    Nav_Directory *nav;
    Tui_Sync_Main *sync_m;
    Gaki_Sync_T_File_Info *sync_t;
} Task_Nav_Directory_Readreg;

typedef struct Task_Nav_Directory_Readdir {
    Gaki_Sync_Panel *sync;
    Nav_Directory *nav;
    Nav_Directory *child;
    Tui_Sync_Main *sync_m;
    Gaki_Sync_T_File_Info *sync_t;
} Task_Nav_Directory_Readdir;

#if 0
void x() {
    DIR *d;
    struct dirent *dir;
    char *cdirname = so_dup(path);
    char *cpath = 0;
    d = opendir(cdirname);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_name[0] == '.') continue;
            File_Info info = {0};
            so_path_join(&info.path, path, so_l(dir->d_name));
            cpath = so_dup(info.path);
            stat(cpath, &info.stats);
            if(S_ISDIR(info.stats.st_mode)) {
                t_panel_file_ensure_exist(panels, &info.panel_file, info.path);
            }
            file_infos_push_back(file_infos, &info);
        }
        closedir(d);
    }
    free(cdirname);
    free(cpath);
    file_infos_sort(file_infos);
    return(0);
}
#endif

#include <dirent.h>


void nav_directories_sort(Nav_Directories *vec) {
    /* shell sort, https://rosettacode.org/wiki/Sorting_algorithms/Shell_sort */
    size_t h, i, j, n = array_len(vec);
    Nav_Directory *temp;
    for (h = n; h /= 2;) {
        for (i = h; i < n; i++) {
            /*t = a[i]; */
            temp = array_at(vec, i);
            //printff("\rtemp = [%zu]",i);
            /*for (j = i; j >= h && t < a[j - h]; j -= h) { */
            So so_a = temp->pwd.ref->path;
            for (j = i; j >= h && so_cmp_s(so_a, (array_at(vec, j-h))->pwd.ref->path) < 0; j -= h) {
                //printff("\r '%.*s'%.*s'", SO_F(so_a), SO_F((array_at(vec, j-h))->pwd.ref->path));
                *array_it(vec, j) = array_at(vec, j-h);
                //printff("\r[%zu] = [%zu]",j,j-h);
                /*a[j] = a[j - h]; */
            }
            /*a[j] = t; */
            *array_it(vec, j) = temp;
            //printff("\r[%zu] = temp",j);
            //sleep(1);
        }
    }
}

void *nav_directory_async_readreg(Pw *pw, bool *cancel, void *void_task) {
    ASSERT_ARG(pw);
    ASSERT_ARG(cancel);
    ASSERT_ARG(void_task);
    Task_Nav_Directory_Readreg *task = void_task;
    Nav_Directory *nav = task->nav;
    ASSERT_ARG(nav);
    ASSERT_ARG(nav->pwd.ref);

    pthread_mutex_lock(&task->nav->pwd.ref->mtx);
    bool loaded = task->nav->pwd.ref->loaded;
    task->nav->pwd.ref->loaded = true;

    So content = SO;
    if(!loaded) {

        task->nav->pwd.have_read = true;
        if(nav->pwd.ref->stats.st_size >= 0x8000) {
            goto exit;
        }

        so_file_read(nav->pwd.ref->path, &content);

        nav->pwd.ref->content.text = content;
    }

    /* done */
    pthread_mutex_unlock(&task->nav->pwd.ref->mtx);

    pthread_mutex_lock(&task->sync->mtx);
    bool main_update = nav == task->nav && content.len;
    pthread_mutex_unlock(&task->sync->mtx);

    //tui_sync_main_render(task->sync_m);
    if(main_update) {
        tui_sync_main_render(task->sync_m);
    }

exit:
    free(task);
    return 0;
}

void *nav_directory_async_readdir(Pw *pw, bool *cancel, void *void_task) {
    ASSERT_ARG(pw);
    ASSERT_ARG(cancel);
    ASSERT_ARG(void_task);
    Task_Nav_Directory_Readdir *task = void_task;

    Nav_Directory tmp = {0};

    size_t len_list = 0;
    size_t index = 0;
    So path = task->nav->pwd.ref->path;

    pthread_mutex_lock(&task->nav->pwd.ref->mtx);
    bool loaded = task->nav->pwd.ref->loaded;
    task->nav->pwd.ref->loaded = true;

    size_t files_len = 0;
    if(loaded) {
        files_len = array_len(task->nav->pwd.ref->content.files);
    } else {
        struct dirent *dir;
        char *cdirname = so_dup(path);
        DIR *d = opendir(cdirname);
        free(cdirname);
        if(d) {
            So search = SO;
            while((dir = readdir(d)) != NULL) {
                if(dir->d_name[0] == '.') continue;
                so_clear(&search);
                so_path_join(&search, so_ensure_dir(path), so_l(dir->d_name));
                //usleep(1e5);printff("\r>> %.*s",SO_F(search));
                File_Info *info = file_info_ensure(task->sync_t, search);
                array_push(task->nav->pwd.ref->content.files, info);
                info->loaded = true;
            }
            so_free(&search);
            closedir(d);
        }
        files_len = array_len(task->nav->pwd.ref->content.files);
    }

    for(size_t i = 0; i < files_len; ++i) {
        File_Info *info = array_at(task->nav->pwd.ref->content.files, i);
        Nav_Directory *nav_sub;
        if(task->child && task->child->pwd.ref == info) {
            nav_sub = task->child;
            //sleep(1);printff("\r[%.*s]->[%.*s]", SO_F(task->nav->pwd.ref->path), SO_F(nav_sub->pwd.ref->path));sleep(1);
        } else {
            NEW(Nav_Directory, nav_sub);
            nav_sub->pwd.ref = info;
        }
        nav_sub->parent = task->nav;
        array_push(tmp.list, nav_sub);
        ++len_list;
    }

    /* done */
    task->nav->list = tmp.list;
    pthread_mutex_unlock(&task->nav->pwd.ref->mtx);

    nav_directories_sort(tmp.list);

    if(task->child) {
        for(size_t i = 0; i < len_list; ++i) {
            Nav_Directory *nav_sub = array_at(tmp.list, i);
            if(nav_sub->pwd.ref == task->child->pwd.ref) {
                index = i;
                break;
            }
        }
    }

    //MTX_DONE(&task->nav->pwd.ref->mtx);

    /* done, apply */
    pthread_mutex_lock(&task->sync->mtx);
    Nav_Directory *nav = task->sync->panel_gaki.nav_directory;
    bool main_update = nav == task->nav;
    bool main_render = task->nav->parent == nav || task->nav == nav->parent;
    task->nav->list = tmp.list;
    task->nav->index = index;
    pthread_mutex_unlock(&task->sync->mtx);

    if(main_update) {
        tui_sync_main_update(task->sync_m);
    }
    if(main_render) {
        tui_sync_main_render(task->sync_m);
    }

    free(task);
    return 0;
}

void nav_directory_dispatch_readdir(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, Nav_Directory *dir, Nav_Directory *child) {
    Task_Nav_Directory_Readdir *task;

    pthread_mutex_lock(&dir->pwd.mtx);
    bool have_read = dir->pwd.have_read;
    dir->pwd.have_read = true;
    pthread_mutex_unlock(&dir->pwd.mtx);
    if(have_read) return;

    NEW(Task_Nav_Directory_Readdir, task);
    task->nav = dir;
    task->sync = sync;
    task->sync_m = sync_m;
    task->sync_t = sync_t;
    task->child = child;
    pw_queue(pw, nav_directory_async_readdir, task);
}

void nav_directory_dispatch_readreg(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, Nav_Directory *dir) {
    Task_Nav_Directory_Readreg *task;

    pthread_mutex_lock(&dir->pwd.mtx);
    bool have_read = dir->pwd.have_read;
    dir->pwd.have_read = true;
    pthread_mutex_unlock(&dir->pwd.mtx);
    if(have_read) return;

    NEW(Task_Nav_Directory_Readreg, task);
    task->nav = dir;
    task->sync = sync;
    task->sync_m = sync_m;
    task->sync_t = sync_t;
    pw_queue(pw, nav_directory_async_readreg, task);
}

void nav_directory_dispatch_readany(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, Nav_Directory *dir) {
    ASSERT_ARG(pw);
    ASSERT_ARG(sync_m);
    ASSERT_ARG(sync_t);
    ASSERT_ARG(sync);
    ASSERT_ARG(dir);
    if(!dir->pwd.ref) return;
    if(dir->pwd.have_read) return;
    switch(dir->pwd.ref->stats.st_mode & S_IFMT) {
        case S_IFDIR: {
            nav_directory_dispatch_readdir(pw, sync_m, sync_t, sync, dir, 0);
        } break;
        case S_IFREG: {
            nav_directory_dispatch_readreg(pw, sync_m, sync_t, sync, dir);
        } break;
        default: break;
    }
}

void *nav_directory_async_register(Pw *pw, bool *cancel, void *void_task) {
    ASSERT_ARG(pw);
    ASSERT_ARG(cancel);
    ASSERT_ARG(void_task);
    Task_Nav_Directory_Register *task = void_task;

    File_Info *info = file_info_ensure(task->sync_t, task->path);
    ASSERT_ARG(S_ISDIR(info->stats.st_mode));

    pthread_mutex_lock(&task->sync->mtx);
    if(task->sync->count_register && task->sync->count_register == task->current_register) {
        /* publish */
        Nav_Directory *nav;
        NEW(Nav_Directory, nav);
        nav->pwd.ref = info;
        task->sync->panel_gaki.nav_directory = nav;
        task->sync->panel_gaki.tab_sel = array_len(task->sync->panel_gaki.tabs);
        array_push(task->sync->panel_gaki.tabs, nav);
        nav_directory_dispatch_readany(pw, task->sync_m, task->sync_t, task->sync, nav);
    }
    pthread_mutex_unlock(&task->sync->mtx);

    free(task);
    return 0;
}

void nav_directory_dispatch_register(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, So path) {
    ASSERT_ARG(sync);
    if(!so_len(path)) return;
    Task_Nav_Directory_Register *task;
    NEW(Task_Nav_Directory_Register, task);
    task->sync = sync;
    task->path = so_clone(path);
    task->sync_t = sync_t;
    task->sync_m = sync_m;

    //ASSERT_ARG(!task->sync->count_register);

    task->current_register = ++task->sync->count_register;

    pw_queue(pw, nav_directory_async_register, task);
}

