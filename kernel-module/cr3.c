/**
 * this is a kernel module to expose the CR3 register and a way
 * to access physical memory. the latter part is a pretty big
 * security problem, so this module should only be used for
 * educational purposes. (it was written for CS 149 at San José
 * State University.)
 *
 * i used code from ALLAN CRUSE (notice below) to start out. i
 * adapted it to work in x86_64 and use the new proc interface
 * code. page_reader was also added.
 *
 * ben reed 2019-3-17
 */ 
//-------------------------------------------------------------------
//    cr3.c
//
//    This kernel module creates a pseudo-file named '/proc/cr3'
//    which allows an application program to discover the values 
//    that currently reside in the processor's control registers 
//    CR3 and CR4, and it explicitly interprets the state of the
//    PAE-bit (bit #5: Page Address Extensions), and the PSE-bit
//    (bit #4: Page Size Extensions), from control register CR4.
//    Together, these register-values determine the scheme being
//    used by the CPU to locate and interpret its 'page-mapping' 
//    tables for virtual-to-physical memory-address translation.
//
//    NOTE: Written and tested with Linux kernel version 2.6.22.
//
//    programmer: ALLAN CRUSE
//    written on: 26 AUG 2007
//-------------------------------------------------------------------

#include <linux/module.h>    // for init_module() 
#include <linux/proc_fs.h>    // for create_proc_info_entry() 
#include <linux/seq_file.h>     // for seq_file functions
#include <linux/io.h>           // for simple_read_from_buffer()

char modname[] = "cr3";
char rpagename[] = "page_reader";

static int proc_cr3_show(struct seq_file *f, void *v)
{
    u64 _cr3, _cr4;

    asm( " mov %%cr3, %%rax \n mov %%rax, %0 " : "=m" (_cr3) :: "%rax" );
    asm( " mov %%cr4, %%rax \n mov %%rax, %0 " : "=m" (_cr4) :: "%rax" );

    seq_printf(f, "CR3=%016llX\n", _cr3 );
    seq_printf(f, "CR4=%016llX\n", _cr4 );
    seq_printf(f, "PAE=%llX\n", (_cr4 >> 5)&1 );
    seq_printf(f, "PSE=%llX\n", (_cr4 >> 4)&1 );
    return 0;
}

static int myopen(struct inode *inode, struct file *file)
{
    return single_open(file, proc_cr3_show, NULL);
}

static ssize_t proc_page_reader(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    loff_t pos = *ppos;
    loff_t offset;
    void *page_data;
    ssize_t rc;

    /* page align */
    pos &= PAGE_MASK;
    page_data = memremap(pos, PAGE_SIZE, MEMREMAP_WB);
    if (page_data == NULL) {
        return -EINVAL;
    }
    offset = *ppos - pos;
    printk("pos = %llX offset = %lld count = %lu\n", *ppos, offset, count);
    rc =  simple_read_from_buffer(buf, count, &offset, page_data, PAGE_SIZE);
    if (rc > 0) {
        *ppos = pos + offset;
    }
    printk("pos = %llX offset = %lld rc = %ld\n", *ppos, offset, rc);
    memunmap(page_data);
    return rc;
}

static const struct proc_ops prfops = {
    .proc_read = proc_page_reader,
    .proc_lseek = default_llseek,
};

static const struct proc_ops fops = {
    .proc_open = myopen,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,
};

static int __init my_init( void )
{
    printk( KERN_INFO "Installing \'%s\' module for /proc/cr3 /proc/page_reader\n", modname );
    proc_create(modname, S_IRUGO, NULL, &fops);
    proc_create(rpagename, S_IRUSR, NULL, &prfops);
    return    0;
}

static void __exit my_exit(void )
{
    remove_proc_entry( modname, NULL );
    remove_proc_entry( rpagename, NULL );
    printk( KERN_INFO "Removing \'%s\' module\n", modname );
}

module_init( my_init );
module_exit( my_exit );
MODULE_LICENSE("GPL"); 

