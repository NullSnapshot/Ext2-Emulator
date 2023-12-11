/************* mkdir_creat.c file **************/

int ex2mkdir()
{
    char basename[128], targetLocation[128], targetDir[128];
    // copy so we can preserve for use.
    strcpy(targetLocation, pathname);
    getTargetDirectory(pathname, &basename, &targetDir);
    //printf("target: %s | %s \n", targetDir, basename);
    int pino = getino(targetDir);
    MINODE *pmip = iget(dev, pino);

    if(pino == 0)
    {
        printf("Cannot create directory '%s': no such file or directory\n",targetLocation);
        iput(pmip);
        return;
    }
    INODE PI = pmip->INODE;
    if(S_ISDIR(PI.i_mode))
    {
        if(search(pmip, &basename) == 0)
        {
            kmkdir(pmip, basename);
        }
        else
        {
            iput(pmip);
            return printf("Cannot create directory '%s': file or directory already exists\n", targetLocation);
        }
    }
    else
    {
        iput(pmip);
        return printf("Cannot create directory '%s': target location is not a directory\n", targetLocation);
    }
    
}

int kmkdir(MINODE *pmip, char *basename)
{
    int ino = ialloc(dev);
    int blk = balloc(dev);
    MINODE *mip = iget(dev, ino); // load new inode into a minode.
    INODE *ip = &mip->INODE;
    ip->i_mode = 0x41ED;    // 040755: DIR type and perms
    ip->i_uid = running->uid;   // owner id
    ip->i_gid = running->gid;   //g roup id
    ip->i_size = BLKSIZE;       // inode size in bytes.
    ip->i_links_count = 2;      // initialized directories have 2 links, ./ and ../
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
    ip->i_blocks = 2;           // LINUX: blocks count in 512 byte chunks.
    ip->i_block[0] = blk;       // new DIR has one data block
    for(int i=1; i<=14; i++)
    {
        ip->i_block[i] = 0; // 0 out the rest of the blocks.
    }
    mip->dirty = 1;         // mark minode dirty (modified)
    iput(mip);

    // Create data block for DIR entries.
    char buf[BLKSIZE];
    bzero(buf, BLKSIZE); // 0 out buffer. No junk data.
    DIR *dp = (DIR *)buf;

    // build ./ entry
    dp->inode = ino;
    dp->rec_len = 12;
    dp->name_len = 1;
    dp->name[0] = '.';

    // build ../ entry
    dp = (char *)dp + 12;

    // build parent ino
    int pino = pmip->ino;
    dp->inode = pino;
    dp->rec_len = BLKSIZE-12; // rec_len spans full block.
    dp->name_len = 2;
    dp->name[0] = dp->name[1] = '.';

    // Write block to disk.
    put_block(dev, blk, buf);

    // add dir_entry to parent directory
    enter_name(pmip, ino, basename);
    
}

int ex2creat(char *pathname)
{
    char basename[128], targetLocation[128], targetDir[128];
    strcpy(targetLocation, pathname);
    getTargetDirectory(pathname, &basename, &targetDir);
    int pino = getino(targetDir);
    MINODE *pmip = iget(dev, pino);

    if(pino == 0)
    {
        printf("Cannot create file '%s': no such parent directory\n",targetLocation);
        return;
    }
    INODE PI = pmip->INODE;
    if(S_ISDIR(PI.i_mode))
    {
        if(search(pmip, &basename) == 0)
        {
            kmkfile(pmip, basename);
            iput(pmip);
        }
        else
            return printf("Cannot create file '%s': file or directory already exists\n", targetLocation);
    }
    else
        return printf("Cannot create file '%s': target location is not a directory\n", targetLocation);
    
}


int kmkfile(MINODE *pmip, char *basename)
{
    int ino = ialloc(dev);
    MINODE *mip = iget(dev, ino); // load new inode into minode.
    INODE *ip = &mip->INODE;
    // Write inode data
    ip->i_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    ip->i_uid = running->uid;
    ip->i_gid = running->gid;
    ip->i_size = 0;
    ip->i_links_count = 1;
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
    ip->i_blocks = 0;
    ip->i_block[0]=0;

    mip->dirty = 1;
    iput(mip);

    enter_name(pmip, ino, basename);

}

int enter_name(MINODE *pip, int ino, char *myname)
{
    MINODE *mip = iget(dev, ino);
    char buf[BLKSIZE];
    get_block(dev, mip->INODE.i_block[0], buf);
    DIR *targetblock = (DIR *)buf;
    int n_len = targetblock->name_len;
    int needed_length = 4 *( (8 + n_len + 3)/4);
    INODE *ip = &pip->INODE;
    for(int i=0; i<12; i++)
    {
        // Load block
        if (ip->i_block[i] == 0) break;
        get_block(dev, ip->i_block[i], buf);
        //Step to last entry of block
        DIR *dp = (DIR *)buf;
        char *cp = buf;
        while (cp + dp->rec_len < buf + BLKSIZE)
        {
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
        // Now pointing to last entry in block
        int ideal_length = 4 * ( (8 + dp->name_len + 3)/4);
        int remaining_length = dp->rec_len - ideal_length;
        if (remaining_length >= needed_length)
        {
            // enter new entry as last entry and trim previous entry to ideal_length.
            DIR *newdp = cp; // newDP points at last entry.
            cp += ideal_length;
            newdp = (DIR *)cp; // newDP points to space we'll start writing the new entry.
            newdp->rec_len = dp->rec_len - ideal_length;
            dp->rec_len = ideal_length;

            newdp->inode = ino;
            newdp->name_len = strlen(myname);
            
            // get file type of new entry.
            int imode = mip->INODE.i_mode;
            if(S_ISREG(imode))
                newdp->file_type = 1;
            if(S_ISDIR(imode))
                newdp->file_type = 2;
            if(S_ISLNK(imode))
                newdp->file_type = 7;
            
            strncpy(newdp->name, myname, strlen(myname));

            put_block(dev, ip->i_block[i], buf);
            pip->dirty = 1;         // mark minode dirty (modified)
            iput(mip);
            return 1;
        }
    }
    // All blocks are exhausted.
    int newblock = balloc(dev);
    for(int i=0; i<12; i++)
    {
        if(ip->i_block[i] == 0)
        {
            // This is our free block.
            ip->i_block[i] = newblock;
            get_block(dev, ip->i_block[i], buf);
            DIR *dp = (DIR *)buf;
            dp->rec_len = BLKSIZE;
            dp->inode = ino;
            dp->name_len = strlen(myname);
            
            // get file type of new entry.
            int imode = mip->INODE.i_mode;
            if(S_ISREG(imode))
                dp->file_type = 1;
            if(S_ISDIR(imode))
                dp->file_type = 2;
            if(S_ISLNK(imode))
                dp->file_type = 7;
            strncpy(dp->name, myname, strlen(myname));
            put_block(dev, ip->i_block[i], buf);
            ip->i_size += BLKSIZE;
            pip->dirty = 1;         // mark minode dirty (modified)
            iput(mip);
            return 1;
        }
    }
    // If we get here, we've already used all 12 iblocks.
    printf("FS Panic: Directory full\n");
    return -1;
}