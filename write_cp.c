/************* mkdir_creat.c file **************/

void ex2write(int fd, char buf[ ], int nbytes)
{
    OFT *oftp = running->fd[fd];
    MINODE *mip = oftp->minodePtr;
    char *blk;
    char *cq = buf;
    while (nbytes > 0)
    {
        // compute logical block and startByte in that lbk.
        int lbk = oftp->offset / BLKSIZE;
        int startByte = oftp->offset % BLKSIZE;

        if(lbk < 12) // direct block
        {
            if (mip->INODE.i_block[lbk] == 0) // no data block
            {
                mip->INODE.i_block[lbk] = balloc(mip->dev); // allocate.
            }
            blk = mip->INODE.i_block[lbk]; // blk should be a disk block now.
        }
        else if(lbk >= 12 && lbk <256 + 12) // indirect block
        {
            if(mip->INODE.i_block[12] == 0)
            {
                mip->INODE.i_block[12] = balloc(mip->dev);

                // zero out block      
                char zeroblock[BLKSIZE];
                bzero(zeroblock, BLKSIZE);
                put_block(mip->dev, mip->INODE.i_block[12], zeroblock);
            }
            int ibuf[256];
            get_block(mip->dev, mip->INODE.i_block[12], ibuf);
            blk = ibuf[lbk-12];
            if(blk == 0) // allocate a disk block.
            {
                blk = balloc(mip->dev);

                // zero out block      
                char zeroblock[BLKSIZE];
                bzero(zeroblock, BLKSIZE);
                put_block(mip->dev, blk, zeroblock);
                ibuf[lbk-12] = blk;

            }
            // commit block indirect block
            put_block(mip->dev, mip->INODE.i_block[12], ibuf);
        }
        else // double indirect
        {
            lbk -= (12+256); // account for offset from already accessing direct and indirect blocks.
            int ibuf[256];
            int dibuf[256];
            if(mip->INODE.i_block[13] == 0)
            {
                mip->INODE.i_block[13] = balloc(mip->dev);

                // zero out block      
                char zeroblock[BLKSIZE];
                bzero(zeroblock, BLKSIZE);
                put_block(mip->dev, mip->INODE.i_block[13], zeroblock);
            }
            get_block(mip->dev, mip->INODE.i_block[13], ibuf);
            int dblk = ibuf[lbk/256];
            if(dblk == 0) // no entry on ibuf table
            {
                dblk = balloc(mip->dev);
                ibuf[lbk/256] = dblk;

                // zero out block      
                char zeroblock[BLKSIZE];
                bzero(zeroblock, BLKSIZE);
                put_block(mip->dev, dblk, zeroblock);
            }
            // commit first level indirect block
            put_block(mip->dev, mip->INODE.i_block[13], ibuf);

            get_block(mip->dev, dblk, dibuf);
            blk = dibuf[lbk % 256];
            if(blk == 0) // no entry on dibuf table
            {
                blk = balloc(mip->dev);
                dibuf[lbk % 256] = blk;
            }
            // commit second level indirect block
            put_block(mip->dev, dblk, dibuf);
        }
        char writebuf[BLKSIZE];
        get_block(mip->dev, blk, writebuf);
        char *cp = writebuf + startByte;
        int remainingBytes = BLKSIZE - startByte;

        int bytestocopy;
        if(remainingBytes < nbytes) // limited by number of bytes in block.
        {
            bytestocopy = remainingBytes;
        }
        else // limited by number of bytes left to write.
            bytestocopy = nbytes;
        
        memcpy(cp, cq, bytestocopy);
        put_block(mip->dev, blk, writebuf);
        oftp->offset += bytestocopy;
        nbytes -= bytestocopy;
        mip->INODE.i_size += bytestocopy;
    }
    mip->dirty = 1;
    return nbytes;
}

void ex2cp()
{
    char mybuf[1024], dummy = 0;
    int n;

    int fd = ex2open(pathname, 0);
    int gd = ex2open(arg2, 0);

    while( n=ex2read(fd,mybuf, BLKSIZE))
    {
        ex2write(gd, mybuf, n);
    }
    ex2close(fd);
    ex2close(gd);
}