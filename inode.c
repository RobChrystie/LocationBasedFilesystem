/*
 * Location Based Filesystem
 *
 * By, Robert Chrystie
 */

#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/string.h>

#include "include/locfs.h"
#include "internal.h"

DEFINE_MUTEX(locfs_sb_lock);

#define BITS_IN_BYTE 8

static const uint64_t LOCFS_INODE_BITMAP_BLOCK_NO = 1;
static const uint64_t LOCFS_DATA_BLOCK_BITMAP_BLOCK_NO = 2;

extern char *curr_location;

/* Finds the starting block of the data blocks */
static inline uint64_t LOCFS_DATA_BLOCK_TABLE_START_BLOCK_NO(struct super_block *sb) 
{
    struct locfs_super_block *locfs_sb;
    locfs_sb = LOCFS_SB(sb);
    return LOCFS_DATA_BLOCK_TABLE_START_BLOCK_NO_HSB(locfs_sb);
}

static inline uint64_t LOCFS_DIR_MAX_RECORD(struct super_block *sb) 
{
    struct locfs_super_block *locfs_sb;
    locfs_sb = LOCFS_SB(sb);
    return locfs_sb->blocksize / sizeof(struct locfs_dir_record);
}

static inline uint64_t LOCFS_INODE_BYTE_OFFSET(struct super_block *sb, 
                                                 uint64_t inode_no) 
{
    struct locfs_super_block *locfs_sb;
    locfs_sb = LOCFS_SB(sb);
    return (inode_no % LOCFS_INODES_PER_BLOCK_HSB(locfs_sb)) * sizeof(struct locfs_inode);
}

int locfs_add_dir_record(struct super_block *sb, struct inode *dir,
                           struct dentry *dentry, struct inode *inode) {
    struct buffer_head *bh;
    struct locfs_inode *parent_locfs_inode;
    struct locfs_dir_record *dir_record;

    parent_locfs_inode = LOCFS_INODE(dir);
    if (unlikely(parent_locfs_inode->dir_children_count
            >= LOCFS_DIR_MAX_RECORD(sb))) {
        return -ENOSPC;
    }

    bh = sb_bread(sb, parent_locfs_inode->data_block_no);
    BUG_ON(!bh);

    dir_record = (struct locfs_dir_record *)bh->b_data;
    dir_record += parent_locfs_inode->dir_children_count;
    dir_record->inode_no = inode->i_ino;
    strcpy(dir_record->filename, dentry->d_name.name);

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    parent_locfs_inode->dir_children_count += 1;
    locfs_save_locfs_inode(sb, parent_locfs_inode);

    return 0;
}

static int locfs_alloc_locfs_inode(struct super_block *sb, 
                                     uint64_t *out_inode_no) 
{
    struct locfs_super_block *locfs_sb;
    struct buffer_head *bh;
    uint64_t i;
    int ret;
    char *bitmap;
    char *slot;
    char needle;

    locfs_sb = LOCFS_SB(sb);

    mutex_lock(&locfs_sb_lock);

    bh = sb_bread(sb, LOCFS_INODE_BITMAP_BLOCK_NO);
    BUG_ON(!bh);

    bitmap = bh->b_data;
    ret = -ENOSPC;
    for (i = 0; i < locfs_sb->inode_table_size; i++) {
        slot = bitmap + i / BITS_IN_BYTE;
        needle = 1 << (i % BITS_IN_BYTE);
        if (0 == (*slot & needle)) {
            *out_inode_no = i;
            *slot |= needle;
            locfs_sb->inode_count += 1;
            ret = 0;
            break;
        }
    }

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    locfs_save_sb(sb);

    mutex_unlock(&locfs_sb_lock);
    return ret;
}

int locfs_alloc_data_block(struct super_block *sb, uint64_t *out_data_block_no) {
    struct locfs_super_block *locfs_sb;
    struct buffer_head *bh;
    uint64_t i;
    int ret;
    char *bitmap;
    char *slot;
    char needle;

    locfs_sb = LOCFS_SB(sb);

    mutex_lock(&locfs_sb_lock);

    bh = sb_bread(sb, LOCFS_DATA_BLOCK_BITMAP_BLOCK_NO);
    BUG_ON(!bh);

    bitmap = bh->b_data;
    ret = -ENOSPC;
    for (i = 0; i < locfs_sb->data_block_table_size; i++) {
        slot = bitmap + i / BITS_IN_BYTE;
        needle = 1 << (i % BITS_IN_BYTE);
        if (0 == (*slot & needle)) {
            *out_data_block_no = LOCFS_DATA_BLOCK_TABLE_START_BLOCK_NO(sb) + i;
            *slot |= needle;
            locfs_sb->data_block_count += 1;
            ret = 0;
            break;
        }
    }

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    locfs_save_sb(sb);

    mutex_unlock(&locfs_sb_lock);
    return ret;
}

