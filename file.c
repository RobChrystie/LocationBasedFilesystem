/*
 * Location Based Filesystem
 *
 * By, Robert Chrystie
 */

#include <linux/buffer_head.h>
#include "internal.h"

ssize_t locfs_read(struct file *filp, 
                     char __user *buf, 
                     size_t len,
                     loff_t *ppos) 
{
    struct super_block *sb;
    struct inode *inode;
    struct locfs_inode *locfs_inode;
    struct buffer_head *bh;
    char *buffer;
    int nbytes;

    // Get the inode from the dentry cache
    inode = filp->f_path.dentry->d_inode;
    sb = inode->i_sb;
    locfs_inode = LOCFS_INODE(inode);
    
    if (*ppos >= locfs_inode->file_size) {
        return 0;
    }

    // Read the inode
    bh = sb_bread(sb, locfs_inode->data_block_no);
    if (!bh) {
        printk(KERN_ERR "Failed to read data block %llu\n",
               locfs_inode->data_block_no);
        return 0;
    }

    // Put the inode data read into a buffer
    buffer = (char *)bh->b_data + *ppos;
    nbytes = min((size_t)(locfs_inode->file_size - *ppos), len);

    // Pass the buffer to user space
    if (copy_to_user(buf, buffer, nbytes)) {
        brelse(bh);
        printk(KERN_ERR
               "Error while copying file content to userspace buffer\n");
        return -EFAULT;
    }

    brelse(bh);

    // Move the file pointer for what was read
    *ppos += nbytes;
    return nbytes;
}

ssize_t locfs_write(struct file *filp, 
                      const char __user *buf, 
                      size_t len,
                      loff_t *ppos) 
{
    struct super_block *sb;
    struct inode *inode;
    struct locfs_inode *locfs_inode;
    struct buffer_head *bh;
    struct locfs_super_block *locfs_sb;
    char *buffer;

    // Get inode from dentry cache
    inode = filp->f_path.dentry->d_inode;
    sb = inode->i_sb;
    locfs_inode = LOCFS_INODE(inode);
    locfs_sb = LOCFS_SB(sb);

    bh = sb_bread(sb, locfs_inode->data_block_no);
    if (!bh) {
        printk(KERN_ERR "Failed to read data block %llu\n",
               locfs_inode->data_block_no);
        return 0;
    }

    buffer = (char *)bh->b_data + *ppos;
    if (copy_from_user(buffer, buf, len)) {
        brelse(bh);
        printk(KERN_ERR
               "Error while copying file content from userspace buffer "
               "to kernel space\n");
        return -EFAULT;
    }
    *ppos += len;

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    // Update the inode and save it back
    locfs_inode->file_size = max((size_t)(locfs_inode->file_size),
                                   (size_t)(*ppos));
    locfs_save_locfs_inode(sb, locfs_inode);

    return len;
}

/* file_operations */
const struct file_operations locfs_file_operations = {
    /* locfs_read is called when read() system call is made
       happens in fs/read_write.c (calls vfs_read) */
	.read	= locfs_read,

    /* locfs_write is called when write() system call is made 
       happens in fs/read_write.c (calls vfs_write) */
	.write	= locfs_write,
};
