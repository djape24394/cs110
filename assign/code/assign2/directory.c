#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// remove the placeholder implementation and replace with your own
int directory_findname(struct unixfilesystem *fs, const char *name,
		       int dirinumber, struct direntv6 *dirEnt) {
  struct inode ind;
  if(inode_iget(fs, dirinumber, &ind) == -1) return -1;
  if((ind.i_mode & IFMT) != IFDIR)
  {
    fprintf(stderr, "directory_findname: inode %d is not directory inode", dirinumber);
    return -1;
  }
  struct direntv6 buff[DISKIMG_SECTOR_SIZE / sizeof(struct direntv6)];
  int file_size = inode_getsize(&ind);
  for(int block_idx = 0; block_idx * DISKIMG_SECTOR_SIZE < file_size; block_idx++)
  {
    int i_nof_bytes = file_getblock(fs, dirinumber, block_idx, buff);
    if(i_nof_bytes == -1) return -1;
    for(int buff_idx = 0; buff_idx * (int)sizeof(struct direntv6) < i_nof_bytes; buff_idx++)
    {
      if(strncmp(name, buff[buff_idx].d_name, sizeof(buff[buff_idx].d_name)) == 0)
      {
        *dirEnt = buff[buff_idx];
        return 0;
      }
    }
  }
  return -1;
}
