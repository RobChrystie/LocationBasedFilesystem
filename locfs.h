/*
 * Location Based Filesystem
 *
 * By, Robert Chrystie
 */

#ifndef _LOCFS_H
#define _LOCFS_H

/* The locfs inode structure */
struct locfs_inode {
    struct inode li_inode;
};

/* Helper function to cast inode to locfs_inode */
static inline struct locfs_inode *LOCFS_INODE(struct inode *inode)
{
	return (struct locfs_inode *)inode;
}

static const unsigned long LOCFS_MAGIC = 0x5050;

#endif
