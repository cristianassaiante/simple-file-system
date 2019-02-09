#include <disk_driver.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>     


void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks) {
    int ret;
    int exists = access(filename, F_OK) != -1;
    int fd = open(filename, O_CREAT | O_RDWR, 0600);
    if (fd == -1) {
        fprintf(stderr, "[DD - init] open failed.");
        exit(EXIT_FAILURE);
    }


    int bitmap_size = num_blocks >> 3;
    if (num_blocks & 0x7) bitmap_size ++;

    int zone_size = sizeof(DiskHeader) + bitmap_size * BLOCK_SIZE * num_blocks;
    void * zone = mmap(0, zone_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!zone) {
        fprintf(stderr, "[DD - init] mmap failed.");
        ret = close(fd);
        if (ret == -1) 
            fprintf(stderr, "[DD -init] close failed.");
        exit(EXIT_FAILURE);
    }

    disk->header = zone;
    disk->bitmap_data = (char*)zone + bitmap_size;
    disk->fd = fd;

    if (exists) {
        if (disk->header->num_blocks != num_blocks) { 
            fprintf(stderr, "[DD - init] trying to use an existing disk of different size.");
            ret = close(fd);
            if (ret == -1) 
                fprintf(stderr, "[DD -init] close failed.");
            ret = munmap(zone, zone_size);
            if (ret == -1)
                fprintf(stderr, "[DD - init] munmap failed.");
            exit(EXIT_FAILURE);
        }
    } 
    else {
        disk->header->num_blocks = num_blocks;
        disk->header->bitmap_blocks = num_blocks;
        disk->header->bitmap_entries = bitmap_size;
        disk->header->free_blocks = num_blocks;
        disk->header->first_free_block = 0;

        bzero(disk->bitmap_data, bitmap_size);
    }
}

int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num) {
    if (block_num >= disk->header->num_blocks)
        return -1;

    BitMap bmap = {
        .num_bits = disk->header->num_blocks,
        .entries = disk->bitmap_data
    };
    if (BitMap_get(&bmap, block_num, 0) == block_num)
        return -1;
    
    void * blocks_start = disk->bitmap_data + disk->header->bitmap_entries;
    memcpy(dest, blocks_start + block_num * BLOCK_SIZE, BLOCK_SIZE);
    return 0;
}

int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num) {
    if (block_num >= disk->header->num_blocks)
        return -1;
    
    if (block_num == disk->header->first_free_block) 
        disk->header->first_free_block = DiskDriver_getFreeBlock(disk, block_num);

    BitMap bmap = {
        .num_bits = disk->header->num_blocks,
        .entries = disk->bitmap_data
    };
    if (BitMap_set(&bmap, block_num, 1) == -1)
        return -1;

    void * blocks_start = disk->bitmap_data + disk->header->bitmap_entries;
    memcpy(blocks_start + block_num * BLOCK_SIZE, src, BLOCK_SIZE);
    return 0;
}

int DiskDriver_freeBlock(DiskDriver* disk, int block_num) {
    if (block_num >= disk->header->num_blocks)
        return -1;

    BitMap bmap = {
        .num_bits = disk->header->num_blocks,
        .entries = disk->bitmap_data
    };
    if (BitMap_set(&bmap, block_num, 0) == -1)
        return -1;

    disk->header->free_blocks ++;
    disk->header->first_free_block = block_num < disk->header->first_free_block ? 
                                        block_num : disk->header->first_free_block;
    return 0;
}

int DiskDriver_getFreeBlock(DiskDriver* disk, int start) {
    if (start >= disk->header->num_blocks)
        return -1;
    
    BitMap bmap = {
        .num_bits = disk->header->num_blocks,
        .entries = disk->bitmap_data
    };
    return BitMap_get(&bmap, start, 0);
}

int DiskDriver_flush(DiskDriver* disk) {
    int ret;
    int zone_size = sizeof(DiskHeader) + disk->header->bitmap_entries +  
                        disk->header->num_blocks * BLOCK_SIZE;
    ret = msync(disk->header, zone_size, MS_SYNC);
    if (ret == -1)
        return -1;
    return 0;
}