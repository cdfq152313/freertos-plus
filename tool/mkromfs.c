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
#define FILE_NAME_LENGTH 51
#define MAX_FILE_COUNT 500
#define FILE_BLOCK_SIZE 1024
#define TRUE 1
#define FALSE 0

struct _Index{
    uint32_t hash;
    uint8_t is_directory;
    char name[FILE_NAME_LENGTH];
    char fullpath[1024];
    uint32_t size;
    uint32_t block_index;
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
    child_scope = ++ scope_count ;
    filetable[cur_index].child_scope = child_scope;
    
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
            filetable[file_count].is_directory = TRUE;
            strcat(fullpath, "/");
            rec_dirp = opendir(fullpath);
            processdir(rec_dirp, fullpath + strlen(prefix) + 1, prefix, child_scope, file_count);
            closedir(rec_dirp);
        } else {
            filetable[file_count].is_directory = FALSE;
            strcpy( filetable[file_count].fullpath , fullpath);
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

int cmp(const void * a, const void * b){
    return ( ((Index*)a)->scope - ((Index*)b)->scope ) ;
}

void calculate_block_dir(uint32_t cur){
    uint32_t i;
    for(i = cur+1; i < file_count; ++i){
        if(filetable[cur].child_scope == filetable[i].scope ){
            filetable[cur].block_index = i-1;
            return ;
        }
    }
}

void calculate_block_file(uint32_t cur, uint32_t * file_block_count){
    filetable[cur].block_index = *file_block_count;
    (*file_block_count) += ((filetable[cur].size / FILE_BLOCK_SIZE) + 1);
}

void calculate_block(){
    uint32_t cur;
    uint32_t file_block_count = 0;
    for(cur = 0; cur < file_count; ++cur){
        if(filetable[cur].is_directory){
            calculate_block_dir(cur);
        }
        else{
            calculate_block_file(cur, &file_block_count);
        }
    }
}

void print_filetable(){
    printf("\nnow print filetable\n");
    int i, j;
    for(i = 0; i < file_count; ++i){
        for(j = 0; j < filetable[i].scope; ++j)
            printf("  ");
        printf("%u %s, block index(%u)",filetable[i].scope , filetable[i].name, filetable[i].block_index );
        if(filetable[i].is_directory)
            printf(", child(%u)", filetable[i].child_scope);
        printf("\n");
    }
    printf("\n");
}

void write_output(FILE * outfile){
    uint8_t i, j;
    uint8_t b;
    uint32_t size, w;
    FILE * infile;
    char buf[FILE_BLOCK_SIZE];
    
    //file index count (root doesn't have index)
    b =( (file_count-1) >> 0) & 0xff; fwrite(&b, 1, 1, outfile);
    b =( (file_count-1) >> 8) & 0xff; fwrite(&b, 1, 1, outfile);
    b =( (file_count-1) >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
    b =( (file_count-1) >> 24) & 0xff; fwrite(&b, 1, 1, outfile);
    
    //root size
    b = (filetable[0].size >> 0) & 0xff; fwrite(&b, 1, 1, outfile);
    b = (filetable[0].size >> 8) & 0xff; fwrite(&b, 1, 1, outfile);
    b = (filetable[0].size >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
    b = (filetable[0].size >> 24) & 0xff; fwrite(&b, 1, 1, outfile);
    
    //file index
    for(i = 1; i < file_count; ++i){
        //hash value
        b = (filetable[i].hash >> 0) & 0xff; fwrite(&b, 1, 1, outfile);
        b = (filetable[i].hash >> 8) & 0xff; fwrite(&b, 1, 1, outfile);
        b = (filetable[i].hash >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
        b = (filetable[i].hash >> 24) & 0xff; fwrite(&b, 1, 1, outfile);
        //is dir?
        b = filetable[i].is_directory; fwrite(&b, 1, 1, outfile);
        //filename
        for(j = 0 ; j < FILE_NAME_LENGTH; j++)
            fwrite(filetable[i].name+j, 1 ,1, outfile);
        //datasize
        b = (filetable[i].size >> 0) & 0xff; fwrite(&b, 1, 1, outfile);
        b = (filetable[i].size >> 8) & 0xff; fwrite(&b, 1, 1, outfile);
        b = (filetable[i].size >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
        b = (filetable[i].size >> 24) & 0xff; fwrite(&b, 1, 1, outfile);

        //data block_index
        b = (filetable[i].block_index >> 0) & 0xff; fwrite(&b, 1, 1, outfile);
        b = (filetable[i].block_index >> 8) & 0xff; fwrite(&b, 1, 1, outfile);
        b = (filetable[i].block_index >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
        b = (filetable[i].block_index >> 24) & 0xff; fwrite(&b, 1, 1, outfile);
    }

    
    //file content
    for(i = 1; i < file_count; ++i){
        if(!filetable[i].is_directory){
            infile = fopen(filetable[i].fullpath, "rb");
            if (!infile) {
                perror("opening input file");
                exit(-1);
            }

            size = filetable[i].size;
            while (size) {
                w = size > FILE_BLOCK_SIZE ? FILE_BLOCK_SIZE : size;
                fread(buf, 1, w, infile);
                fwrite(buf, 1, FILE_BLOCK_SIZE, outfile) ;
                size -= w;
            }
            fclose(infile);
       }
    }
    
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
    //recu
    processdir(dirp, "", dirname, scope_count, file_count);
    file_count ++ ; //add root
    //sort
    qsort(filetable, file_count, sizeof(Index), cmp);
    //calculate block index
    calculate_block();
    //write to output
    write_output(outfile);
    fwrite(&z, 1, 8, outfile);

    //print_filetable();
    if (outname)
        fclose(outfile);
    closedir(dirp);
    
    return 0;
}
