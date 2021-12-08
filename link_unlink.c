/************* link_unlink.c file **************/

int ex2link()
{
    char newfileParent[128], newfileName[128];
    getTargetDirectory(arg2, &newfileName, newfileParent);

    int oino = getino(pathname);
    MINODE *omip =iget(dev, oino);
    if(S_ISDIR(omip->INODE.i_mode))
    {
        iput(omip);
        return printf("Cannot link '%s': source is a directory\n", pathname);
    }
    int newino = getino(arg2);
    if(newino != 0)
    {
        iput(omip);
        return printf("Cannot link to %s': file or directory already exists at that location\n", arg2);
    }
    int pino = getino(newfileParent);
    
    MINODE *pmip = iget(dev, pino);
    enter_name(pmip, oino, &newfileName);

    omip->INODE.i_links_count++; // inc INODE's links count by one.
    omip->dirty = 1;
    iput(omip);
    iput(pmip);

    char buf[BLKSIZE];
    get_block(dev, pmip->INODE.i_block[0], buf);
    DIR *dp = (DIR *)buf;
    char *cp = dp;
    while (cp < buf + BLKSIZE)
    {
        //printf("%d|%d|%d|%s\n", dp->inode, dp->rec_len, dp->name_len, dp->name);
        cp += dp->rec_len;
        dp = (DIR *)cp;
    }
}

int ex2unlink()
{
    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;
    if(ip->i_uid != running->uid) // not owner
    {
        iput(mip);
        return printf("Access Denied: Not owner of file '%s'\n", pathname);
    }
    // file has to be either a regular or symbolic link file.
    if(S_ISREG(mip->INODE.i_mode) || S_ISLNK(mip->INODE.i_mode))
    {
        char fileParent[128], fileName[128];
        getTargetDirectory(pathname, &fileName, &fileParent);
        int pino = getino(fileParent);
        MINODE *pmip = iget(dev, pino);
        ex2rm_child(pmip, fileName);
        pmip->dirty = 1;
        iput(pmip);

        //dec link count by one.
        mip->INODE.i_links_count--;
        if(mip->INODE.i_links_count > 0)
        {
            mip->dirty = 1;
        }
        else // links_count = 0, remove file
        {
            for(int i=0; i<mip->INODE.i_blocks; i++)
            {
                bdalloc(mip->dev, mip->INODE.i_block[i]);
                idalloc(mip->dev, mip->ino);
            }
        }
    }
    else
    {
        printf("Cannot unlink '%s': location is not a file or symbolic link\n");
    }
    iput(mip);
}