/*
 *  linux/fs/truncate.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <linux/sched.h>

#include <sys/stat.h>

static void free_ind(int dev,int block)//清除一级块
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;

	if (!block)
		return;
	if (bh=bread(dev,block)) {
		p = (unsigned short *) bh->b_data;
		for (i=0;i<512;i++,p++)
			if (*p)
				free_block(dev,*p);//真的删除数据块
		brelse(bh);
	}
	free_block(dev,block);
}

static void free_dind(int dev,int block)//清除二级块
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;

	if (!block)
		return;
	if (bh=bread(dev,block)) {
		p = (unsigned short *) bh->b_data;
		for (i=0;i<512;i++,p++)
			if (*p)
				free_ind(dev,*p);//清除二级块的一级块
		brelse(bh);
	}
	free_block(dev,block);
}

void truncate(struct m_inode * inode)//清空一个文件的数据，也就从头再来
{
	int i;

	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
		return;
	for (i=0;i<7;i++)
		if (inode->i_zone[i]) {
			free_block(inode->i_dev,inode->i_zone[i]);//直接清除
			inode->i_zone[i]=0;
		}
	free_ind(inode->i_dev,inode->i_zone[7]);//清除一级块
	free_dind(inode->i_dev,inode->i_zone[8]);//清除二级块
	inode->i_zone[7] = inode->i_zone[8] = 0;
	inode->i_size = 0;
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
}

