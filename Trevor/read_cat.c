/************* mkdir_creat.c file **************/

int ex2read(int fd, char buf[ ], int nbytes)
{
    // assume fd is open for read.
    OFT *oftp = running->fd[fd];
    MINODE *mip = oftp->minodePtr;
    int remainingbytes;
    int avil = mip->INODE.i_size - oftp->offset;
    char *cq = buf;
    char *blk;
    int count = 0;

    while (nbytes && avil)
    {
        // compute logical block number lbk and startByte in that block from offset;
        int lbk = oftp->offset / BLKSIZE;
        int startbyte = oftp->offset % BLKSIZE;
        if (lbk < 12) // lbk is a direct block.
        {
            blk = mip->INODE.i_block[lbk];
        }
        else if(lbk >= 12 && lbk < 256 + 12) // indirect block
        {
            int ibuf[256];
            get_block(mip->dev, mip->INODE.i_block[12], ibuf);
            blk = ibuf[lbk-12];
            
        }
        else // double indirect
        {
            lbk -= (12+256); // account for offset from already accessing direct and indirect blocks.
            int ibuf[256];
            int dibuf[256];
            get_block(mip->dev, mip->INODE.i_block[13], ibuf);
            int dblk = ibuf[lbk/256];
            get_block(mip->dev, dblk, dibuf);
            blk = dibuf[lbk % 256];
        }
        // Get the data block into readbuf[BLKSIZE]
        char readbuf[BLKSIZE];
        get_block(mip->dev, blk, readbuf);
        char *cp = readbuf + startbyte;
        remainingbytes = BLKSIZE - startbyte;

        int bytestocopy;
        if(remainingbytes < avil)
        {
            bytestocopy = remainingbytes;
        }
        else
        {
            bytestocopy = avil;
        }
           memcpy(cq, cp, bytestocopy);
           oftp->offset += bytestocopy;
           count += bytestocopy;
           avil -= bytestocopy;
           nbytes -= bytestocopy;
        //}

        // if one data block is not enough, we can just loop back to outer while for more.
    }
    //printf("ex2read: read %d char from file descriptor %d\n", count, fd);
    return count;
}

void ex2cat()
{
    char mybuf[1024], dummy = 0; // a null char at end of mybuf[ ]
    int n;

    int fd = ex2open(pathname, 0);
    while(n = ex2read(fd, mybuf, 1024))
    {
        mybuf[n] = 0;
        fwrite(mybuf, 1, BLKSIZE, stdout);
        bzero(mybuf, 1024);
    }
    ex2close(fd);
}