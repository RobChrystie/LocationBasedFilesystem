/*
 * Location Based Filesystem
 *
 * By, Robert Chrystie
 */

#include "include/locfs.h"

/* main.c */
extern struct kmem_cache *locfs_inode_cache;

/* file.c */
extern const struct file_operations locfs_file_operations;

/* super.c */
struct dentry *locfs_mount(struct file_system_type *fs_type,
                             int flags, 
                             const char *dev_name,
                             void *data);

void locfs_save_sb(struct super_block *sb);

/* inode.c */
void locfs_save_locfs_inode(struct super_block *sb,
                              struct locfs_inode *inode);

struct locfs_inode *locfs_get_locfs_inode(struct super_block *sb,
                                            uint64_t inode_no);

void locfs_fill_inode(struct super_block *sb, 
                        struct inode *inode,
                        struct locfs_inode *locfs_inode);

void locfs_destroy_inode(struct inode *inode);

int locfs_mkdir(struct inode *dir, struct dentry *dentry,
                   umode_t mode);

/* Helper functions */
/* Used to get the locfs_super_block out of the super_block */
static inline struct locfs_super_block *LOCFS_SB(struct super_block *sb) 
{
    return sb->s_fs_info;
}

/* Used to get the locfs_inode out of the super_block */
static inline struct locfs_inode *LOCFS_INODE(struct inode *inode) 
{
    return inode->i_private;
}

/* Used to retrieve the number of inodes in a block */
static inline uint64_t LOCFS_INODES_PER_BLOCK(struct super_block *sb) 
{
    struct locfs_super_block *locfs_sb;
    locfs_sb = LOCFS_SB(sb);
    return LOCFS_INODES_PER_BLOCK_HSB(locfs_sb);
}
