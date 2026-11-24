/* TontonOS - système de fichiers en mémoire */
#include "fs.h"

static fnode_t nodes[FS_MAX];
fnode_t *fs_root;

fnode_t *fs_find(fnode_t *dir, const char *name) {
    if (!strcmp(name, ".")) return dir;
    if (!strcmp(name, "..")) return dir->parent ? dir->parent : dir;
    for (int i = 0; i < FS_MAX; i++)
        if (nodes[i].used && nodes[i].parent == dir && !strcmp(nodes[i].name, name)) return &nodes[i];
    return NULL;
}
fnode_t *fs_create(fnode_t *dir, const char *name, int is_dir) {
    if (!dir || !dir->is_dir || !*name || strlen(name) >= FS_NAME || fs_find(dir, name)) return NULL;
    for (const char *p = name; *p; p++) if (*p == '/') return NULL;
    for (int i = 0; i < FS_MAX; i++) if (!nodes[i].used) {
        fnode_t *n = &nodes[i];
        memset(n, 0, sizeof(*n));
        n->used = 1; n->is_dir = is_dir; n->parent = dir;
        strlcpy(n->name, name, FS_NAME);
        return n;
    }
    return NULL;
}
int fs_write(fnode_t *f, const char *data, u32 size) {
    if (!f || f->is_dir) return 0;
    char *d = kmalloc(size + 1);
    if (!d) return 0;
    memcpy(d, data, size); d[size] = 0;
    kfree(f->data);
    f->data = d; f->size = size;
    return 1;
}
int fs_delete(fnode_t *n) {
    if (!n || n == fs_root) return 0;
    for (int i = 0; i < FS_MAX; i++) if (nodes[i].used && nodes[i].parent == n) return 0;
    kfree(n->data);
    n->used = 0;
    return 1;
}
int fs_list(fnode_t *dir, fnode_t **out, int max) {
    int c = 0;
    /* dossiers d'abord, puis fichiers, chacun par ordre alphabétique */
    for (int pass = 1; pass >= 0; pass--) {
        int start = c;
        for (int i = 0; i < FS_MAX && c < max; i++)
            if (nodes[i].used && nodes[i].parent == dir && nodes[i].is_dir == pass) out[c++] = &nodes[i];
        for (int a = start + 1; a < c; a++)
            for (int b = a; b > start && strcmp(out[b - 1]->name, out[b]->name) > 0; b--) {
                fnode_t *t = out[b]; out[b] = out[b - 1]; out[b - 1] = t;
            }
    }
    return c;
}
fnode_t *fs_resolve(fnode_t *cwd, const char *path) {
    fnode_t *n = *path == '/' ? fs_root : cwd;
    char part[FS_NAME];
    while (*path) {
        while (*path == '/') path++;
        if (!*path) break;
        int k = 0;
        while (*path && *path != '/') { if (k < FS_NAME - 1) part[k++] = *path; path++; }
        part[k] = 0;
        if (!n->is_dir) return NULL;
        n = fs_find(n, part);
        if (!n) return NULL;
    }
    return n;
}
void fs_path(fnode_t *n, char *out, int max) {
    fnode_t *chain[16]; int d = 0;
    for (; n && n != fs_root && d < 16; n = n->parent) chain[d++] = n;
    out[0] = 0;
    if (!d) { strlcpy(out, "/", max); return; }
    int len = 0;
    while (d--) {
        if (len + 1 < max) { out[len++] = '/'; out[len] = 0; }
        strlcpy(out + len, chain[d]->name, max - len);
        len = strlen(out);
    }
}
int fs_count(void) { int c = 0; for (int i = 0; i < FS_MAX; i++) c += nodes[i].used; return c; }

static void add_file(fnode_t *dir, const char *name, const char *text) {
    fnode_t *f = fs_create(dir, name, 0);
    if (f) fs_write(f, text, strlen(text));
}
void fs_init(void) {
    memset(nodes, 0, sizeof(nodes));
    fs_root = &nodes[0];
    fs_root->used = 1; fs_root->is_dir = 1;
    strcpy(fs_root->name, "/");
    fnode_t *docs = fs_create(fs_root, "Documents", 1);
    fs_create(fs_root, "Images", 1);
    fnode_t *sys = fs_create(fs_root, "Système", 1);
    add_file(docs, "Bienvenue.txt",
        "Bienvenue dans TontonOS !\n\n"
        "Ce fichier se trouve dans le dossier Documents.\n"
        "Modifiez-le puis cliquez sur Enregistrer (ou Ctrl+S).\n\n"
        "Attention : les fichiers vivent en mémoire vive.\n"
        "Ils disparaissent quand l'ordinateur redémarre.\n");
    add_file(docs, "Liste de courses.txt", "- pain\n- lait\n- fromage\n- café\n");
    add_file(sys, "version.txt", "TontonOS 1.1\nNoyau 32 bits, mode protégé x86\n");
    add_file(sys, "licence.txt", "TontonOS est écrit à partir de zéro.\nPolice : Lat15-VGA16 (console-setup).\n");
}
