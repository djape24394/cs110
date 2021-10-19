#include "pathname.h"
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

int pathname_lookup(struct unixfilesystem *fs, const char *pathname) {
  struct direntv6 dirEnt;
  int inumber = ROOT_INUMBER;
  char *string,*found;

  if(pathname[0] == '/') string = strdup(&pathname[1]);
  else string = strdup(pathname);

  char *for_free = string;
  while( (found = strsep(&string,"/")) != NULL && found[0] != '\0')
  {
    if(directory_findname(fs, found, inumber, &dirEnt) < 0) return -1;
    else inumber = dirEnt.d_inumber;
  }
  free(for_free);

  return inumber;
}
