#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#define SECTOR_SIZE 512

void catalogDir(uint8_t *in);
void copyFat(uint8_t *in);
void countDir(uint8_t *in);
void decode(int a, int b, int c, int arr[2]);
void printFat();
void printArr();
void writeArr(const char *argv[]);

typedef struct dir {
    char filename[8];
    char ext[3];
    char attributes;
    char firstlog[2];
    char filesize[4];
    bool delete;
}directory_t;

int fat[384];
directory_t *arr[224];
int count;

int main(int argc, const char *argv[])
{
    struct stat sb;    
    
	count = 0;
    int infile = open(argv[1], O_RDONLY);
    if (infile == -1) {
		fprintf(stderr, "Unable to open file.\n");
		exit(1);
	}
	
	if (stat(argv[1], &sb) == -1) {
	    perror("Stat failed!\n");
	    exit(EXIT_FAILURE);
	}
	
	uint8_t *mem_block = mmap(NULL, sb.st_blocks*SECTOR_SIZE, PROT_READ | PROT_WRITE,
								MAP_PRIVATE, infile, 0);
	                                
    assert(mem_block != MAP_FAILED);
    copyFat(mem_block + 512);
    catalogDir(mem_block + 0x200 + 0x1200 + 0x1200);
    printArr();
    //printFat();
}

//accepts the starting address for a directory and locates the first logical cluster
//for each entry?
//returns an array?
void catalogDir(uint8_t *in) {
    countDir(in);
    uint8_t *ptr = in;
    int temp = 0;
    directory_t *dir;
    int array[32];

    while(temp < count) {
        dir = malloc(sizeof(directory_t));
        for (int i=0; i<32; i++){
            array[i] = (int)*ptr&0xff;
            ptr ++;
        }
        for(int i = 0; i < 8; i++) {
            dir->filename[i] = array[i];
            if(array[0] == 0xe5) {
                dir->delete = true;
                dir->filename[0] = '_';
            }
            else
                dir->delete = false;
        }
        for(int i = 8; i < 11; i++) {
            dir->ext[i-8] = array[i];
        }
        dir->attributes = array[11];
        for(int i = 26; i < 28; i++) {
            dir->firstlog[i-26] = array[i];
        }
        for(int i = 28; i < 32; i++) {
            dir->filesize[i-28] = array[i];
        }
        arr[temp] = dir;
        temp++;
    }
}

//accepts the starting address for the FAT, decodes it
//copies it into an array
void copyFat(uint8_t *in) {
    int loc = 0;
    int arr[2];
    uint8_t *temp = in;
    while(loc < 384) {
        decode((int)*temp, (int)*(temp+1), (int)*(temp+2), arr);
        fat[loc] = arr[0];
        fat[loc+1] = arr[1];
        loc += 2;
        temp += 3;
    }
    return;
}

//starts at the beginning of the root dir and counts number of files in dir
//DOES NOT COUNT SUBDIRECTORIES
//ALSO DOESN'T COUNT CORRECTLY????
void countDir(uint8_t *in) {
	uint8_t *ptr = in;
	while(*ptr != 0x00 && count < 224) {
		count++;
		ptr += 32;
	}
	return;
}

//FOR SURE THIS WORKS
//accepts 3 ints to decode into FAT12 ints, stores it in array
void decode(int a, int b, int c, int arr[2]) {
    int mask1 = 0xf0;
    int mask2 = 0x0f;
    int ret1 = a|((b&mask2)<<8);
    int ret2 = (c<<4)|((b&mask1)>>4);
    arr[0] = ret1;
    arr[1] = ret2;
    return;
}

void printFat() {
    for(int i = 0; i < 384; i++) {
        printf("%x   ", fat[i]);
        if ((i+1)%10 ==0){
            printf("\n");
        }
    }
    printf("\n");
}

void printArr() {
    char path[1024];
    memset(path, 0, 1024);
    int size;
    for(int i = 0; i < count; i++) {
        path[0] = ' ';
        strcat(path, "/");
        strncat(path, arr[i]->filename, 8);
        strcat(path, ".");
        strncat(path, arr[i]->ext, 3);
        size = (int)*arr[i]->filesize & 0xffffffff;
        if(arr[i]->delete) {
            printf("FILE\tDELETED\t%s\t%d", path, size);
        }
        else {
            printf("FILE\tNORMAL\t%s\t%d", path, size);
        }
        printf("\n");
        memset(path, 0, 1024);
    }
}

void writeArr(const char *argv[]) {
    
}
