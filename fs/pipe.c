/*
 *  linux/fs/pipe.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <signal.h>

#include <linux/sched.h>
#include <linux/mm.h>	/* for get_free_page */
#include <asm/segment.h>

int read_pipe(struct m_inode * inode, char * buf, int count)//读取pipe
{
	int chars, size, read = 0;

	while (count>0) {
		while (!(size=PIPE_SIZE(*inode))) {//如果发现没有内容
			wake_up(&inode->i_wait);//唤醒写端
			if (inode->i_count != 2) /* are there any writers? *///没有写端
				return read;
			sleep_on(&inode->i_wait);//没有内容就睡眠
		}
		chars = PAGE_SIZE-PIPE_TAIL(*inode);//判断尾部的数据
		if (chars > count)
			chars = count;
		if (chars > size)
			chars = size;
		count -= chars;
		read += chars;
		size = PIPE_TAIL(*inode);//头部开始读的指针
		PIPE_TAIL(*inode) += chars;
		PIPE_TAIL(*inode) &= (PAGE_SIZE-1);
		while (chars-->0)
			put_fs_byte(((char *)inode->i_size)[size++],buf++);
	}
	wake_up(&inode->i_wait);
	return read;
}
	
int write_pipe(struct m_inode * inode, char * buf, int count)//写pipe的实现
{
	int chars, size, written = 0;

	while (count>0) {
		while (!(size=(PAGE_SIZE-1)-PIPE_SIZE(*inode))) {//如果写满了，size为0
			wake_up(&inode->i_wait);//唤醒读端
			if (inode->i_count != 2) { /* no readers *///没有读者直接返回
				current->signal |= (1<<(SIGPIPE-1));
				return written?written:-1;
			}
			sleep_on(&inode->i_wait);//写端休眠
		}
		chars = PAGE_SIZE-PIPE_HEAD(*inode);//计算管道头部到缓冲区末端的空闲字节数 4098
		if (chars > count)
			chars = count;
		if (chars > size)
			chars = size;
		count -= chars;
		written += chars;
		size = PIPE_HEAD(*inode);//当前的头指正
		PIPE_HEAD(*inode) += chars;
		PIPE_HEAD(*inode) &= (PAGE_SIZE-1);
		while (chars-->0)
			((char *)inode->i_size)[size++]=get_fs_byte(buf++);//一个写字符到管道
	}
	wake_up(&inode->i_wait);//写完唤醒读端
	return written;
}

int sys_pipe(unsigned long * fildes)//系统调用生成一对pipe
{
	struct m_inode * inode;
	struct file * f[2];
	int fd[2];
	int i,j;

	j=0;
	for(i=0;j<2 && i<NR_FILE;i++)
		if (!file_table[i].f_count)//找到空闲的file
			(f[j++]=i+file_table)->f_count++;//将空闲的file的f_count+1,并保存这两个file结构体
	if (j==1)//只找到一个
		f[0]->f_count=0;//将第一file重置，清空
	if (j<2)
		return -1;//没找到一队，反正就是失败
	j=0;
	for(i=0;j<2 && i<NR_OPEN;i++)//将两个file的指针分别保存到current->filp[i]的空闲处
		if (!current->filp[i]) {
			current->filp[ fd[j]=i ] = f[j];
			j++;
		}
	if (j==1)
		current->filp[fd[0]]=NULL;
	if (j<2) {
		f[0]->f_count=f[1]->f_count=0;
		return -1;
	}//和上面逻辑类似
	if (!(inode=get_pipe_inode())) {//新建一个pipe的inode
		current->filp[fd[0]] =
			current->filp[fd[1]] = NULL;
		f[0]->f_count = f[1]->f_count = 0;
		return -1;
	}
	f[0]->f_inode = f[1]->f_inode = inode;
	f[0]->f_pos = f[1]->f_pos = 0;
	f[0]->f_mode = 1;		/* read */
	f[1]->f_mode = 2;		/* write */
	put_fs_long(fd[0],0+fildes);//将fd返回给用户空间
	put_fs_long(fd[1],1+fildes);//将fd返回给用户空间
	return 0;
}
