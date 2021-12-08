/*********** util.c file ****************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <time.h>

#include "type.h"


/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;

extern char gpath[128];
extern char *name[64];
extern int n;

extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, iblk;

extern char line[128], cmd[32], pathname[128], arg2[128];

int get_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   read(dev, buf, BLKSIZE);
}   

int put_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   write(dev, buf, BLKSIZE);
}   

int tokenize(char *pathname)
{
  int i;
  char *s;
  printf("tokenize %s\n", pathname);

  strcpy(gpath, pathname);   // tokens are in global gpath[ ]
  n = 0;

  s = strtok(gpath, "/");
  while(s){
    name[n] = s;
    n++;
    s = strtok(0, "/");
  }
  name[n] = 0;
  
  for (i= 0; i<n; i++)
    printf("%s  ", name[i]);
  printf("\n");
}

// allocate a free minode for use
MINODE *mialloc()
{
   int i;
   for (i=0; i<NMINODE; i++)
   {
      MINODE *mp = &minode[i];
      if (mp->refCount == 0)
      {
         mp->refCount = 1;
         return mp;
      }
   }
   printf("FS panic: out of minodes\n");
   return 0;
}

// release a used minode
int midalloc(MINODE *mip)
{
   mip->refCount = 0;
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
  int i;
  MINODE *mip;
  char buf[BLKSIZE];
  int blk, offset;
  INODE *ip;

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount && mip->dev == dev && mip->ino == ino){
       mip->refCount++;
       //printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
       return mip;
    }
  }
    
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount == 0){
       //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
       mip->refCount = 1;
       mip->dev = dev;
       mip->ino = ino;

       // get INODE of ino to buf    
       blk    = (ino-1)/8 + iblk;
       offset = (ino-1) % 8;

       //printf("iget: ino=%d blk=%d offset=%d\n", ino, blk, offset);

       get_block(dev, blk, buf);
       ip = (INODE *)buf + offset;
       // copy INODE to mp->INODE
       mip->INODE = *ip;
       return mip;
    }
  }   
  printf("PANIC: no more free minodes\n");
  return 0;
}

//iblk is a global var
void iput(MINODE *mip)
{
 int i, block, offset;
 char buf[BLKSIZE];
 INODE *ip;

 if (mip==0) 
     return;

 mip->refCount--;
 
 if (mip->refCount > 0) return; // user still accessing mip.
 if (!mip->dirty)       return; // Don't need to write if the mip hasn't been modified.
 
 /* write INODE back to disk */
 block = (mip->ino -1) / 8 + iblk;
 offset = (mip->ino -1) % 8;

 //Get block containing this inode
 get_block(mip->dev, block, buf);
 ip = (INODE *)buf + offset;        // ip points at INODE
 *ip = mip->INODE;                  // copy INODE to inode in block
 put_block(dev, block, buf);   // write the inode back to the disk.
 midalloc(mip);                     // and set the refcount back to 0.
} 

int search(MINODE *mip, char *name)
{
   int i; 
   char *cp, c, sbuf[BLKSIZE], temp[256];
   DIR *dp;
   INODE *ip;

   printf("search for %s in MINODE = [%d, %d]\n", name,mip->dev,mip->ino);
   ip = &(mip->INODE);

   /*** search for name in mip's data blocks: ASSUME i_block[0] ONLY ***/

   get_block(dev, ip->i_block[0], sbuf);
   dp = (DIR *)sbuf;
   cp = sbuf;
   printf("  ino   rlen  nlen  name\n");

   while (cp < sbuf + BLKSIZE){
     strncpy(temp, dp->name, dp->name_len);
     temp[dp->name_len] = 0;
     printf("%4d  %4d  %4d    %s\n", 
           dp->inode, dp->rec_len, dp->name_len, dp->name);
     if (strcmp(temp, name)==0){
        printf("found %s : ino = %d\n", temp, dp->inode);
        return dp->inode;
     }
     cp += dp->rec_len;
     dp = (DIR *)cp;
   }
   return 0;
}

int getino(char *pathname)
{
  int i, ino, blk, offset;
  char buf[BLKSIZE];
  INODE *ip;
  MINODE *mip;

  printf("getino: pathname=%s\n", pathname);
  if (strcmp(pathname, "/")==0)
      return 2;
  
  // starting mip = root OR CWD
  if (pathname[0]=='/')
     mip = root;
  else
     mip = running->cwd;

  mip->refCount++;         // because we iput(mip) later
  
  tokenize(pathname);

  for (i=0; i<n; i++){
      printf("===========================================\n");
      printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);
 
      ino = search(mip, name[i]);

      if (ino==0){
         iput(mip);
         printf("name %s does not exist\n", name[i]);
         return 0;
      }
      iput(mip);
      mip = iget(dev, ino);
   }

   iput(mip);
   return ino;
}

