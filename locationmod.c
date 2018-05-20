/*
 * Location Based Filesystem
 *
 * By, Robert Chrystie
 */

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "include/locfs.h"
#include "internal.h"

char *curr_location = "Home";
EXPORT_SYMBOL(curr_location);

static int locationmod_seq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s", curr_location);
	return 0;
}

static ssize_t locationmod_write(struct file* file, const char __user *buffer, size_t count, loff_t *f_pos)
{
	char *tmp = kzalloc((count+1),GFP_KERNEL);

	if(!tmp) {
        return -ENOMEM;
    }

	if(copy_from_user(tmp, buffer, count)){
		kfree(tmp);
		return EFAULT;
	}

    curr_location = tmp;

    printk(KERN_INFO "locationmod: Location set to %s", curr_location);

	return count;
}

static int locationmod_open(struct inode *inode,
                              struct file *file)
{
	return single_open(file, locationmod_seq_show, NULL);
}

static struct file_operations locationmod_fops={
	.owner = THIS_MODULE,
	.open = locationmod_open,
	.write = locationmod_write,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

int create_locationmod_proc(void) 
{
	struct proc_dir_entry *entry;
	entry = proc_create("locationmod", 0777, NULL, &locationmod_fops);

	if(!entry) {
		return -1;	
    }

	return 0;
}

void remove_locationmod_proc(void) 
{
	remove_proc_entry("locationmod", NULL);
	printk(KERN_INFO "locationmod: Removed proc file\n");
}
