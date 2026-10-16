// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>

static int cmdline_proc_show(struct seq_file *m, void *v)
{
	char *buf, *p;

	/*
	 * Android first_stage_init (PID 1) must see the authentic unlocked bootloader
	 * state during early boot, otherwise AVB (Android Verified Boot) fails verification
	 * against custom/ported partitions and reboots to bootloader/fastboot.
	 * We cloak only for userspace apps, Play Integrity, and GMS (PID != 1).
	 */
	if (current->pid <= 1) {
		seq_puts(m, saved_command_line);
		seq_putc(m, '\n');
		return 0;
	}

	buf = kstrdup(saved_command_line, GFP_KERNEL);
	if (!buf) {
		seq_puts(m, saved_command_line);
		seq_putc(m, '\n');
		return 0;
	}

	/* Cloak verified boot state for Play Integrity / CTS: orange -> green */
	p = strstr(buf, "androidboot.verifiedbootstate=orange");
	if (p) {
		memcpy(p + 30, "green", 5);
		memmove(p + 35, p + 36, strlen(p + 36) + 1);
	}

	/* Cloak flash locked flag: 0 -> 1 */
	p = strstr(buf, "androidboot.flash.locked=0");
	if (p)
		p[25] = '1';

	/* Cloak vbmeta device state: unlocked -> locked */
	p = strstr(buf, "androidboot.vbmeta.device_state=unlocked");
	if (p) {
		memcpy(p + 32, "locked", 6);
		memmove(p + 38, p + 40, strlen(p + 40) + 1);
	}

	seq_puts(m, buf);
	seq_putc(m, '\n');
	kfree(buf);
	return 0;
}

static int __init proc_cmdline_init(void)
{
	proc_create_single("cmdline", 0, NULL, cmdline_proc_show);
	return 0;
}
fs_initcall(proc_cmdline_init);
