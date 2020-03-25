/*
 *  linux/fs/super.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * super.c contains code to handle the super-block tables.
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include <errno.h>
#include <sys/stat.h>

int sync_dev(int dev);
void wait_for_keypress(void);

/* set_bit uses setb, as gas doesn't recognize setc */
#define set_bit(bitnr,addr) ({ \
register int __res __asm__("ax"); \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res; })

struct super_block super_block[NR_SUPER];
/* this is initialized in init/main.c */
int ROOT_DEV = 0;

static void lock_super(struct super_block * sb)//锁定超级块，变成不可中断
{
	cli();
	while (sb->s_lock)
		sleep_on(&(sb->s_wait));//如果该超级块已经上锁，那就休眠
	sb->s_lock = 1;
	sti();
}

static void free_super(struct super_block * sb)//释放超级块数据结构的锁
{
	cli();
	sb->s_lock = 0;
	wake_up(&(sb->s_wait));//唤醒等待在改超级块的进程
	sti();
}

static void wait_on_super(struct super_block * sb)//等待超级块锁的释放，和lock_super有一些区别
{
	cli();
	while (sb->s_lock)
		sleep_on(&(sb->s_wait));
	sti();
}

struct super_block * get_super(int dev)//获得dev对应的super_block的结构体
{
	struct super_block * s;

	if (!dev)
		return NULL;
	s = 0+super_block;
	while (s < NR_SUPER+super_block)
		if (s->s_dev == dev) {
			wait_on_super(s);
			if (s->s_dev == dev)
				return s;
			s = 0+super_block;
		} else
			s++;
	return NULL;
}

void put_super(int dev)//释放dev对应的super_block的结构体
{
	struct super_block * sb;
	struct m_inode * inode;
	int i;

	if (dev == ROOT_DEV) {
		printk("root diskette changed: prepare for armageddon\n\r");
		return;
	}
	if (!(sb = get_super(dev)))//获得dev对应的super_block的结构体
		return;
	if (sb->s_imount) {//如果已经挂在上了，无法直接释放super_block的结构体
		printk("Mounted disk changed - tssk, tssk\n\r");
		return;
	}
	lock_super(sb);
	sb->s_dev = 0;
	for(i=0;i<I_MAP_SLOTS;i++)
		brelse(sb->s_imap[i]);
	for(i=0;i<Z_MAP_SLOTS;i++)
		brelse(sb->s_zmap[i]);
	free_super(sb);
	return;
}

static struct super_block * read_super(int dev)//从设备上读取超级块到缓冲区
{
	struct super_block * s;
	struct buffer_head * bh;
	int i,block;

	if (!dev)
		return NULL;
	check_disk_change(dev);//是否更换过磁盘
	if (s = get_super(dev))//如果超级块的结构体已经从磁盘读出来了，就直接返回。
		return s;
	for (s = 0+super_block ;; s++) {//先找个一个空闲的结构体
		if (s >= NR_SUPER+super_block)//如果数据已经满了，就返回null
			return NULL;
		if (!s->s_dev)
			break;
	}
	s->s_dev = dev;//初始化
	s->s_isup = NULL;
	s->s_imount = NULL;
	s->s_time = 0;
	s->s_rd_only = 0;
	s->s_dirt = 0;
	lock_super(s);//锁定改对象
	if (!(bh = bread(dev,1))) {//读取该设备的第一个块数据
		s->s_dev=0;
		free_super(s);
		return NULL;
	}
	*((struct d_super_block *) s) =
		*((struct d_super_block *) bh->b_data);//讲读取的数据直接赋值给d_super_block，这是硬盘中的数据
	brelse(bh);//释放bh
	if (s->s_magic != SUPER_MAGIC) {//校验魔数
		s->s_dev = 0;
		free_super(s);
		return NULL;
	}
	for (i=0;i<I_MAP_SLOTS;i++)
		s->s_imap[i] = NULL;
	for (i=0;i<Z_MAP_SLOTS;i++)
		s->s_zmap[i] = NULL;
	block=2;
	for (i=0 ; i < s->s_imap_blocks ; i++)//初始化inode位图
		if (s->s_imap[i]=bread(dev,block))
			block++;
		else
			break;
	for (i=0 ; i < s->s_zmap_blocks ; i++)//初始化逻辑块位图
		if (s->s_zmap[i]=bread(dev,block))
			block++;
		else
			break;
	if (block != 2+s->s_imap_blocks+s->s_zmap_blocks) {
		for(i=0;i<I_MAP_SLOTS;i++)
			brelse(s->s_imap[i]);
		for(i=0;i<Z_MAP_SLOTS;i++)
			brelse(s->s_zmap[i]);
		s->s_dev=0;
		free_super(s);
		return NULL;
	}
	s->s_imap[0]->b_data[0] |= 1;//inode位图的0号节点是不能用
	s->s_zmap[0]->b_data[0] |= 1;//逻辑块位图的0号节点是不能用
	free_super(s);
	return s;
}

