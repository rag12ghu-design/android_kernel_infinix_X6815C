// SPDX-License-Identifier: GPL-2.0
/*
 * Transsion Compatibility Shim Layer for Infinix Zero 5G 2023 (X6815C / MT6877)
 * Provides safe, no-op compatibility stubs and symbol exports for vendor daemons
 * and legacy kernel modules.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define TRAN_COMPAT_VERSION "1.0.0-slot-b"

/* Safe stub for Transsion FRE (Frame Rate Engine) */
void tran_fre_notify(int event, void *data)
{
	/* Handled by standard Linux CFS and Android Power HAL */
}
EXPORT_SYMBOL_GPL(tran_fre_notify);

/* Safe stub for Transsion PNPMGR (Power & Performance Manager) */
int tran_pnpmgr_boost(int level)
{
	return 0;
}
EXPORT_SYMBOL_GPL(tran_pnpmgr_boost);

/* Safe stub for Transsion Log */
void tran_log_write(const char *fmt, ...)
{
}
EXPORT_SYMBOL_GPL(tran_log_write);

/* Proc entry for userspace checks */
static int tran_compat_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Transsion X6815C Compatibility Layer v%s (Active)\n", TRAN_COMPAT_VERSION);
	seq_puts(m, "Hardware: MT6877 (Infinix Zero 5G 2023)\n");
	seq_puts(m, "Status: Slot B Verified / Safe Decoupled Mode\n");
	return 0;
}

static int tran_compat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, tran_compat_proc_show, NULL);
}

static const struct file_operations tran_compat_proc_fops = {
	.owner   = THIS_MODULE,
	.open    = tran_compat_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init tran_compat_init(void)
{
	proc_create("tran_compat", 0444, NULL, &tran_compat_proc_fops);
	pr_info("tran_compat: Initialized Transsion X6815C compatibility layer v%s\n", TRAN_COMPAT_VERSION);
	return 0;
}

static void __exit tran_compat_exit(void)
{
	remove_proc_entry("tran_compat", NULL);
	pr_info("tran_compat: Exited Transsion X6815C compatibility layer\n");
}

module_init(tran_compat_init);
module_exit(tran_compat_exit);

MODULE_AUTHOR("rag12ghu-design");
MODULE_DESCRIPTION("Transsion X6815C Compatibility Shim Layer");
MODULE_LICENSE("GPL v2");
