#include "osdebug.h"
#include "filesystem.h"
#include "fio.h"

#include <stdint.h>
#include <string.h>
#include <hash-djb2.h>

#define MAX_FS 16

struct fs_t {
    uint32_t hash;
    const char * name;
    fs_open_t cb;
    void * opaque;
};

static struct fs_t fss[MAX_FS];

__attribute__((constructor)) void fs_init() {
    memset(fss, 0, sizeof(fss));
}

int register_fs(const char * mountpoint, fs_open_t callback, void * opaque) {
    int i;
    DBGOUT("register_fs(\"%s\", %p, %p)\r\n", mountpoint, callback, opaque);
    
    for (i = 0; i < MAX_FS; i++) {
        if (!fss[i].cb) {
            fss[i].hash = hash_djb2((const uint8_t *) mountpoint, -1);
            fss[i].name = mountpoint;
            fss[i].cb = callback;
            fss[i].opaque = opaque;
            return 0;
        }
    }
    
    return -1;
}

void fs_list_root(char * output){
    int i;
    
    output[0] = '\0';
    for(i = 0; i < MAX_FS; ++i){
        if(fss[i].cb){
            strcat(output, fss[i].name);
            strcat(output, "\n");
        }
    }
}

int fs_list(const char * path, char * output){

    //if only ///// list root
    while(path[0] == '/')
        path++;
    if(path[0] == '\0'){
        fs_list_root(output);
        return 0;
    }

    //else list mountpoint

    return -1;
}

int fs_open(const char * path, int flags, int mode) {
    const char * slash;
    uint32_t hash;
    int i;
//    DBGOUT("fs_open(\"%s\", %i, %i)\r\n", path, flags, mode);
    
    while (path[0] == '/')
        path++;
    
    slash = strchr(path, '/');
    
    if (!slash)
        return -2;

    hash = hash_djb2((const uint8_t *) path, slash - path);
    path = slash + 1;

    for (i = 0; i < MAX_FS; i++) {
        if (fss[i].hash == hash)
            return fss[i].cb(fss[i].opaque, path, flags, mode);
    }
    
    return -2;
}