// These 2 functions are needed for pwd()
int findmyname(MINODE *parent, u32 myino, char myname[ ]) 
{
  char *cp, sbuf[BLKSIZE], temp[256];
  DIR *dp;
  INODE *ip;
  ip = &(parent->INODE);

  get_block(dev, ip->i_block[0], sbuf);
  dp = (DIR *)sbuf;
  cp = sbuf;

  while (cp <sbuf + BLKSIZE)
  {
     //printf("dpinode %d | myinode %d\n", dp->inode, myino);
     if(dp->inode == myino)
     {
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0;
        strcpy(myname, temp);
        return 1;
     }
     cp += dp->rec_len;
     dp = (DIR *)cp;
  }
  return 0;
}

int findino(MINODE *mip, u32 *myino) // myino = i# of . return i# of ..
{
  // mip points at a DIR minode
  // WRITE your code here: myino = ino of .  return ino of ..
  // all in i_block[0] of this DIR INODE.
   char *cp, sbuf[BLKSIZE];
   ip = &(mip->INODE);
   get_block(dev, ip->i_block[0], sbuf);
   dp = (DIR *)sbuf;
   cp = sbuf + dp->rec_len; // advance pointer from current to parent.
   dp = (DIR *)cp;
   *myino = mip->ino;
   return iget(dev, dp->inode)->ino;
  
}

// Generic helper function that will let us input a path and get the name of the target location, as well
// as the directory it is located in.
void getTargetDirectory(char *targetLocation, char basename[ ], char targetDir[ ])
{
   char targetpath[128];
   strcpy(targetpath, "");
   strcpy(basename, "");
   strcpy(targetDir,"");
   if (strcmp(targetLocation, "/") == 0)
   {
      printf("Please specify a directory other than root! \n");
      return;
   }
   // We have to be careful about using tokenize, as we'll lose our entire path if we're not careful.
   // We should store the input in functions that will use getTargetDirectory so we don't lose user input.
   tokenize(targetLocation);
   strcpy(basename, name[n-1]);
   int endOffset = strlen(basename);
   strncpy(targetpath, targetLocation,strlen(targetLocation) - endOffset);
   targetpath[strlen(targetLocation) - endOffset - 1] = 0;
   strcpy(targetDir, targetpath);

   if(strcmp(targetpath,"") == 0) // CWD
      strcpy(targetDir, "./");
}

// tst_bit, set_bit functions
int tst_bit(char *buf, int bit)
{
   return buf[bit/8] & (1<< (bit % 8));
}
int set_bit(char *buf, int bit)
{
   buf[bit/8] |= (1 << (bit % 8));
}

// Allocate an inode number from inode_bitmap
int ialloc(int dev)
{
   int i;
   char buf[BLKSIZE];
   
   // read inode_bitmap block
   get_block(dev, imap, buf);

   for(i=0; i < ninodes; i++)
   {
      if(tst_bit(buf, i)== 0)
      {
         set_bit(buf, i);
         put_block(dev, imap, buf);
         printf("allocated ino = %d\n", i+1); // Bits count from 0; ino from 1
         return i+1;
      }
   }
   return 0; // out of free inodes
}

int balloc(int dev)
{
   int i;
   char buf[BLKSIZE];

   get_block(dev, bmap, buf);

   for(i=0; i < nblocks; i++)
   {
      if(tst_bit(buf, i)== 0)
      {
         set_bit(buf, i);
         put_block(dev, bmap, buf);
         printf("Allocated block = %d\n", i+1);
         return i+1;
      }
   }
   return 0; // Out of disk blocks.
}

// Deallocation functions
int clr_bit(char *buf, int bit) // clear bit in char buf[BLKSIZE]
{
   buf[bit/8] &= ~(1 << (bit%8));
}
int idalloc(int dev, int ino)
{
   int i;
   char buf[BLKSIZE];

   if(ino > ninodes)
   {
      printf("inumber %d out of range\n", ino);
      return;
   }

   get_block(dev, imap, buf); // get inode bitmap block into buf
   clr_bit(buf, ino-1); // clear test bit.
   put_block(dev, imap, buf); // write buf back to imap.
}

int bdalloc(int dev, int bno)
{
   int i;
   char buf[BLKSIZE];

   if(bno > nblocks)
   {
      printf("bnumber %d out of range\n", bno);
      return;
   }

   get_block(dev, bmap, buf);
   clr_bit(buf, bno-1);
   put_block(dev, bmap, buf);
}