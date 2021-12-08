/************* cd_ls_pwd.c file **************/


int cd()
{
  if(access(pathname, 'r') && access(pathname, 'x'))
  {
    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);
    if(S_ISDIR(mip->INODE.i_mode))
    {
      iput(running->cwd);
      running->cwd = mip;
    }
    else
    {
      printf("%s is not a directory!\n", pathname);
    }
  }
  else
  {
    printf("Access Denied: You do not have permission to access %s\n",pathname);
  }
}

// ref: 257
char *modedescriptor1 = "xwrxwrxwr-------";
char *modedescriptor2 = "----------------";
int ls_file(MINODE *mip, char *name)
{
  char ftime[64];
  INODE ip = mip->INODE;
  if (S_ISREG(ip.i_mode))
    printf("%c", '-');
  if (S_ISDIR(ip.i_mode)) // ISDIR
    printf("%c", 'd');
  if (S_ISLNK(ip.i_mode))
    printf("%c", 'l');
  for (int i=8; i >= 0; i--)
  {
    if (ip.i_mode & (1 << i))
      printf("%c", modedescriptor1[i]);
    else
      printf("%c", modedescriptor2[i]);
  }
  printf("%4d ", ip.i_links_count);
  printf("%4d ", ip.i_gid);
  printf("%4d ", ip.i_uid);

  //time
  time_t timevar = ip.i_ctime;
  strcpy(ftime, ctime(&timevar));
  ftime[strlen(ftime)-1] = 0; // remove \n
  printf("%s ", ftime);

  //size and name
  printf("%8d ", ip.i_size);
  printf("\t %s \t", name);

  //symlink
  if (S_ISLNK(ip.i_mode))
  {
    char buf[60];
    strncpy(buf, (char *)ip.i_block, 60);
    printf("[symlnk-> %s] ",buf);
  }

  //mount
  if (mip->mounted != 0)
  {
    MOUNT *mptr = mip->mptr;
    printf("[mnt->%s] ",mptr->name);
  }

  printf("\n");
}

int ls_dir(MINODE *mip)
{
  char buf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;

  get_block(dev, mip->INODE.i_block[0], buf);
  dp = (DIR *)buf;
  cp = buf;
  
  while (cp < buf + BLKSIZE){
     strncpy(temp, dp->name, dp->name_len);
     temp[dp->name_len] = 0;
    
      // get stat
      MINODE *dirmip = iget(dev, dp->inode);
      ls_file(dirmip, temp);
      iput(dirmip);

     cp += dp->rec_len;
     dp = (DIR *)cp;
  }
  printf("\n");
}

// ref 330
int ls()
{
  if(strcmp(pathname, "")==0)
    ls_dir(running->cwd);
  else
  {
    int target = getino(pathname);
    MINODE *targetmip = iget(dev, target);
    if(S_ISDIR(targetmip->INODE.i_mode))
    {
      ls_dir(targetmip);
    }
    else
    {
      char *targetname = NULL;
      int i = 0;
      while(targetname == NULL)
      {
        if(name[i] == NULL)
        {
          targetname = name[i-1];
        }
        printf("name[%d]: %s \n", i, name[i]);
        i++;
      }
      ls_file(targetmip, targetname);
    }

  }
}

char *pwd(MINODE *wd)
{
  if (wd == root){
    printf("/\n");
    return;
  }
  else
  {
    rpwd(wd);
  }
  printf("\n");
}


int rpwd(MINODE *wd)
{
  u32 my_ino, parent_ino;
  char my_name[255];

  if (wd == root) return;
  //from wd-> INODE.i_block[0] get my_ino and parent_ino
  parent_ino = findino(wd, &my_ino);
  MINODE *pip = iget(dev, parent_ino);
  findmyname(pip, my_ino, my_name);
  rpwd(pip); // recurse call rpwd(pip) with parent minode
  printf("/%s", my_name);
  iput(pip);
}

