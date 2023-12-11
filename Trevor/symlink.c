/************* symlink.c file **************/

int ex2symlink()
{
    char newfileParent[128], newfileName[128];
    getTargetDirectory(arg2, &newfileName, newfileParent);

    int oino = getino(pathname);
    MINODE *omip =iget(dev, oino);
    if(oino == 0)
    {
        iput(omip);
        return printf("Cannot create symlink: File '%s' does not exist\n", pathname);
    }
    int newino = getino(arg2);
    if(newino != 0)
    {
        iput(omip);
        return printf("Cannot symlink to %s': file already exists at that location\n", arg2);
    }
    // Should be safe to create now!
    ex2creat(arg2);
    newino = getino(arg2);
    MINODE *mip = iget(dev, newino);
    INODE *ip = &mip->INODE;
    ip->i_mode = S_IFLNK | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    // asume length of old file name is <= 60 characters
    strncpy((char *)ip->i_block,pathname, strlen(pathname));
    ip->i_size = strlen(pathname);
    mip->dirty = 1;
    iput(mip);
    
}

int ex2readlink()
{
    char buf[60];
    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    if(S_ISLNK(ip->i_mode))
    {
        strncpy(buf, (char *)ip->i_block, 60);
        printf("%s is linked to %s\n", pathname, buf);
        printf("File size: %d\n", ip->i_size);
    }
    iput(mip);
}