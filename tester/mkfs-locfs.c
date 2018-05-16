#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../include/locfs.h"

#define LOCFS_DEFAULT_BLOCKSIZE 4096
#define LOCFS_DEFAULT_INODE_TABLE_SIZE 1024
#define LOCFS_DEFAULT_DATA_BLOCK_TABLE_SIZE 1024

static const uint64_t LOCFS_ROOTDIR_DATA_BLOCK_NO_OFFSET = 0;

int main(int argc, char *argv[]) {
    int fd;
    uint64_t welcome_inode_no;
    uint64_t welcome_data_block_no_offset;

    fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("Error opening the device");
        return -1;
    }

    // Create the locfs_super_block
    struct locfs_super_block locfs_sb = {
        .version = 1,
        .magic = LOCFS_MAGIC,
        .blocksize = LOCFS_DEFAULT_BLOCKSIZE,
        .inode_table_size = LOCFS_DEFAULT_INODE_TABLE_SIZE,
        .inode_count = 2,
        .data_block_table_size = LOCFS_DEFAULT_DATA_BLOCK_TABLE_SIZE,
        .data_block_count = 2,
    };

    // write super block
    if (sizeof(locfs_sb) != write(fd, &locfs_sb, sizeof(locfs_sb))) {
        return -1;
    }

    // construct inode bitmap
    char inode_bitmap[locfs_sb.blocksize];
    memset(inode_bitmap, 0, sizeof(inode_bitmap));
    inode_bitmap[0] = 1;

    // write inode bitmap
    if (sizeof(inode_bitmap) != write(fd, inode_bitmap, sizeof(inode_bitmap))) {
        return -1;
    }

    // construct data block bitmap
    char data_block_bitmap[locfs_sb.blocksize];
    memset(data_block_bitmap, 0, sizeof(data_block_bitmap));
    data_block_bitmap[0] = 1;

    // write data block bitmap
    if (sizeof(data_block_bitmap) != 
            write(fd, data_block_bitmap, sizeof(data_block_bitmap))) {
        return -1;
    }

    // construct root inode
    struct locfs_inode root_locfs_inode = {
        .mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH,
        .inode_no = LOCFS_ROOTDIR_INODE_NO,
        .data_block_no 
            = LOCFS_DATA_BLOCK_TABLE_START_BLOCK_NO_HSB(&locfs_sb)
                + LOCFS_ROOTDIR_DATA_BLOCK_NO_OFFSET,
        .dir_children_count = 1,
    };

    // write root inode
    if (sizeof(root_locfs_inode)
            != write(fd, &root_locfs_inode,
                     sizeof(root_locfs_inode))) {
        return -1;
    }

    close(fd);
    return 0;
}
