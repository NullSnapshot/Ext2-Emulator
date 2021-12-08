/************* rmdir.c file **************/

int ex2rmdir()
{
    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    //verify INODE is a dir.
    if(S_ISDIR(ip->i_mode))
    {
        if(mip->refCount > 1) // we have to go one higher since we ref it to get info.
            return printf("Cannot remove directory '%s': target is in use\n", pathname);
        
        // verify directory is empty
        int entry_count = 0;
        char buf[BLKSIZE];
        int test = ip->i_blocks;
        for(int i=0; i<ip->i_blocks; i++)
        {
            if (ip->i_block[i] == 0) break;

            get_block(dev, ip->i_block[i], buf);
            DIR *dp = (DIR *)buf;
            char *cp = dp;
            while(cp + dp->rec_len <= buf + BLKSIZE)
            {
                printf("Name found: %s \n", dp->name);
                cp += dp->rec_len;
                dp = (DIR *)cp;
                entry_count++;
            }
        }
        if(entry_count > 2)
            return printf("Cannot remove directory '%s': directory is not empty\n", pathname);
        
        int pino = findino(mip, &ino);
        MINODE *pmip = iget(mip->dev, pino);

        char name[128];
        findmyname(pmip, ino, &name);

        ex2rm_child(pmip, name);

        INODE *pip = &pmip->INODE;
        pip->i_links_count -= 1;
        pmip->dirty = 1;
        iput(pmip);

        /* deallocate data blocks and inode */
        bdalloc(mip->dev, mip->INODE.i_block[0]);
        idalloc(mip->dev, mip->ino);
        iput(mip);
    }
    else
        return printf("Cannot remove directory '%s': target location is not a directory\n", pathname);
}

// remove from parent directory.
int ex2rm_child(MINODE* pmip, char *myname)
{
    char buf[BLKSIZE];
    INODE *pip = &pmip->INODE;
    for(int i=0; i<pip->i_blocks; i++)
    {
        if(pip->i_block[i] == 0)
            return printf("FSPanic: reached end of directory without finding child\n");

        get_block(pmip->dev, pip->i_block[i], buf);
        DIR *dp = (DIR *)buf;
        char *cp = dp;
        char *prevcp = dp;
        size_t excess_len = 0;
        while(cp < buf + BLKSIZE)
        {
            char dpname[128];
            strncpy(dpname, dp->name, dp->name_len);
            dpname[dp->name_len] = 0;
            if(strcmp(dpname, myname) == 0) // match
            {
                // case removal
                // case 1, only entry in datablock.
                if(dp->rec_len == BLKSIZE)
                {
                    bdalloc(pmip->dev, pip->i_block[i]);
                    pip->i_size -=BLKSIZE;
                    // compact entries.
                    for(int j=0; j<i; j++)
                    {
                        if(pip->i_block[j] == 0)
                        {
                            pip->i_block[j] = pip->i_block[i];
                            pip->i_block[i] = 0; 
                            
                            put_block(pmip->dev, pip->i_block[i], buf);
                            pmip->dirty = 1;
                            return;

                        }
                    }
                }
                // Case 2, last entry in block.
                if(cp + dp->rec_len == buf + BLKSIZE)
                {
                    DIR *prevdp = (DIR *)prevcp;
                    prevdp->rec_len += dp->rec_len;
                    
                    put_block(pmip->dev, pip->i_block[i], buf);
                    pmip->dirty = 1;

                    return;
                }
                // Case 3, entries follow removed entry.
                if(cp + dp->rec_len != buf + BLKSIZE)
                {

                    // Save the memmove arguments since we'll be finding the last entry to expand its size.
                    DIR *translateDestination = (DIR *)cp;
                    char *translateSource = (cp+dp->rec_len);

                    // Adjustment to account for the size of the directory we're removing.
                    excess_len -= dp->rec_len;

                    // Advance to last record
                    while (cp + dp->rec_len < buf + BLKSIZE)
                    {
                        excess_len += dp->rec_len;
                        cp += dp->rec_len;
                        dp = (DIR *)cp;
                    }

                    // set last record to fill remaining space in block.
                    dp->rec_len = BLKSIZE - excess_len;

                    // now we can do the move.
                    memmove(translateDestination, translateSource, BLKSIZE - excess_len);
                    
                    // commit to drive.
                    put_block(pmip->dev, pip->i_block[i], buf);
                    pmip->dirty = 1;

                    return;
                    
                }
            }
            prevcp = cp;
            excess_len += dp->rec_len;
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
    
}