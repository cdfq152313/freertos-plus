#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>

#define hash_init 5381
#define MAX_FILE_COUNT 500
#define TRUE 1
#define FALSSE 0

struct _Index{
    uint32_t hash;
    uint8_t is_directory;
    char name[51];
    uint32_t size;
    uint32_t address;
    uint32_t scope;
    uint32_t child_scope;
};
typedef struct _Index Index;
Index filetable[MAX_FILE_COUNT];

uint32_t hash_djb2(const uint8_t * str, uint32_t hash) {
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) ^ c;

    return hash;
}

void usage(const char * binname) {
    printf("Usage: %s [-d <dir>] [outfile]\n", binname);
    exit(-1);
}

static uint32_t file_count = 0;
static uint32_t scope_count = 0;
void processdir(DIR * dirp, const char * curpath, const char * prefix, uint32_t cur_scope, uint32_t cur_index) {

    char fullpath[1024];
    struct dirent * ent;
    DIR * rec_dirp;
    uint32_t cur_hash = hash_djb2((const uint8_t *) curpath, hash_init);
    uint32_t child_scope;
    uint32_t dir_size = 0;
    FILE * infile;

    //if this is a dirctory, it has child nodes.
    child_scope = scope_count ++;
    
    //traversal all child. when end of traversal, record the directory size.
    while ((ent = readdir(dirp))) {
        //if . and .. return
        if (strcmp(ent->d_name, ".") == 0)
            continue;
        if (strcmp(ent->d_name, "..") == 0)
            continue;
        
        //record some information
        dir_size ++;
        file_count ++;
        filetable[file_count].hash = hash_djb2((const uint8_t *) ent->d_name, cur_hash);
        filetable[file_count].is_directory = TRUE;
        strcpy( filetable[file_count].name, ent->d_name); 
        filetable[file_count].scope = child_scope ;

        //if this file is directory, recursive
        //else record the data size
        strcpy(fullpath, prefix);
        strcat(fullpath, "/");
        strcat(fullpath, curpath);
        strcat(fullpath, ent->d_name);

    #ifdef _WIN32
        if (GetFileAttributes(fullpath) & FILE_ATTRIBUTE_DIRECTORY) {
    #else
        if (ent->d_type == DT_DIR) {
    #endif
            strcat(fullpath, "/");
            rec_dirp = opendir(fullpath);
            processdir(rec_dirp, fullpath + strlen(prefix) + 1, prefix, child_scope, file_count);
            closedir(rec_dirp);
        } else {
            infile = fopen(fullpath, "rb");
            if (!infile) {
                perror("opening input file");
                exit(-1);
            }
            fseek(infile, 0, SEEK_END);
            filetable[file_count].size = ftell(infile);
            fclose(infile);
        }
    }
    filetable[cur_index].size = dir_size;

}


void print_filetable(){
    printf("\nnow print filetable");
    int i, j;
    for(i = 0; i <= file_count; ++i){
        for(j = 0; j < filetable[i].scope; ++j)
            printf("  ");
        printf("%u %s",filetable[i].scope ,filetable[i].name );
        if(filetable[i].is_directory)
            printf("%u", filetable[i].child_scope);
        printf("\n");
    }
    printf("\n");
}

int main(int argc, char ** argv) {
    char * binname = *argv++;
    char * o;
    char * outname = NULL;
    char * dirname = ".";
    uint64_t z = 0;
    FILE * outfile;
    DIR * dirp;

    while ((o = *argv++)) {
        if (*o == '-') {
            o++;
            switch (*o) {
            case 'd':
                dirname = *argv++;
                break;
            default:
                usage(binname);
                break;
            }
        } else {
            if (outname)
                usage(binname);
            outname = o;
        }
    }

    if (!outname)
        outfile = stdout;
    else
        outfile = fopen(outname, "wb");

    if (!outfile) {
        perror("opening output file");
        exit(-1);
    }

    dirp = opendir(dirname);
    if (!dirp) {
        perror("opening directory");
        exit(-1);
    }

    //init
    file_count = 0;
    scope_count = 0;
    filetable[0].is_directory = TRUE;
    strcpy( filetable[0].name, "/" );
    filetable[0].scope = scope_count;
    processdir(dirp, "", dirname, scope_count, file_count);

    //fwrite(&z, 1, 8, outfile);
    print_filetable();
    if (outname)
        fclose(outfile);
    closedir(dirp);
    
    return 0;
}
