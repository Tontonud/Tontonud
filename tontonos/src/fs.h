/* TontonOS - système de fichiers en mémoire (arborescence) */
#ifndef FS_H
#define FS_H
#include "kernel.h"

#define FS_NAME 28
#define FS_MAX  128

typedef struct fnode {
    int   used, is_dir;
    char  name[FS_NAME];
    struct fnode *parent;
    char *data;
    u32   size;
} fnode_t;

extern fnode_t *fs_root;
void     fs_init(void);
fnode_t *fs_find(fnode_t *dir, const char *name);
fnode_t *fs_create(fnode_t *dir, const char *name, int is_dir);
int      fs_write(fnode_t *f, const char *data, u32 size);
int      fs_delete(fnode_t *n);              /* 0 si dossier non vide */
int      fs_list(fnode_t *dir, fnode_t **out, int max);
fnode_t *fs_resolve(fnode_t *cwd, const char *path);
void     fs_path(fnode_t *n, char *out, int max);
int      fs_count(void);

#endif
