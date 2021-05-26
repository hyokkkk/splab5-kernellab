#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
#define MAXLEN 1000

static struct dentry *dir, *inputfile, *ptreefile;
static struct task_struct *curr;
ssize_t total_len= 0;
char output[MAXLEN];

// todo 1 : change to kmalloc
// todo 2 : use whole name instead of comm

// called when cmd gets `echo 1234 >> input`.
static ssize_t write_pid_to_input(struct file *fp, 
                            const char __user *user_buffer, 
                            size_t length, 
                            loff_t *position)
{
  pid_t input_pid;

  memset(output, 0, MAXLEN);
  char temp_output[MAXLEN];
  ssize_t temp_len = 0;

  // Get input
  sscanf(user_buffer, "%u", &input_pid);

  // Find task_struct using input_pid. Hint: pid_task
  if (!(curr = pid_task(find_vpid(input_pid), PIDTYPE_PID))){ 
    printk("Invalid pid\n");
    return -1; 
  }


  // Tracing process tree from input_pid to init(1) process
  while (curr->pid !=0){
    // init to 0
    memset(temp_output, 0, MAXLEN);
    // comm : 15chars
    temp_len= snprintf(temp_output, MAXLEN, "%s (%d)\n", curr->comm, curr->pid);
    //append the last result 
    snprintf(temp_output+temp_len, MAXLEN-temp_len, output);
    //add to the total length
    if(temp_len>= 0) { total_len+= temp_len; }
    //update output string
    strcpy(output, temp_output);
    //go to parent
    curr = curr -> parent;

  }

  // Make Output Format string: process_command (process_id)

  return length;
}

static ssize_t read_output_from_ptreefile(struct file *fp, 
                                          char __user *user_buffer, 
                                          size_t length, 
                                          loff_t *position)
{
  // read the output from kernel buffer to user buffer
  return simple_read_from_buffer(user_buffer, length, position, output, total_len);
}

static const struct file_operations dbfs_fops_write = {
  .write = write_pid_to_input,
};

static const struct file_operations dbfs_fops_read = {
  .read = read_output_from_ptreefile,
};

static int __init dbfs_module_init(void)
{
  if (!(dir = debugfs_create_dir("ptree", NULL))) {
    printk("Cannot create ptree dir\n");
    return -1;
  }
  // 00700: allow read, write, execute mode to owner.
  if (!(inputfile = debugfs_create_file("input", 00700, dir, NULL, &dbfs_fops_write))){
    printk("Cannot create inputfile\n");
    return -1;
  }
  if (!(ptreefile = debugfs_create_file("ptree", 00700, dir, NULL, &dbfs_fops_read))){
    printk("Cannot create ptreefile\n");
    return -1;
  }
  
  printk("dbfs_ptree module initialize done\n");

  return 0;
}

static void __exit dbfs_module_exit(void)
{
  debugfs_remove_recursive(dir);
  printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
