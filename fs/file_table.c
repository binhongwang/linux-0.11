/*
 *  linux/fs/file_table.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <linux/fs.h>

struct file file_table[NR_FILE];//linux最多打开文件的数量是64，对于单个进程来说最多打开的是20
