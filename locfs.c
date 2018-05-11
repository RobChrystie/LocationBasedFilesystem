/*
 * Location Based Filesystem
 *
 * By, Robert Chrystie
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "locfs.h"

static struct kmem_cache *locfs_inode_cache;

struct inode *locfs_inode_alloc(struct super_block *sb)
{
	struct locfs_inode *inode = (struct locfs_inode *)
				kmem_cache_alloc(locfs_inode_cache, GFP_KERNEL);

	if (!inode) {
        printk(KERN_ERR "Could not allocate locfs_inode\n");
        return NULL;
    }

	return &inode->li_inode;
}

void locfs_inode_init_once(void *i)
{
	struct locfs_inode *inode = (struct locfs_inode *)i;

    // This will ensure initialization of the inode only occurs once.
	inode_init_once(&inode->li_inode);
}

static void locfs_free_inode_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);

    // Free this locfs_inode from the cache
	kmem_cache_free(locfs_inode_cache, LOCFS_INODE(inode));
    printk(KERN_INFO "Freed locfs inode %lu\n", (unsigned long)inode->i_ino);
}

void locfs_inode_free(struct inode *inode)
{
	call_rcu(&inode->i_rcu, locfs_free_inode_callback);
}

static void locfs_kill_sb(struct super_block *sb)
{
    kfree(sb->s_fs_info);
    kill_litter_super(sb);
    printk(KERN_INFO "Killed locfs super_block\n");
}

static struct super_operations const locfs_super_ops = {    
	.destroy_inode = locfs_inode_free,
};

int locfs_fill_super(struct super_block *sb, 
                           void *data, 
                           int silent)
{
    struct inode *root = NULL;
    sb->s_magic = LOCFS_MAGIC; // Magic number used to identify the filesystem
    sb->s_op = &locfs_super_ops; // Pointer to the functions for this super_block  
    
    root = new_inode(sb);
    if (!root) {
        printk(KERN_ERR "Error allocating root inode for locfs\n");
        return -ENOMEM;
    }

    root->i_ino = 0;
    root->i_sb = sb; // Sets this inodes super_block to our super_block
    root->i_atime = root->i_mtime = root->i_ctime = CURRENT_TIME; // Set all the time stamps to current time
//    root->i_op = &locfs_inode_ops; // Set the inode oeprations
    inode_init_owner(root, NULL, S_IFDIR); // This will set the i_uid, i_gid, and i_mode

    // Set our super_blocks root inode to the inode we just created
    sb->s_root = d_make_root(root);
    if (!sb->s_root) {
        printk(KERN_ERR "Error creating root inode for locfs\n");
        return -ENOMEM;
    }

    return 0;
}

static struct dentry *locfs_mount(struct file_system_type *fs_type,
                                  int flags, 
                                  const char *dev_name,
                                  void *data)
{
    struct dentry *ret = mount_bdev(fs_type, flags, dev_name, data, locfs_fill_super);

    if (IS_ERR(ret))
        printk(KERN_ERR "Error mounting locfs\n");
    else
        printk(KERN_INFO "Locfs mounted on [%s]\n", dev_name);

    return ret;
}

static struct file_system_type locfs_type = {
    .owner      = THIS_MODULE,      // Defined in <linux/export.h>
    .name       = "locfs",          // Name of this file system as presented in /proc/filesystems and name the user
                                    // uses to mount this file system.
    .mount      = locfs_mount,      // Function called when the file system is mounted in userspace.
    .kill_sb    = locfs_kill_sb,    // Function called when the file system is unmounted
};

static int __init locfs_init(void)
{
    int err;

    // Create SLAB
    locfs_inode_cache = kmem_cache_create("locfs_inode_cache",
                                           sizeof(struct locfs_inode),
                                           0,
                                           (SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD),
                                           locfs_inode_init_once);

    if (locfs_inode_cache == NULL) {
        printk(KERN_ERR "Error creating locfs_inode_cache\n");
        return -ENOMEM;
    }

    err = register_filesystem(&locfs_type);
    if (err) {
        kmem_cache_destroy(locfs_inode_cache);
        printk(KERN_ERR "Failed to register locfs. Error:[%d]\n", err);
        return err;
    }
    
    printk(KERN_INFO "Sucessfully registered locfs\n");
    return 0;
}

static void __exit locfs_exit(void)
{
    int err;

    err = unregister_filesystem(&locfs_type);
    kmem_cache_destroy(locfs_inode_cache);

    if (err == 0)
        printk(KERN_INFO "Sucessfully unregistered locfs\n");
    else
        printk(KERN_ERR "Failed to unregister locfs. Error:[%d]\n", err);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert Chrystie");

module_init(locfs_init);
module_exit(locfs_exit);
