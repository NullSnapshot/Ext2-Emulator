/*************** type.h file for LEVEL-1 ****************/
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

typedef struct ext2_super_block SUPER;
typedef struct ext2_group_desc  GD;
typedef struct ext2_inode       INODE;
typedef struct ext2_dir_entry_2 DIR;

SUPER *sp;
GD    *gp;
INODE *ip;
DIR   *dp;   

#define FREE        0
#define READY       1

#define BLKSIZE  1024
#define NMINODE   128
#define NPROC       2

#define NFD         4

typedef struct minode{
  INODE INODE;           // INODE structure on disk
  int dev, ino;          // (dev, ino) of INODE
  int refCount;          // in use count
  int dirty;             // 0 for clean, 1 for modified

  int mounted;           // for level-3
  struct mntable *mptr;  // for level-3
}MINODE;

typedef struct oft{ // Open file table
  int mode; // R|W|RW|APP
  int refCount;
  MINODE *minodePtr;
  int offset;
}OFT;


typedef struct proc{
  struct proc *next;
  int          pid;      // process ID  
  int          ppid;
  int          status;
  int          uid;      // user ID
  int          gid;
  MINODE      *cwd;      // CWD directory pointer  

  OFT *fd[NFD]; // opened ofts of this process
}PROC;

typedef struct Mount{
  int     dev; // dev(opened vdisk fd number) 0 means FREE
  int ninodes;  //from superblock
  int nblocks;
  int bmap;     // from GD block
  int imap;
  int iblk;
  MINODE *mounted_inode;
  char  name[64]; // device name, e.g. mydisk
  char mount_name[64]; // mounted DIR pathname
}MOUNT;