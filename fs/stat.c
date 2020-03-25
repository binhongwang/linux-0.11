/*
 *  linux/fs/stat.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>
#include <sys/stat.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

static void cp_stat(struct m_inode * inode, struct stat * statbuf)//拷贝文件信息到一个statbuf
{
	struct stat tmp;
	int i;

	verify_area(statbuf,sizeof (* statbuf));
	tmp.st_dev = inode->i_dev;
	tmp.st_ino = inode->i_num;
	tmp.st_mode = inode->i_mode;
	tmp.st_nlink = inode->i_nlinks;
	tmp.st_uid = inode->i_uid;
	tmp.st_gid = inode->i_gid;
	tmp.st_rdev = inode->i_zone[0];
	tmp.st_size = inode->i_size;
	tmp.st_atime = inode->i_atime;
	tmp.st_mtime = inode->i_mtime;
	tmp.st_ctime = inode->i_ctime;
	for (i=0 ; i<sizeof (tmp) ; i++)
		put_fs_byte(((char *) &tmp)[i],&((char *) statbuf)[i]);
}

int sys_stat(char * filename, struct stat * statbuf)//根据name获得文件属性
{
	struct m_inode * inode;

	if (!(inode=namei(filename)))//根据name获得一个inode
		return -ENOENT;
	cp_stat(inode,statbuf);//把文件的信息复制到statbuf
	iput(inode);//释放inode
	return 0;
}

int sys_fstat(unsigned int fd, struct stat * statbuf)//根据fd获得文件属性
{
	struct file * f;
	struct m_inode * inode;

	if (fd >= NR_OPEN || !(f=current->filp[fd]) || !(inode=f->f_inode))//根据fd到房钱的file，然后拿到当前的inode
		return -EBADF;
	cp_stat(inode,statbuf);//把文件的信息复制到statbuf，因为文件是打开的，所以inode不需要删除
	return 0;
}
