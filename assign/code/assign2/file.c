#include <stdio.h>
#include <assert.h>

#include "file.h"
#include "inode.h"
#include "diskimg.h"

// remove the placeholder implementation and replace with your own
int file_getblock(struct unixfilesystem *fs, int inumber, int blockNum, void *buf) {
  struct inode in;
  if(inode_iget(fs, inumber, &in) == -1) return -1;
  int i_file_size = inode_getsize(&in);
  int i_nof_necessary_blocks = (i_file_size - 1) / DISKIMG_SECTOR_SIZE + 1;
  if(blockNum < 0 || blockNum >= i_nof_necessary_blocks)
  {
    fprintf(stderr, "file_getblock, blockNum exceeds file size\n");
    return -1;  
  }
  int i_sector_number = inode_indexlookup(fs, &in, blockNum);
  if(i_sector_number == -1)return -1;
  int i_read_nof_bytes = diskimg_readsector(fs->dfd, i_sector_number, buf);
  if(i_read_nof_bytes == -1) 
  {
    fprintf(stderr, "file_getblock, Error reading sector %d, returning -1\n", i_sector_number);
    return -1;  
  }

  if(blockNum < i_nof_necessary_blocks - 1)
  {
    return DISKIMG_SECTOR_SIZE;
  }
  
  return i_file_size - (i_nof_necessary_blocks - 1) * DISKIMG_SECTOR_SIZE;
}
