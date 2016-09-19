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

void catalogDir(uint8_t *in, char *path);
void copyFat();
void decode(int a, int b, int c, int arr[2]);
uint8_t *findCluster(int FAT);
void printFat();
void printArr();
void newFile();
void writeCluster(int fat, FILE *file, size_t size);

typedef struct dir {
    char filename[8];   //the name of the file/directory     
    char ext[3];        //the extension on the file
    char attributes;    //the attributes bit
    char firstlog;      //first logical cluster
    int filesize;       //size of the file
    bool delete;        //if the file has been deleted
    bool dir;           //if the entry is a directory
    char filepath[1000];
}directory_t;

int fat[384];           //a copy of the decoded FAT
directory_t *arr[224];  //an array for our directory entries
int count;              //count of the number of entries in our array, includes directories themselves
int filenum;            //the number of files created

char *arg;              //destination folder given to us via argv[2]
uint8_t *mem_block;     //the initial memory block

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
	
	
    arg = (char *)argv[2];
    mem_block = mmap(NULL, sb.st_blocks*SECTOR_SIZE, PROT_READ | PROT_WRITE,
				MAP_PRIVATE, infile, 0);                                
    assert(mem_block != MAP_FAILED);
    
    copyFat();
    catalogDir(mem_block + 0x200 + 0x1200 + 0x1200, "");
    printArr();
    for(int i = 0; i<count; i++) {
        newFile(arr[i]);
    }
}

//accepts the starting address for a directory and traverses each file in the directory
//stores the results in a global array
void catalogDir(uint8_t *in, char *pathin) {
    uint8_t *ptr = in;
    directory_t *dir;
    int array[32];

    //directory is null terminated
    while(*ptr != 0x00) {
        dir = malloc(sizeof(directory_t));  //directory to store our files in
        int len = 8;                        //size of the filename
        
        //stores the directory entry in a temporary array
        for (int i=0; i<32; i++){
            array[i] = (int)*ptr&0xff;
            ptr++;
        }
        
        //stores the filename and checks if it is a deleted file
        for(int i = 0; i < 8; i++) {
            dir->filename[i] = array[i];
            
            //if first byte of the array is 0xe5, then it was "deleted"
            if(array[0] == 0xe5) {
                dir->delete = true;
                dir->filename[0] = '_';
            }
            else
                dir->delete = false;
            
            //checks if the filename is less than 8 and exits the for loop if it is
            if(array[i+1] == 0x20) {
                i = 8;
                len = i;
            }
        }
        
        //copies the extension into struct
        for(int i = 8; i < 11; i++) {
            dir->ext[i-8] = array[i];
            if(array[i+1] == 0x20)
                i = 11;
        }
        
        //stores the attributes byte
        dir->attributes = array[11];
        
        //stores the index of the first logical cluster 
        dir->firstlog = array[26]|((array[27]&0xf)<<8);
        
        //size is stored backwards, so two temp array are created that combines the 4 byte into one size
        int size[4];
        for(int i = 28; i < 32; i++) {
            size[i-28] = array[i];
        }
        int temp[4];
        temp[0] = size[3];
        temp[1] = size[2];
        temp[2] = size[1];
        temp[3] = size[0];
        dir->filesize = (temp[0]<<24)|(temp[1]<<16)|(temp[2]<<8)|temp[3];
        
        //adds the new directory entry into a global array
        arr[count] = dir;
        count++;
        
        //directory entry assumes not a directory until proven otherwise
        dir->dir = false;
        
        //stores the current filename as well as any parent directories
        sprintf(dir->filepath, "%s/", pathin);
        strncat(dir->filepath, dir->filename, len);
        
        //checks to see if the current entry is a subdirectory
        //if it is, we recursively calls this function
        if(((int)dir->attributes&0xf0) == 0x10) {
            dir->dir = true;
            uint8_t *clust = findCluster(dir->firstlog);
            //must skip the first two entries, becuase they are . and .. respectively
            clust += 64;
            catalogDir(clust, dir->filepath);
        }
    }
}

//accepts the starting address for the FAT, decodes it
//copies it into a global array
void copyFat(uint8_t *in) {
    int loc = 0;
    int arr[2];
    uint8_t *temp = mem_block + 512;
    while(loc < 384) {
        decode((int)*temp, (int)*(temp+1), (int)*(temp+2), arr);
        fat[loc] = arr[0];
        fat[loc+1] = arr[1];
        loc += 2;
        temp += 3;
    }
    return;
}

