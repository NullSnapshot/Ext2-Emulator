/****************************************************************************
*                   KCW: mount root file system                             *
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <time.h>
#include "type.h"

extern MINODE *iget();

MINODE minode[NMINODE];
MINODE *root;
PROC   proc[NPROC], *running;
OFT oft[4];
MOUNT mountTable[8]; // set all dev = 0

char gpath[128]; // global for tokenized components
char *name[64];  // assume at most 64 components in pathname
int   n;         // number of component strings

int fd, dev;
int nblocks, ninodes, bmap, imap, iblk;
char line[128], cmd[32], pathname[128], arg2[128];
char *disk;

// Level 1 files
#include "cd_ls_pwd.c"
#include "mkdir_creat.c"
#include "rmdir.c"
#include "link_unlink.c"
#include "symlink.c"

// Level 2 files
#include "open_close.c"
#include "read_cat.c"
#include "write_cp.c"

// Level 3 files
#include "mount_umount.c"


int init()
{
  int i, j;
  MINODE *mip;
  PROC   *p;
  OFT *o;
  MOUNT *m;

  printf("init()\n");

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    mip->dev = mip->ino = 0;
    mip->refCount = 0;
    mip->mounted = 0;
    mip->mptr = 0;
  }
  for (i=0; i<NPROC; i++){
    p = &proc[i];
    p->pid = i;
    p->uid = p->gid = i;
    p->cwd = 0;
    for(int j=0; j<NFD; j++)
    {
      p->fd[j] = NULL;
    }
  }
  for(i=0; i<NFD; i++)
  {
    o = &oft[i];
    o->refCount = 0; // resource is free.
  }
  for(i=0; i<8; i++)
  {
    m = &mountTable[i];
    m->dev = 0;
  }
}

// load root INODE and set root pointer to it
int mount_root()
{  
  printf("mount_root()\n");
  mountTable[0].dev = dev;
  mountTable[0].ninodes = ninodes;
  mountTable[0].nblocks = nblocks;
  mountTable[0].bmap = bmap;
  mountTable[0].imap = imap;
  mountTable[0].iblk = iblk;
  strncpy(mountTable[0].name, disk, 64);
  root = iget(dev, 2);
  mountTable[0].mounted_inode = root;
  strcpy(mountTable[0].mount_name, "/");
}

int main(int argc, char *argv[ ])
{
  int ino;
  char buf[BLKSIZE];
  disk = argv[1];

  printf("checking EXT2 FS ....");
  if ((fd = open(argv[1], O_RDWR)) < 0){
    printf("open %s failed\n", argv[1]);
    exit(1);
  }

  dev = fd;    // global dev same as this fd   

  /********** read super block  ****************/
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;

  /* verify it's an ext2 file system ***********/
  if (sp->s_magic != 0xEF53){
      printf("magic = %x is not an ext2 filesystem\n", sp->s_magic);
      exit(1);
  }     
  printf("EXT2 FS OK\n");
  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;

  get_block(dev, 2, buf); 
  gp = (GD *)buf;

  bmap = gp->bg_block_bitmap;
  imap = gp->bg_inode_bitmap;
  iblk = gp->bg_inode_table;
  printf("bmp=%d imap=%d inode_start = %d\n", bmap, imap, iblk);

  init();  
  mount_root();
  printf("root refCount = %d\n", root->refCount);

  printf("creating P0 as running process\n");
  running = &proc[0];
  running->status = READY;
  running->cwd = iget(dev, 2);
  printf("root refCount = %d\n", root->refCount);

  // WRTIE code here to create P1 as a USER process
  printf("creating P1 as running process\n");
  proc[1].status = READY;
  proc[1].cwd = iget(dev, 2);
  printf("root refcount = %d\n", root->refCount);
  
  while(1){
    printf("input command : ");
    fgets(line, 128, stdin);
    line[strlen(line)-1] = 0;

    if (line[0]==0)
       continue;
    pathname[0] = 0;
    arg2[0] = 0;

    sscanf(line, "%s %s %s", cmd, pathname, arg2);
    //printf("cmd=%s pathname=%s arg2=%s\n", cmd, pathname, arg2);
  
    if (strcmp(cmd, "ls")==0)
       ls();
    else if (strcmp(cmd, "cd")==0)
       cd();
    else if (strcmp(cmd, "pwd")==0)
       pwd(running->cwd);
    else if (strcmp(cmd, "mkdir")==0)
        ex2mkdir();
    else if (strcmp(cmd, "creat")==0)
        ex2creat(pathname);
    else if (strcmp(cmd, "rmdir")==0)
        ex2rmdir();
    else if (strcmp(cmd, "link")==0)
        ex2link();
    else if (strcmp(cmd, "unlink")==0)
        ex2unlink();
    else if (strcmp(cmd, "symlink")==0)
        ex2symlink();
    else if (strcmp(cmd, "readlink")==0)
        ex2readlink();
    else if (strcmp(cmd, "open")== 0)
        ex2opencommand();
    else if (strcmp(cmd, "close")== 0)
        ex2closecommand();
    else if (strcmp(cmd, "pfd")==0)
        ex2pfd();
    else if (strcmp(cmd, "cat")==0)
        ex2cat();
    else if (strcmp(cmd, "cp")==0)
        ex2cp();
    else if (strcmp(cmd, "mount")==0)
        ex2mount();
    else if (strcmp(cmd, "umount")==0)
        ex2umount();
    else if(strcmp(cmd, "cs")==0)
        ex2cs();
    else if (strcmp(cmd, "quit")==0)
       quit();
  }
}

int quit()
{
  int i;
  MINODE *mip;
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount > 0)
      iput(mip);
  }
  exit(0);
}

void ex2cs()
{
  if(running == &proc[0])
  {
    printf("Switching to P1\n");
    running = &proc[1];
  }
  else
  {
    printf("Switching to P0\n");
    running = &proc[0];
  }
}