#include <stdio.h>
#include <assert.h>

#include "inode.h"
#include "diskimg.h"

#define INODE_SIZE ((int)sizeof(struct inode))
#define NOF_INODES_PER_BLOCK (DISKIMG_SECTOR_SIZE / sizeof(struct inode))

// remove the placeholder implementation and replace with your own
int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
  // INODE_START_SECTOR starter sector for inodes ( its num 2)
  // fs->superblock.s_isize total number of blocks of inodes 
  // each block(sector) has 512/32 = 16 inodes, so total nof inodes is fs->superblock.s_isize * 16
  // if(inumber < 1 || inumber > (int)(fs->superblock.s_isize * NOF_INODES_PER_BLOCK))
  // {
  //   fprintf(stderr, "inorde number %d out of bound, returning -1\n", inumber);
  //   return -1;  
  // }
  int sector_number = INODE_START_SECTOR + (inumber - 1) / INODE_SIZE;
  struct inode buffer[NOF_INODES_PER_BLOCK];
  int i_nof_bytes = diskimg_readsector(fs->dfd, sector_number, buffer);
  if(i_nof_bytes == -1) 
  {
    fprintf(stderr, "inode_iget: Error reading sector %d, returning -1\n", sector_number);
    return -1;  
  }

  // int inode_index_in_buffer = (inumber - (sector_number - INODE_START_SECTOR) * NOF_INODES_PER_BLOCK) - 1;
  int inode_index_in_buffer = (inumber - 1) % NOF_INODES_PER_BLOCK;

  *inp = buffer[inode_index_in_buffer];

  return 0;
}

// remove the placeholder implementation and replace with your own
int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum) {
  int i_disk_block_number = -1;
  if((inp->i_mode & ILARG) == 0)
  {
    // direct addressing, i_addr array points to content block number
    i_disk_block_number = inp->i_addr[blockNum];
  }
  else
  {
    // indirect addressing
    // For unix V6, in the inode, the first 7 block numbers point to singly-indirect blocks,
    // but the last block number points to a block which itself stores singly-indirect block numbers.
    // so in total there are maximum 7 + 256 singly indirect blocks
    // block_numbers are of type uint16_t, 2 bytes
    uint16_t buff[DISKIMG_SECTOR_SIZE / sizeof(uint16_t)];
    int i_bocks_nums_in_indirect_block = DISKIMG_SECTOR_SIZE / sizeof(uint16_t); // 256, number of block numbers in one indirect block 
    int i_indirect_block_idx = blockNum / i_bocks_nums_in_indirect_block; // [0..263]
    int i_indirect_block_disc_number;
    if(i_indirect_block_idx < 7)
    {
      i_indirect_block_disc_number = inp->i_addr[i_indirect_block_idx];
    }else
    {
      // need to go to doubly indirect block
      // printf("ZASOOOO");
      i_indirect_block_idx -= 7;
      // read doubly indirect block to the buffer
      int i_nof_bytes = diskimg_readsector(fs->dfd, inp->i_addr[7], buff);
      if(i_nof_bytes == -1) 
      {
        fprintf(stderr, "inode_indexlookup: Error reading sector %d, returning -1\n", inp->i_addr[7]);
        return -1;  
      }
      i_indirect_block_disc_number = buff[i_indirect_block_idx];
    }
    int i_nof_bytes = diskimg_readsector(fs->dfd, i_indirect_block_disc_number, buff);
    if(i_nof_bytes == -1) 
    {
      fprintf(stderr, "inode_indexlookup: Error reading sector %d, returning -1\n", i_indirect_block_disc_number);
      return -1;  
    }
    int i_relative_block_num_index = blockNum % i_bocks_nums_in_indirect_block; // relative index of block number in singly indirect block
    i_disk_block_number = buff[i_relative_block_num_index];
  }

  return i_disk_block_number;
}

int inode_getsize(struct inode *inp) {
  return ((inp->i_size0 << 16) | inp->i_size1);
}