int locfs_create_inode(struct inode *dir, struct dentry *dentry,
                         umode_t mode) {
    struct super_block *sb;
    struct locfs_super_block *locfs_sb;
    uint64_t inode_no;
    struct locfs_inode *locfs_inode;
    struct inode *inode;
    int ret;

    printk(KERN_INFO "locfs: in locfs_create_inode");

    sb = dir->i_sb;
    locfs_sb = LOCFS_SB(sb);

    /* Create locfs_inode */
    ret = locfs_alloc_locfs_inode(sb, &inode_no);
    if (0 != ret) {
        printk(KERN_ERR "Unable to allocate on-disk inode. "
                        "Is inode table full? "
                        "Inode count: %llu\n",
                        locfs_sb->inode_count);
        return -ENOSPC;
    }
    locfs_inode = kmem_cache_alloc(locfs_inode_cache, GFP_KERNEL);
    locfs_inode->inode_no = inode_no;
    locfs_inode->mode = mode;
    if (S_ISDIR(mode)) {
        locfs_inode->dir_children_count = 0;
    } else if (S_ISREG(mode)) {
        locfs_inode->file_size = 0;
    } else {
        printk(KERN_WARNING
               "Inode %llu is neither a directory nor a regular file",
               inode_no);
    }

    // Add the current location data
    strcpy(locfs_inode->location, curr_location);
    printk(KERN_INFO "Tagged file location %s", locfs_inode->location);

    /* Allocate data block for the new locfs_inode */
    ret = locfs_alloc_data_block(sb, &locfs_inode->data_block_no);
    if (0 != ret) {
        printk(KERN_ERR "Unable to allocate on-disk data block. "
                        "Is data block table full? "
                        "Data block count: %llu\n",
                        locfs_sb->data_block_count);
        return -ENOSPC;
    }

    /* Create VFS inode */
    inode = new_inode(sb);
    if (!inode) {
        return -ENOMEM;
    }
    locfs_fill_inode(sb, inode, locfs_inode);

    /* Add new inode to parent dir */
    ret = locfs_add_dir_record(sb, dir, dentry, inode);
    if (0 != ret) {
        printk(KERN_ERR "Failed to add inode %lu to parent dir %lu\n",
               inode->i_ino, dir->i_ino);
        return -ENOSPC;
    }

    inode_init_owner(inode, dir, mode);
    d_add(dentry, inode);

    /* TODO we should free newly allocated inodes when error occurs */

    return 0;
}

static int locfs_create(struct inode *dir, 
                          struct dentry *dentry,
                          umode_t mode, 
                          bool excl) 
{
    return locfs_create_inode(dir, dentry, mode);
}

int locfs_mkdir(struct inode *dir, 
                  struct dentry *dentry,
                  umode_t mode) 
{
    printk(KERN_INFO "locfs: in locfs_mkdir");

    // Set the mode explicitly to a directory
    mode |= S_IFDIR;
    return locfs_create_inode(dir, dentry, mode);
}

struct dentry *locfs_lookup(struct inode *dir,
                              struct dentry *child_dentry,
                              unsigned int flags) 
{
    struct locfs_inode *parent_locfs_inode = LOCFS_INODE(dir);
    struct super_block *sb = dir->i_sb;
    struct buffer_head *bh;
    struct locfs_dir_record *dir_record;
    struct locfs_inode *locfs_child_inode;
    struct inode *child_inode;
    uint64_t i;

    printk(KERN_INFO "locfs: in locfs_lookup");

    bh = sb_bread(sb, parent_locfs_inode->data_block_no);
    BUG_ON(!bh);

    dir_record = (struct locfs_dir_record *)bh->b_data;

    for (i = 0; i < parent_locfs_inode->dir_children_count; i++) {
        printk(KERN_INFO "locfs_lookup: i=%llu, dir_record->filename=%s, child_dentry->d_name.name=%s", i, dir_record->filename, child_dentry->d_name.name);
        if (strcmp(dir_record->filename, child_dentry->d_name.name) == 0) {
            locfs_child_inode = locfs_get_locfs_inode(sb, dir_record->inode_no);
            printk(KERN_INFO "locfs: %s", locfs_child_inode->location);
            child_inode = new_inode(sb);
            if (!child_inode) {
                printk(KERN_ERR "Cannot create new inode. No memory.\n");
                return NULL; 
            }
            locfs_fill_inode(sb, child_inode, locfs_child_inode);
            inode_init_owner(child_inode, dir, locfs_child_inode->mode);
            d_add(child_dentry, child_inode);
            return NULL;    
        }
        dir_record++;
    }

    printk(KERN_ERR
           "No inode found for the filename: %s\n",
           child_dentry->d_name.name);
    return NULL;
}

static const struct inode_operations locfs_inode_ops = {
    .create = locfs_create,
    .mkdir  = locfs_mkdir,
    .lookup = locfs_lookup,
};

