/*
 * This file has definitions for some important file table
 * structures etc.
 */

#ifndef _FS_H
#define _FS_H

#include <sys/types.h>

/* devices are as follows: (same as minix, so we can use the minix
 * file system. These are major numbers.)
 *
 * 0 - unused (nodev)
 * 1 - /dev/mem
 * 2 - /dev/fd
 * 3 - /dev/hd
 * 4 - /dev/ttyx
 * 5 - /dev/tty
 * 6 - /dev/lp
 * 7 - unnamed pipes
 */

#define IS_SEEKABLE(x) ((x)>=1 && (x)<=3)

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a))>>8)
#define MINOR(a) ((a)&0xff)

#define NAME_LEN 14
#define ROOT_INO 1

#define I_MAP_SLOTS 8
#define Z_MAP_SLOTS 8
#define SUPER_MAGIC 0x137F

#define NR_OPEN 20 //单个进程最大的fd的数量
#define NR_INODE 32 //最多可以实例化的inode
#define NR_FILE 64 //系统能打开的file
#define NR_SUPER 8
#define NR_HASH 307
#define NR_BUFFERS nr_buffers //buffer没有上限
#define BLOCK_SIZE 1024 //每一个块的大小
#define BLOCK_SIZE_BITS 10 //移动10位，就是增加1024的地址
#ifndef NULL
#define NULL ((void *) 0)
#endif

#define INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct d_inode)))//每一个块存放d_inode的数量
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct dir_entry)))//每一个块dir_entry存放的数量

#define PIPE_HEAD(inode) ((inode).i_zone[0])
#define PIPE_TAIL(inode) ((inode).i_zone[1])
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)-PIPE_TAIL(inode))&(PAGE_SIZE-1))
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode)==PIPE_TAIL(inode))
#define PIPE_FULL(inode) (PIPE_SIZE(inode)==(PAGE_SIZE-1))
#define INC_PIPE(head) \
__asm__("incl %0\n\tandl $4095,%0"::"m" (head))

typedef char buffer_block[BLOCK_SIZE];//没地方用到

struct buffer_head {//一块buffer_head最多指向了1024bytes = 8192bits，一般存放的就是对应的数据块
	char * b_data;			/* pointer to data block (1024 bytes) */
	unsigned long b_blocknr;	/* block number */ //块号
	unsigned short b_dev;		/* device (0 = free) */ //设备号
	unsigned char b_uptodate;
	unsigned char b_dirt;		/* 0-clean,1-dirty */
	unsigned char b_count;		/* users using this block */
	unsigned char b_lock;		/* 0 - ok, 1 -locked */
	struct task_struct * b_wait;
	struct buffer_head * b_prev;
	struct buffer_head * b_next;
	struct buffer_head * b_prev_free;
	struct buffer_head * b_next_free;
};

struct d_inode {  //块设备中对应的inode的结构体
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_time;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
};

struct m_inode {
	unsigned short i_mode;//15-12文件类型，11-9保存执行文件设置，8-0保存文件权限
	unsigned short i_uid;//文件宿主的用户id
	unsigned long i_size;//文件长度
	unsigned long i_mtime;//修改时间
	unsigned char i_gid;//文件宿主的组id
	unsigned char i_nlinks;//硬链接的次数
	unsigned short i_zone[9];//对应数据块 0-6直接是块号，7一次间接块，8二次间接块，一个块是1KB=1024Byte
	//因为块号用short来表示，也就是2Byte，所以一个块可以存放512个块号，所以一次块512个，二次块就是512*512。
	//所以变相的可以算出一个文件的最大size是7+512+512*512 kb
	//一般逻辑块的大小会和buffer_head大小一样。
/* these are in memory also */
	struct task_struct * i_wait;
	unsigned long i_atime;
	unsigned long i_ctime;
	unsigned short i_dev;
	unsigned short i_num;
	unsigned short i_count;
	unsigned char i_lock;
	unsigned char i_dirt;
	unsigned char i_pipe;
	unsigned char i_mount;
	unsigned char i_seek;
	unsigned char i_update;
};

struct file {
	unsigned short f_mode;
	unsigned short f_flags;
	unsigned short f_count;//对应的文件句柄数字
	struct m_inode * f_inode;
	off_t f_pos;//文件当前的读写指针，读到哪里了。
};

struct super_block {
	unsigned short s_ninodes;//inode数
	unsigned short s_nzones;//逻辑块数
	unsigned short s_imap_blocks;//inode位图所占块数
	unsigned short s_zmap_blocks;//逻辑块位图所占块数
	unsigned short s_firstdatazone;//第一个逻辑块号
	unsigned short s_log_zone_size;
	unsigned long s_max_size;//最大文件长度
	unsigned short s_magic;//魔数
/* These are only in memory */
	struct buffer_head * s_imap[8];//inode位图在bh指针数组，因为一个buffer_head最多指向1024byte=8192，所以最多表示8192块
	struct buffer_head * s_zmap[8];//逻辑块位图，和上面类似，但是两者的第一位都不适用。
	unsigned short s_dev;//超级块所在设备号
	struct m_inode * s_isup;//被安装文件系统根目录的inode
	struct m_inode * s_imount;//改文件系统被安装到的inode，挂载点
	unsigned long s_time;//修改时间
	struct task_struct * s_wait;//等待super_block的进程
	unsigned char s_lock;//锁定标识
	unsigned char s_rd_only;//只读标识
	unsigned char s_dirt;//已经修改标识
};

struct d_super_block { //块设备中对应的super_block的结构体
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
};

struct dir_entry { //目录
	unsigned short inode;//inode
	char name[NAME_LEN];//inode对应的文件名
};

extern struct m_inode inode_table[NR_INODE];
extern struct file file_table[NR_FILE];
extern struct super_block super_block[NR_SUPER];
extern struct buffer_head * start_buffer;
extern int nr_buffers;

extern void check_disk_change(int dev);
extern int floppy_change(unsigned int nr);
extern int ticks_to_floppy_on(unsigned int dev);
extern void floppy_on(unsigned int dev);
extern void floppy_off(unsigned int dev);
extern void truncate(struct m_inode * inode);
extern void sync_inodes(void);
extern void wait_on(struct m_inode * inode);
extern int bmap(struct m_inode * inode,int block);
extern int create_block(struct m_inode * inode,int block);
extern struct m_inode * namei(const char * pathname);
extern int open_namei(const char * pathname, int flag, int mode,
	struct m_inode ** res_inode);
extern void iput(struct m_inode * inode);
extern struct m_inode * iget(int dev,int nr);
extern struct m_inode * get_empty_inode(void);
extern struct m_inode * get_pipe_inode(void);
extern struct buffer_head * get_hash_table(int dev, int block);
extern struct buffer_head * getblk(int dev, int block);
extern void ll_rw_block(int rw, struct buffer_head * bh);
extern void brelse(struct buffer_head * buf);
extern struct buffer_head * bread(int dev,int block);
extern void bread_page(unsigned long addr,int dev,int b[4]);
extern struct buffer_head * breada(int dev,int block,...);
extern int new_block(int dev);
extern void free_block(int dev, int block);
extern struct m_inode * new_inode(int dev);
extern void free_inode(struct m_inode * inode);
extern int sync_dev(int dev);
extern struct super_block * get_super(int dev);
extern int ROOT_DEV;

extern void mount_root(void);

#endif
