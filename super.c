/*
 * Location Based Filesystem
 *
 * By, Robert Chrystie
 */

#include <linux/buffer_head.h>

#include "internal.h"

/* Start of the device, super_block is stored here */
static const uint64_t LOCFS_SUPER_BLOCK_BLOCK_NO = 0;

static const struct super_operations locfs_sb_ops = {
    .destroy_inode  = locfs_destroy_inode,
};

/* Function called from mount_bdev() */
static int locfs_fill_super(struct super_block *sb, 
                              void *data, 
                              int silent) 
{
    struct inode *root_inode;
    struct locfs_inode *root_locfs_inode;
    struct buffer_head *bh;
    struct locfs_super_block *locfs_sb;
    int ret = 0;

    // Read the block containint the super_block
    bh = sb_bread(sb, LOCFS_SUPER_BLOCK_BLOCK_NO);
    BUG_ON(!bh);
    locfs_sb = (struct locfs_super_block *)bh->b_data;

    // Verify that this is a locfs by comparing the magic numbers
    if (unlikely(locfs_sb->magic != LOCFS_MAGIC)) {
        printk(KERN_ERR
               "Locfs magic number mismatch: %llu != %llu\n",
               locfs_sb->magic, (uint64_t)LOCFS_MAGIC);
        goto release;
    }

    if (unlikely(sb->s_blocksize != locfs_sb->blocksize)) {
        printk(KERN_ERR
               "Locfs formatted with mismatching blocksize: %lu\n",
               sb->s_blocksize);
        goto release;
    }

    // Take the data from the device and write it to the super_block
    sb->s_magic = locfs_sb->magic;
    sb->s_fs_info = locfs_sb;
    sb->s_maxbytes = locfs_sb->blocksize;
    sb->s_op = &locfs_sb_ops;

    // Time to setup the root inode, get it from the device
    root_locfs_inode = locfs_get_locfs_inode(sb, LOCFS_ROOTDIR_INODE_NO);
    root_inode = new_inode(sb);
    if (!root_inode || !root_locfs_inode) {
        ret = -ENOMEM;
        goto release;
    }
    locfs_fill_inode(sb, root_inode, root_locfs_inode);
    inode_init_owner(root_inode, NULL, root_inode->i_mode);

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        ret = -ENOMEM;
        goto release;
    }

release:
    brelse(bh);
    return ret;
}

/* Mounting function used in file_system_type.mount */
struct dentry *locfs_mount(struct file_system_type *fs_type,
                             int flags, 
                             const char *dev_name,
                             void *data) 
{
    struct dentry *ret;
    ret = mount_bdev(fs_type, flags, dev_name, data, locfs_fill_super);

    if (unlikely(IS_ERR(ret))) {
        printk(KERN_ERR "Error mounting locfs.\n");
    } else {
        printk(KERN_INFO "locfs is succesfully mounted on: %s\n", dev_name);
    }

    return ret;
}

/* Save the super_block back to the device */
void locfs_save_sb(struct super_block *sb) 
{
    struct buffer_head *bh;
    struct locfs_super_block *locfs_sb = LOCFS_SB(sb);

    bh = sb_bread(sb, LOCFS_SUPER_BLOCK_BLOCK_NO);
    BUG_ON(!bh);

    bh->b_data = (char *)locfs_sb;
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
}
