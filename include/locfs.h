#ifndef __LOCFS_H__
#define __LOCFS_H__

#define LOCFS_MAGIC 0x050505
#define LOCFS_FILENAME_MAXLEN 255
#define LOCFS_LOCATION_MAXLEN 255

static const uint64_t LOCFS_INODE_TABLE_START_BLOCK_NO = 3;
static const uint64_t LOCFS_ROOTDIR_INODE_NO = 0;

/* Define filesystem structures */
struct locfs_dir_record {
    char filename[LOCFS_FILENAME_MAXLEN];
    uint64_t inode_no;
};

struct locfs_inode {
    mode_t mode;
    uint64_t inode_no;
    uint64_t data_block_no;

    char location[LOCFS_LOCATION_MAXLEN];

    union {
        uint64_t file_size;
        uint64_t dir_children_count;
    };
};

struct locfs_super_block {
    uint64_t version;
    uint64_t magic;
    uint64_t blocksize;

    uint64_t inode_table_size;
    uint64_t inode_count;

    uint64_t data_block_table_size;
    uint64_t data_block_count;
};

/* Helper functions */
static inline uint64_t LOCFS_INODES_PER_BLOCK_HSB(struct locfs_super_block *locfs_sb) 
{
    return locfs_sb->blocksize / sizeof(struct locfs_inode);
}

static inline uint64_t LOCFS_DATA_BLOCK_TABLE_START_BLOCK_NO_HSB(struct locfs_super_block *locfs_sb) 
{
    return LOCFS_INODE_TABLE_START_BLOCK_NO
           + locfs_sb->inode_table_size / LOCFS_INODES_PER_BLOCK_HSB(locfs_sb)
           + 1;
}

#endif /*__LOCFS_H__*/
