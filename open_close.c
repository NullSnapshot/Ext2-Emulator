/************* open_close.c file **************/

int ex2opencommand()
{
    char *end;
    int mode = strtol(arg2, &end, 10);

    int fdIndex = ex2open(pathname, mode);
    if(fdIndex != -1)
    {
        OFT *fdPtr = running->fd[fdIndex];
        printf("File open in index %d | mode: %d | offset: %d\n", fdIndex, fdPtr->mode, fdPtr->mode);
    }

}

int ex2open(char *filename, int flags)
{

    int ino = getino(filename);
    if (ino == 0) // file doesn't exist
    {
        ex2creat(filename); // create and get ino.
        ino = getino(filename);
    }
    MINODE *mip = iget(dev, ino);

    if(S_ISREG(mip->INODE.i_mode))
    {
        // verify file's not already opened
        for(int i = 0; i<NFD; i++)
        {
            if(oft[i].minodePtr == mip)
            {
                if(oft[i].mode != 0)
                {
                    iput(mip);
                    printf("Cannot open '%s': file is already open!\n", filename);
                    return -1;
                }
            }
        }
        // File isn't open in incompatible mode.
        int i = 0;
        // find an open OFT.
        for(int i = 0; i<NFD; i++)
        {
            if(oft[i].refCount == 0) // free
            {
                OFT *oftp = &oft[i];
                oftp->mode = flags; // set flags 0 | 1 | 2 | 3 -> R | W | RW | APPEND
                oftp->refCount = 1;
                oftp->minodePtr = mip; // Point at file's minode.

                switch(flags)
                {
                    case 0: oftp->offset = 0; // R
                            break;
                    case 1: truncate(mip); // W
                            break;
                    case 2: oftp->offset = 0; // RW
                            break;
                    case 3: oftp->offset = mip->INODE.i_size; // append
                            break;
                    default: printf("invalid mode\n");
                            return(-1);
                }
                for(int j=0; j<NFD; j++)
                {
                    if(running->fd[j] == NULL)
                    {
                        running->fd[j] = oftp;
                        INODE *ip = &mip->INODE;
                        if(flags == 0)
                        {
                            ip->i_atime = time(0L);
                            return j;
                        }
                        else
                        {
                            ip->i_atime = ip->i_mtime = time(0L);
                            return j;
                        }
                    }
                }
            }
        }
    }
    else
    {
        iput(mip);
        printf("Cannot open '%s': location is not regular file\n", filename);
        return -1;
    }
}

int truncate(MINODE *mip)
{
    char buf[BLKSIZE];
    for(int i = 0; i < 12; i++) // dealloc direct
    {
        if(mip->INODE.i_block[i] == 0)
        {
            mip->INODE.i_mtime = mip->INODE.i_atime = time(0L);
            mip->dirty = 1;
            return;
        }
            
        bdalloc(mip->dev, mip->INODE.i_block[i]);
    }

    //dealloc indirect
    if(mip->INODE.i_block[12] != 0)
    {
        int ibuf[256];
        get_block(mip->dev, mip->INODE.i_block[12], ibuf);
        for(int i=0; i<256; i++)
        {
            if(ibuf[i] == 0)
            {
                mip->INODE.i_mtime = mip->INODE.i_atime = time(0L);
                mip->dirty = 1;
                mip->INODE.i_size = 0;
                bdalloc(mip->dev, mip->INODE.i_block[12]);
                return;
            }
            bdalloc(mip->dev, ibuf[i]);
        }
        bdalloc(mip->dev, mip->INODE.i_block[12]);
    }
   
    //dealloc doubleindirect
    if(mip->INODE.i_block[13] != 0)
    {
        int ibuf[256];
        int dibuf[256];
        get_block(mip->dev, mip->INODE.i_block[13], ibuf);
        for(int i=0; i<256; i++)
        {
            if(ibuf[i] == 0)
            {
                mip->INODE.i_mtime = mip->INODE.i_atime = time(0L);
                mip->dirty = 1;
                mip->INODE.i_size = 0;
                return;
            }
            get_block(mip->dev, mip->INODE.i_block[13],dibuf);
            for(int j=0; j<256; j++)
            {
                if(dibuf[j] == 0)
                {
                mip->INODE.i_mtime = mip->INODE.i_atime = time(0L);
                mip->dirty = 1;
                mip->INODE.i_size = 0;
                bdalloc(mip->dev, mip->INODE.i_block[13]);
                return;
                bdalloc(mip->dev, dibuf[j]);
                }
            }
        }
        bdalloc(mip->dev, mip->INODE.i_block[13]);
    }
    mip->dirty = 1;
    mip->INODE.i_size = 0;

}

int ex2closecommand()
{
    char *end;
    int fdIndex = strtol(pathname, &end, 10);
    ex2close(fdIndex);
}

void ex2close(int fd)
{
    if(fd > NFD || fd < 0)
    {
        return printf("File descriptor index '%d' is out of range\n",fd);
    }
    if(running->fd[fd] == NULL)
    {
        return printf("File descriptor index '%d' does not point at an active OFT\n",fd);
    }
    OFT *oftp = running->fd[fd];
    running->fd[fd] = NULL;
    oftp->refCount--;
    if (oftp->refCount > 0) return 0;

    // last user of this OFT entry disposes of the MINODE[]
    MINODE *mip = oftp->minodePtr;
    iput(mip);

    return 0;
}

int ex2lseek(int fd, int position)
{
    OFT *oftp = running->fd[fd];
    MINODE *mip = oftp->minodePtr;
    if(position < 0 || position > mip->INODE.i_size)
        return -1;
    oftp->offset = position;
    return 1;
}
void ex2pfd()
{
    printf("%4s\t%4s\t%6s\t%10s\n", "fd", "mode", "offset", "INODE");
    printf("%4s\t%4s\t%6s\t%10s\n", "----", "----", "------", "-------");
    for(int i= 0; i < NFD; i++)
    {
        OFT *oftp = running->fd[i];
        if(oftp != NULL)
        {
            char mode[6];
            switch(oftp->mode)
            {
                case 0:
                    strcpy(mode, "READ");
                    break;
                case 1:
                    strcpy(mode, "WRITE");
                    break;
                case 2:
                    strcpy(mode, "READWR");
                    break;
                case 3:
                    strcpy(mode, "APPEND");
                    break;
                default:
                    strcpy(mode, "UNDEF");
                    break;
            }
            printf("%4d\t%4s\t%6d\t[%3d, %3d]\n",i, mode, oftp->offset, oftp->minodePtr->dev, oftp->minodePtr->ino);
        }
    }
    printf("--------------------------------------\n");
}