int sys_umount(char * dev_name)//卸载文件系统
{
	struct m_inode * inode;
	struct super_block * sb;
	int dev;

	if (!(inode=namei(dev_name)))
		return -ENOENT;
	dev = inode->i_zone[0];//获取设备号
	if (!S_ISBLK(inode->i_mode)) {
		iput(inode);
		return -ENOTBLK;
	}
	iput(inode);
	if (dev==ROOT_DEV)
		return -EBUSY;
	if (!(sb=get_super(dev)) || !(sb->s_imount))
		return -ENOENT;
	if (!sb->s_imount->i_mount)
		printk("Mounted inode has i_mount=0\n");
	for (inode=inode_table+0 ; inode<inode_table+NR_INODE ; inode++)//判断一下是否有进程在使用该inode，如果有就禁止
		if (inode->i_dev==dev && inode->i_count)
				return -EBUSY;
	sb->s_imount->i_mount=0;
	iput(sb->s_imount);
	sb->s_imount = NULL;
	iput(sb->s_isup);
	sb->s_isup = NULL;
	put_super(dev);//释放superblock结构体
	sync_dev(dev);//同步一些高速缓存的数据
	return 0;
}

int sys_mount(char * dev_name, char * dir_name, int rw_flag)//安装文件系统
{
	struct m_inode * dev_i, * dir_i;
	struct super_block * sb;
	int dev;

	if (!(dev_i=namei(dev_name)))//根据设备文件名找到对应的inode，也就是说这个inode在块驱动程序安装的已经完成了。
		return -ENOENT;
	dev = dev_i->i_zone[0];//对于块设备，设备号在inode的i_zone[0]
	if (!S_ISBLK(dev_i->i_mode)) {//判断是不是块设备
		iput(dev_i);
		return -EPERM;
	}
	iput(dev_i);//释放inode
	if (!(dir_i=namei(dir_name)))//根据对应的目录新建一个inode
		return -ENOENT;
	if (dir_i->i_count != 1 || dir_i->i_num == ROOT_INO) {
		iput(dir_i);
		return -EBUSY;
	}
	if (!S_ISDIR(dir_i->i_mode)) {//该inode不是一个路径
		iput(dir_i);
		return -EPERM;
	}
	if (!(sb=read_super(dev))) {//拿到super_block
		iput(dir_i);
		return -EBUSY;
	}
	if (sb->s_imount) {//super_block已经被挂
		iput(dir_i);
		return -EBUSY;
	}
	if (dir_i->i_mount) {//该dir_i已经挂在了
		iput(dir_i);
		return -EPERM;
	}
	sb->s_imount=dir_i;//复制挂载点，因为我们需要使用所以不能释放sb和inode
	dir_i->i_mount=1;
	dir_i->i_dirt=1;		/* NOTE! we don't iput(dir_i) */
	return 0;			/* we do that in umount */
}

void mount_root(void)//这个算是根目录root的加载方式
{
	int i,free;
	struct super_block * p;
	struct m_inode * mi;

	if (32 != sizeof (struct d_inode))
		panic("bad i-node size");
	for(i=0;i<NR_FILE;i++)//文件描述符表初始化
		file_table[i].f_count=0;
	if (MAJOR(ROOT_DEV) == 2) {//如果根文件是软盘的话，就提示插入软盘
		printk("Insert root floppy and press ENTER");
		wait_for_keypress();
	}
	for(p = &super_block[0] ; p < &super_block[NR_SUPER] ; p++) {
		p->s_dev = 0;
		p->s_lock = 0;
		p->s_wait = NULL;
	}
	if (!(p=read_super(ROOT_DEV)))//读取root的目录
		panic("Unable to mount root");
	if (!(mi=iget(ROOT_DEV,ROOT_INO)))//从块设备读取第一个root inode (dev, id)
		panic("Unable to read root i-node");
	mi->i_count += 3 ;	/* NOTE! it is logically used 4 times, not 1 */
	p->s_isup = p->s_imount = mi;
	current->pwd = mi;//作为当前进程的pwd
	current->root = mi;//作为当前进程的root
	free=0;
	i=p->s_nzones;
	while (-- i >= 0)//计算当前super_block空闲的逻辑块数
		if (!set_bit(i&8191,p->s_zmap[i>>13]->b_data))
			free++;
	printk("%d/%d free blocks\n\r",free,p->s_nzones);
	free=0;
	i=p->s_ninodes+1;
	while (-- i >= 0)//计算当前super_block空闲的inode数
		if (!set_bit(i&8191,p->s_imap[i>>13]->b_data))
			free++;
	printk("%d/%d free inodes\n\r",free,p->s_ninodes);
}
