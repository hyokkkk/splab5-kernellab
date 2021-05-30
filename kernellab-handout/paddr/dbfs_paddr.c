#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

// <Informations are stored in little endian.>
// struct packet in app.c size  : 24byte(pid 8, vaddr 8, paddr 8)
struct packet {
  pid_t pid;            // 4byte + padding 4byte
  unsigned long vaddr;  // 8byte
  unsigned long paddr;  // 8byte
};

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
  struct packet pckt;
  unsigned char* kernel_buf = kzalloc(length, GFP_KERNEL);

  /* page table data */
  struct mm_struct *mm;
  pgd_t *pgdp;      // lv1 Page Table
  p4d_t *p4dp;      // lv2
  pud_t *pudp;      // lv3
  pmd_t *pmdp;      // lv4
  pte_t *ptep;      // lv5

  if (copy_from_user(kernel_buf, user_buffer, length)) {return -1;}

  // get pckt.pid and vaddr
  pckt.pid |= kernel_buf[3]; pckt.pid <<= 8;
  pckt.pid |= kernel_buf[2]; pckt.pid <<= 8;
  pckt.pid |= kernel_buf[1]; pckt.pid <<= 8;
  pckt.pid |= kernel_buf[0];

  pckt.vaddr |= kernel_buf[13]; pckt.vaddr <<= 8;
  pckt.vaddr |= kernel_buf[12]; pckt.vaddr <<= 8;
  pckt.vaddr |= kernel_buf[11]; pckt.vaddr <<= 8;
  pckt.vaddr |= kernel_buf[10]; pckt.vaddr <<= 8;
  pckt.vaddr |= kernel_buf[9];  pckt.vaddr <<= 8;
  pckt.vaddr |= kernel_buf[8];

  // get page table info
  if (!(task = pid_task(find_get_pid(pckt.pid), PIDTYPE_PID))) {
    printk("Cannot get task\n");
    return -1;
  }

  // get Physical address
  mm = task -> mm;
  pgdp = pgd_offset(mm, pckt.vaddr);
  p4dp = p4d_offset(pgdp, pckt.vaddr);
  pudp = pud_offset(p4dp, pckt.vaddr);
  pmdp = pmd_offset(pudp, pckt.vaddr);
  ptep = pte_offset_kernel(pmdp, pckt.vaddr);
  pckt.paddr = (pte_pfn(*ptep)<<12) | (pckt.vaddr&0xfff);

  // send info to user buffer
  kfree(kernel_buf);
  return simple_read_from_buffer(user_buffer, length, position, &pckt, length);
}


static const struct file_operations dbfs_fops = {
  .read = read_output,
};


static int __init dbfs_module_init(void)
{
  if (!(dir = debugfs_create_dir("paddr", NULL))) {
    printk("Cannot create paddr dir\n");
    return -1;
  }
  if (!(output = debugfs_create_file("output", 00700, dir, NULL, &dbfs_fops))){
    printk("Cannot create output file\n");
    return -1;
  }
	printk("dbfs_paddr module initialize done\n");
  return 0;
}


static void __exit dbfs_module_exit(void)
{
  debugfs_remove_recursive(dir);
	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
