#include "nav-directory.h"
#include "gaki-sync.h"

typedef struct Task_Nav_Directory_Register {
    Gaki_Sync_Panel *sync;
    Gaki_Sync_Main *sync_m;
    Gaki_Sync_T_File_Info *sync_t;
    So path;
    size_t current_register;
} Task_Nav_Directory_Register;

typedef struct Task_Nav_Directory_Readreg {
    Gaki_Sync_Panel *sync;
    Nav_Directory *nav;
    Gaki_Sync_Main *sync_m;
    Gaki_Sync_T_File_Info *sync_t;
} Task_Nav_Directory_Readreg;

typedef struct Task_Nav_Directory_Readdir {
    Gaki_Sync_Panel *sync;
    Nav_Directory *nav;
    Gaki_Sync_Main *sync_m;
    Gaki_Sync_T_File_Info *sync_t;
} Task_Nav_Directory_Readdir;

File_Info *file_info_ensure(Gaki_Sync_T_File_Info *sync, So path) {
    pthread_rwlock_wrlock(&sync->rwl);
    File_Info *info = t_file_info_get(&sync->t_file_info, path);
    if(!info) {
        File_Info info_new = {0};
        info_new.path = so_clone(path);
        char *cpath = so_dup(path);
        stat(cpath, &info_new.stats);
        free(cpath);
        info = t_file_info_once(&sync->t_file_info, path, &info_new)->val;
    }
    pthread_rwlock_unlock(&sync->rwl);
    return info;
}

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
    So content = SO;
    so_file_read(nav->pwd.ref->path, &content);
    //printff(TUI_ESC_CODE_CLEAR);
    //printff("\r[%.*s] << READ, LEN: %zu",SO_F(nav->pwd.ref->path), content.len);sleep(1);
    //printff("\r[%.*s]",SO_F(content));sleep(1);


    pthread_mutex_lock(&task->sync->mtx);
    bool main_update = nav == task->nav && content.len;
    nav->pwd.ref->content.text = content;
    pthread_mutex_unlock(&task->sync->mtx);

    if(main_update) {
        pthread_mutex_lock(&task->sync_m->mtx);
        ++task->sync_m->update_do;
        pthread_cond_signal(&task->sync_m->cond);
        pthread_mutex_unlock(&task->sync_m->mtx);
    }

    free(task);
    return 0;
}

void *nav_directory_async_readdir(Pw *pw, bool *cancel, void *void_task) {
    ASSERT_ARG(pw);
    ASSERT_ARG(cancel);
    ASSERT_ARG(void_task);
    Task_Nav_Directory_Readdir *task = void_task;
    Nav_Directory tmp = {0};

    So path = task->nav->pwd.ref->path;
    //printff("NOT READ: %.*s",SO_F(path));

    struct dirent *dir;
    char *cdirname = so_dup(path);
    DIR *d = opendir(cdirname);
    free(cdirname);
    if(d) {
        while((dir = readdir(d)) != NULL) {
            if(dir->d_name[0] == '.') continue;
            So search = SO;
            so_path_join(&search, so_ensure_dir(path), so_l(dir->d_name));
            //printff("\r>> %.*s",SO_F(search));
            File_Info *info = file_info_ensure(task->sync_t, search);
            so_free(&search);
            info->loaded = true;
            Nav_Directory *nav_sub;
            NEW(Nav_Directory, nav_sub);
            nav_sub->parent = task->nav;
            nav_sub->pwd.ref = info;
            array_push(tmp.list, nav_sub);
        }
        closedir(d);
    }

    nav_directories_sort(tmp.list);

    Nav_Directory *nav = task->sync->panel_gaki.nav_directory;
    pthread_mutex_lock(&task->sync->mtx);
    bool main_update = nav == task->nav || task->nav->parent == nav;
    task->nav->list = tmp.list;
    pthread_mutex_unlock(&task->sync->mtx);

    if(main_update) {
        pthread_mutex_lock(&task->sync_m->mtx);
        ++task->sync_m->update_do;
        pthread_cond_signal(&task->sync_m->cond);
        pthread_mutex_unlock(&task->sync_m->mtx);
    }

    free(task);
    return 0;
}

void nav_directory_dispatch_readany(Pw *pw, Gaki_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, Nav_Directory *dir) {
    ASSERT_ARG(pw);
    ASSERT_ARG(sync_m);
    ASSERT_ARG(sync_t);
    ASSERT_ARG(sync);
    ASSERT_ARG(dir);
    if(!dir->pwd.ref) return;
    if(dir->pwd.have_read) return;
    dir->pwd.have_read = true;
    switch(dir->pwd.ref->stats.st_mode & S_IFMT) {
        case S_IFDIR: {
            Task_Nav_Directory_Readdir *task;
            NEW(Task_Nav_Directory_Readdir, task);
            task->nav = dir;
            task->sync = sync;
            task->sync_m = sync_m;
            task->sync_t = sync_t;
            pw_queue(pw, nav_directory_async_readdir, task);
        } break;
        case S_IFREG: {
            Task_Nav_Directory_Readreg *task;
            NEW(Task_Nav_Directory_Readreg, task);
            task->nav = dir;
            task->sync = sync;
            task->sync_m = sync_m;
            task->sync_t = sync_t;
            pw_queue(pw, nav_directory_async_readreg, task);
        } break;
        default: break;
    }

    if(!dir->parent) {
        So path = so_clone(so_rsplit_ch(dir->pwd.ref->path, PLATFORM_CH_SUBDIR, 0));
        if(!so_len(path)) so_copy(&path, so(PLATFORM_S_SUBDIR));
        if(so_len(path) < so_len(dir->pwd.ref->path)) {
            //printff("\r[%.*s] -> PARENT:[%.*s]",SO_F(dir->pwd.ref->path),SO_F(path));sleep(1);
            File_Info *info = file_info_ensure(sync_t, path);
            NEW(Nav_Directory, dir->parent);
            dir->parent->pwd.ref = info;
            nav_directory_dispatch_readany(pw, sync_m, sync_t, sync, dir->parent);
        }
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
        nav_directory_dispatch_readany(pw, task->sync_m, task->sync_t, task->sync, nav);
    }
    pthread_mutex_unlock(&task->sync->mtx);

    pthread_mutex_lock(&task->sync_m->mtx);
    ++task->sync_m->render_do;
    pthread_cond_signal(&task->sync_m->cond);
    pthread_mutex_unlock(&task->sync_m->mtx);

    free(task);
    return 0;
}

void nav_directory_dispatch_register(Pw *pw, Gaki_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, So path) {
    ASSERT_ARG(sync);
    if(!so_len(path)) return;
    Task_Nav_Directory_Register *task;
    NEW(Task_Nav_Directory_Register, task);
    task->sync = sync;
    task->path = so_clone(path);
    task->sync_t = sync_t;
    task->sync_m = sync_m;

    ASSERT_ARG(!task->sync->count_register);

    pthread_mutex_lock(&task->sync->mtx);
    task->current_register = ++task->sync->count_register;
    pthread_mutex_unlock(&task->sync->mtx);

    pw_queue(pw, nav_directory_async_register, task);
}

