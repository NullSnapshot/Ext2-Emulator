/************* mount_umount.c file **************/

void ex2mount()
{
    if( strcmp(pathname, "") == 0 && strcmp(arg2, "") == 0) // display current mounts.
    {
        for(int i = 0; i < 8; i++)
        {
            MOUNT *disk = &mountTable[i];
            if(disk->dev != 0)
            {
                printf("%s [%d] mounted on %s\n",disk->name, disk->dev, disk->mount_name);
            }
        }
    }
    else if( strcmp(pathname, "") == 0 || strcmp(arg2, "") == 0) // missing a command
    {
            printf("Cannot mount drive '%s': Please specify a location to mount to\n", pathname);
            return;
    }
    else // user specified mount
    {
        for(int i =0; i <8; i++)
        {
            MOUNT *disk = &mountTable[i];
            if(strcmp(disk->name, pathname) == 0) // already mounted
            {
                printf("Disk '%s' is already mounted\n", pathname);
                return;
            }
        }
        // Disk isn't mounted
        int newDiskIndex = -1;
        for(int i=0; i < 8; i++)
        {
            MOUNT *disk = &mountTable[i];
            if(disk->dev == 0) // found a free disk mount.
            {
                newDiskIndex = i;
                break;
            }
        }

        if(newDiskIndex == -1)
        {
            printf("Cannot mount drive '%s': All mount points currently in use\n", pathname);
            return;
        }

        // we can mount if we're here.
        int fd;
        if ((fd = open(pathname, O_RDWR)) < 0)
        {
            printf("Cannot mount drive '%s': File could not be found or opened\n", pathname);
            return;
        }

        //setup for EX2 verification.
        char buf[BLKSIZE];
        SUPER *newsp;

        // Read super block
        get_block(fd, 1, buf);
        newsp = (SUPER *)buf;

        // verify EX2
        if (newsp->s_magic != 0xEF53)
        {
            printf("Cannot mount drive '%s': Drive is not an ex2 filesystem\n", pathname);
            close(fd);
            return;
        }

        // get mountpoint
        int ino = getino(arg2);
        MINODE *mip = iget(dev, ino);

        //verify mount point is a dir and not busy
        if(!S_ISDIR(mip->INODE.i_mode))
        {
            printf("Cannot mount drive '%s' to '%s': Mount point is not a directory\n", pathname, arg2);
            close(fd);
            iput(mip);
            return;
        }
        if(mip->refCount > 1) // mount point is busy
        {
            printf("Cannot mount drive '%s' to '%s': Mount point is in use\n", pathname, arg2);
            close(fd);
            iput(mip);
            return;
        }

        mountTable[newDiskIndex].dev = fd;
        mountTable[newDiskIndex].ninodes = newsp->s_inodes_count;
        mountTable[newDiskIndex].nblocks = newsp->s_blocks_count;

        get_block(fd, 2, buf);
        GD *newgp = (GD *)buf;

        mountTable[newDiskIndex].bmap = newgp->bg_block_bitmap;
        mountTable[newDiskIndex].imap = newgp->bg_inode_bitmap;
        mountTable[newDiskIndex].iblk = newgp->bg_inode_table;
        mountTable[newDiskIndex].mounted_inode = mip;
        strncpy(mountTable[newDiskIndex].name,pathname,strlen(pathname));
        strncpy(mountTable[newDiskIndex].mount_name, arg2, strlen(arg2));
        mip->mounted = 1;
        mip->mptr = &mountTable[newDiskIndex];
        MOUNT *sanitycheck = &mountTable[newDiskIndex];
        printf("Mounted '%s' to '%s'\n", pathname, arg2);
        return 0;
    }
}

void ex2umount()
{
    int mountIndex = -1;
    for(int i =0; i <8; i++)
    {
        MOUNT *disk = &mountTable[i];
        if(strcmp(disk->name, pathname) == 0) 
        {
            mountIndex = i;
            break;
        }
    }
    if(mountIndex == -1)
    {
        printf("Cannot unmount '%s': No mount entry found for directory\n", pathname);
        return;
    }
    MOUNT *mountp = &mountTable[mountIndex];
    MINODE *mip = mountp->mounted_inode;
    for(int i = 0; i<NMINODE; i++)
    {
        MINODE *mip = &minode[i];
        if(mip->dev == mountp->dev)
            return printf("Cannot unmount '%s': Mounted file system is still in use\n",pathname);
    }
    mip->mounted = 0;
    mip->mptr = 0;
    close(mountTable[mountIndex].dev);
    mountTable[mountIndex].dev = 0; // free resource again.
    iput(mip);
    return 0;
}