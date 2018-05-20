/*
 * Location Based Filesystem
 *
 * By, Robert Chrystie
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "internal.h"

/* Cache used to store the inodes in memory. */
struct kmem_cache *locfs_inode_cache = NULL;

static struct file_system_type locfs_type = {
    /* Defined in <linux/export.h> */
    .owner      = THIS_MODULE,
    
    /* Name of this file system as presented in /proc/filesystems and 
       name the user uses to mount this file system */
    .name       = "locfs",          

    /* Function called when the file system is mounted in userspace */
    .mount      = locfs_mount,      
                                    
    /* Function called when the file system is unmounted, use generic 
       kill_super_block defined in fs/super.c */
    .kill_sb    = kill_block_super,                                         

    /* Flag stating locfs requires a device */
    .fs_flags = FS_REQUIRES_DEV,    
};

static int __init locfs_init(void)
{
    int err;

    // Create SLAB
    locfs_inode_cache = kmem_cache_create("locfs_inode_cache",
                                           sizeof(struct locfs_inode),
                                           0,
                                           (SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD),
                                           NULL);

    if (locfs_inode_cache == NULL) {
        printk(KERN_ERR "Error creating locfs_inode_cache\n");
        return -ENOMEM;
    }

    err = register_filesystem(&locfs_type);

    if (likely(err == 0)) {
        printk(KERN_INFO "Sucessfully registered locfs\n");
    } else {
        printk(KERN_ERR "Failed to register locfs. Error:[%d]\n", err);
    }

    err = create_locationmod_proc();
    if (likely(err == 0)) {
        printk(KERN_INFO "Sucessfully created locationmod proc file\n");
    } else {
        printk(KERN_ERR "Failed to create locationmod proc file\n");
    }

    if (unlikely(err != 0)) {        
        // Cleanup SLAB on error
        kmem_cache_destroy(locfs_inode_cache);
    }
    
    return err;
}

static void __exit locfs_exit(void)
{
    int err;

    err = unregister_filesystem(&locfs_type);
    kmem_cache_destroy(locfs_inode_cache);

    if (likely(err == 0)) {
        printk(KERN_INFO "Sucessfully unregistered locfs\n");
    } else {
        printk(KERN_ERR "Failed to unregister locfs. Error:[%d]\n", err);
    }

    remove_locationmod_proc();
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert Chrystie");

module_init(locfs_init);
module_exit(locfs_exit);
