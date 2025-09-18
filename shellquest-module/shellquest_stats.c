#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>

static struct proc_dir_entry *proc_entry;
static int shellquest_xp = 0, shellquest_level = 1, shellquest_quests = 0;

static ssize_t proc_read(struct file *file, char __user *usr_buf, size_t count, loff_t *pos) {
    char buffer[128];
    int len = snprintf(buffer, sizeof(buffer), "ShellQuest Stats: XP=%d, Level=%d, Quests=%d\n",
                       shellquest_xp, shellquest_level, shellquest_quests);
    if (*pos >= len) return 0;
    if (copy_to_user(usr_buf, buffer, len)) return -EFAULT;
    *pos += len;
    return len;
}

static ssize_t proc_write(struct file *file, const char __user *usr_buf, size_t count, loff_t *pos) {
    char buffer[128];
    if (count > sizeof(buffer) - 1) count = sizeof(buffer) - 1;
    if (copy_from_user(buffer, usr_buf, count)) return -EFAULT;
    buffer[count] = '\0';
    sscanf(buffer, "%d %d %d", &shellquest_xp, &shellquest_level, &shellquest_quests);
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init shellquest_init(void) {
    proc_entry = proc_create("shellquest_stats", 0666, NULL, &proc_fops);
    if (!proc_entry) return -ENOMEM;
    pr_info("ShellQuest module loaded\n");
    return 0;
}

static void __exit shellquest_exit(void) {
    proc_remove(proc_entry);
    pr_info("ShellQuest module unloaded\n");
}

module_init(shellquest_init);
module_exit(shellquest_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vijay");
MODULE_DESCRIPTION("ShellQuest stats via procfs");