// Given the inode_no, calcuate which block in inode table contains the corresponding inode
static inline uint64_t LOCFS_INODE_BLOCK_OFFSET(struct super_block *sb, uint64_t inode_no) 
{
    struct locfs_super_block *locfs_sb;
    locfs_sb = LOCFS_SB(sb);
    return inode_no / LOCFS_INODES_PER_BLOCK_HSB(locfs_sb);
}

/* Called from locfs_dir_operations.iterate , called from ls in user space */
int locfs_iterate(struct file *filp, 
                    struct dir_context *ctx)
{
	loff_t pos;
	struct inode *inode;
	struct super_block *sb;
	struct buffer_head *bh;
	struct locfs_inode *lfs_inode;
	struct locfs_dir_record *record;
	int i;

    printk(KERN_INFO "In locfs_iterate");

	pos = ctx->pos;
	inode = filp->f_inode;
	sb = inode->i_sb;

	if (pos) {
		return 0;
	}

	lfs_inode = LOCFS_INODE(inode);

    // Check to make sure this is a directory
	if (unlikely(!S_ISDIR(lfs_inode->mode))) {
		printk(KERN_ERR "inode [%llu][%lu] for fs object not a directory\n",
		       lfs_inode->inode_no, inode->i_ino);
		return -ENOTDIR;
	}

    // Read this inode
	bh = sb_bread(sb, lfs_inode->data_block_no);
	BUG_ON(!bh);

	record = (struct locfs_dir_record *)bh->b_data;

	for (i = 0; i < lfs_inode->dir_children_count; i++) {
        // Compare to see if this file was saved at the current location   
        printk(KERN_INFO "locfs: li_gps_data == %s", lfs_inode->location);
        if (strcmp(lfs_inode->location, curr_location) == 0) {
	        dir_emit(ctx, record->filename, LOCFS_FILENAME_MAXLEN, record->inode_no, DT_UNKNOWN);
        	ctx->pos += sizeof(struct locfs_dir_record);
        	pos += sizeof(struct locfs_dir_record);
        	record++;
        } else {
            printk(KERN_INFO "File %s not in %s location", record->filename, curr_location);
        }
	}
	brelse(bh);

	return 0;
}

static const struct file_operations locfs_dir_operations = {
    .owner   = THIS_MODULE,
    .iterate = locfs_iterate,
};

/* Used in super.c for the super_operations */
void locfs_destroy_inode(struct inode *inode) {
    struct locfs_inode *locfs_inode = LOCFS_INODE(inode);

    printk(KERN_INFO "Freeing private data of inode %p (%lu)\n",
           locfs_inode, inode->i_ino);
    kmem_cache_free(locfs_inode_cache, locfs_inode);
}

void locfs_fill_inode(struct super_block *sb, struct inode *inode,
                        struct locfs_inode *locfs_inode) {
    inode->i_mode = locfs_inode->mode;
    inode->i_sb = sb;
    inode->i_ino = locfs_inode->inode_no;
    inode->i_op = &locfs_inode_ops;    
    inode->i_atime = inode->i_mtime 
                   = inode->i_ctime
                   = CURRENT_TIME;
    inode->i_private = locfs_inode;    
    
    if (S_ISDIR(locfs_inode->mode)) { 
        inode->i_fop = &locfs_dir_operations;
    } else if (S_ISREG(locfs_inode->mode)) {
        inode->i_fop = &locfs_file_operations;
    } else {
        printk(KERN_WARNING
               "Inode %lu is neither a directory nor a regular file",
               inode->i_ino);
        inode->i_fop = NULL;
    }
}

struct locfs_inode *locfs_get_locfs_inode(struct super_block *sb,
                                                uint64_t inode_no) {
    struct buffer_head *bh;
    struct locfs_inode *inode;
    struct locfs_inode *inode_buf;

    bh = sb_bread(sb, LOCFS_INODE_TABLE_START_BLOCK_NO + LOCFS_INODE_BLOCK_OFFSET(sb, inode_no));
    BUG_ON(!bh);
    
    inode = (struct locfs_inode *)(bh->b_data + LOCFS_INODE_BYTE_OFFSET(sb, inode_no));
    inode_buf = kmem_cache_alloc(locfs_inode_cache, GFP_KERNEL);
    memcpy(inode_buf, inode, sizeof(*inode_buf));

    brelse(bh);
    return inode_buf;
}

void locfs_save_locfs_inode(struct super_block *sb,
                                struct locfs_inode *inode_buf) {
    struct buffer_head *bh;
    struct locfs_inode *inode;
    uint64_t inode_no;

    inode_no = inode_buf->inode_no;
    bh = sb_bread(sb, LOCFS_INODE_TABLE_START_BLOCK_NO + LOCFS_INODE_BLOCK_OFFSET(sb, inode_no));
    BUG_ON(!bh);

    inode = (struct locfs_inode *)(bh->b_data + LOCFS_INODE_BYTE_OFFSET(sb, inode_no));
    memcpy(inode, inode_buf, sizeof(*inode));

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
}