//accepts 3 ints to decode into FAT12 ints, stores it in a given array
void decode(int a, int b, int c, int arr[2]) {
    int mask1 = 0xf0;
    int mask2 = 0x0f;
    int ret1 = a|((b&mask2)<<8);
    int ret2 = (c<<4)|((b&mask1)>>4);
    arr[0] = ret1;
    arr[1] = ret2;
    return;
}

//returns the starting address of a cluster given the FAT12 index
uint8_t *findCluster(int FAT) {
    int sector = 33 + FAT - 2;
    return mem_block + (sector*512);
}

//prints the FAT
//used for debugging purposes
void printFat() {
    for(int i = 0; i < 384; i++) {
        printf("%x   ", fat[i]);
        if ((i+1)%10 ==0){
            printf("\n");
        }
    }
    printf("\n");
}

//prints all of the files in an array
//does not print directories
void printArr() {
    char path[1024];
    memset(path, 0, 1024);
    
    //walks through our array of entries
    for(int i = 0; i < count; i++) {
        //stores the entry's path and extension
        path[0] = ' ';
        strcat(path, arr[i]->filepath);
        strcat(path, ".");
        strncat(path, arr[i]->ext, 3);
        
        //checks if it is a "deleted" entry
        if(arr[i]->delete) {
            printf("FILE\tDELETED\t%s\t%d", path, arr[i]->filesize);
            //saves the filepath and extension for easy retrieval
            strcpy(arr[i]->filepath, path);
            printf("\n");
        }
        else {
            //does not print directories
            if(!arr[i]->dir) {
                printf("FILE\tNORMAL\t%s\t%d", path, arr[i]->filesize);
                strcpy(arr[i]->filepath, path);
                printf("\n");
            }
        }
        memset(path, 0, 1024);
    }
}

//creates a new file for the passed in entry that is not a directory
//destination passed in via argv[2]
void newFile(directory_t *dir) {
    if(!dir->dir) {
        FILE *file;
        char name[13];
        char buff[1000];
        
        //specifies destination and filename, which is just a count of the total entries
        sprintf(buff, "%s/", arg);
        sprintf(name, "file%d.%.3s", filenum, dir->ext);
        strcat(buff, name);
        
        //opens file to write to it
        file = fopen(buff, "w");
        
        //starting index is the first logical cluster
        int fatndx = dir->firstlog;
        int sizerem = dir->filesize;
        
        //checks to see if size of file is greater than one cluster
        if(sizerem>=SECTOR_SIZE) {
            writeCluster(fatndx, file, SECTOR_SIZE);
            sizerem -= SECTOR_SIZE;
        }
        else {
            writeCluster(fatndx, file, sizerem);
            sizerem = 0;
        }
        
        //if not a deleted file, we can just step through the FAT until we encounter and EOF marker
        if(!dir->delete) {
            while(fat[fatndx] != 0xfff && sizerem > 0) {
                fatndx = fat[fatndx];
                
                //again, checks to see if file is greater than one cluster
                if(sizerem>=SECTOR_SIZE) {
                    writeCluster(fatndx, file, SECTOR_SIZE);
                    sizerem -= SECTOR_SIZE;
                }
                else {
                    writeCluster(fatndx, file, sizerem);
                    sizerem = 0;
                }
            }
        }
        //else, it is a deleted file
        //and we have to just guess if the next FAT entry is a part of the file
        else {
            while(fat[fatndx+1] == 0x000 && sizerem > 0) {
                fatndx++;
                
                //last check to see if the remaining size is greater than one cluster
                if(sizerem>=SECTOR_SIZE) {
                    writeCluster(fatndx, file, SECTOR_SIZE);
                    sizerem -= SECTOR_SIZE;
                }
                else {
                    writeCluster(fatndx, file, sizerem);
                    sizerem = 0;
                }
            }
        }
        //increments the total number of files, for naming purposes
        filenum++;
    }
}

//writes the contents for a cluster at a specified FAt entry into a give file
void writeCluster(int fat, FILE *file, size_t size) {
    uint8_t *clust = findCluster(fat);
    fwrite(clust, size, sizeof(char), file);
}
