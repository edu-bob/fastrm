/**
 * fastrm - a fast rm command.  Experimental
 *
 * Use at your own risk
 *
 * Author:
 *   Robert L. Brown
 */


#define _GNU_SOURCE
#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>

typedef struct {
   long           d_ino;
   off_t          d_off;
   unsigned short d_reclen;
   char           d_name[];
} linux_dirent_t;

#define BUF_SIZE (1024*256)

void dumpRecord(linux_dirent_t *, char *);

unsigned long DeletedFiles = 0;
unsigned long DeletedDirectories = 0;
int o_nogo = 0;
int o_ignore = 0;
int o_older = 0;

int main(int argc, char *argv[])
{
  int c, index;

  while ((c = getopt (argc, argv, "nio:")) != -1)
	switch (c) {
	case 'n':
	  o_nogo = 1;
	  break;
    case 'i':
        o_ignore = 1;
        break;
    case 'o':
        o_older = atoi(optarg);
        break;
	default:
	  fprintf(stderr, "Unknown option: %c\n", c);
	  exit(1);
	}
  if ( optind >= argc ) {
	fprintf(stderr, "ERROR: missing directory name argument\n");
	exit(1);
  }

  if ( chdir(argv[optind]) ) {
	perror(argv[optind]);
	exit(1);
  }

  if ( o_nogo ) {
	printf("INFO: Dry run only.\n");
  }

  // current directory is now the directory whose contents are to be deleted

  process(".", 0);
  fprintf(stderr, "\n");
  fprintf(stderr, "%10ld files deleted\n", DeletedFiles);
  fprintf(stderr, "%10ld directories deleted\n", DeletedDirectories);
  exit(0);
}

int process(char *dirname, int level)
{
  int fd, nread;
  char buf[BUF_SIZE];
  linux_dirent_t *d;
  int bpos;
  char d_type;
  struct stat statbuf;

  if ( o_nogo ) printf("%d: chdir(%s)\n", level, dirname);

  if ( chdir(dirname) ) {
	perror(dirname);
	exit(1);
  }

  if ((fd = open(".", O_RDONLY | O_DIRECTORY)) < 0 ) {
	perror(".");
	exit(1);
  }

  while ( 1 ) {

      // Read a block of directory entries

      if ( !o_nogo ) fprintf(stderr, "r");
      nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);
      if (nread < 0) {
          fprintf(stderr,"SYS_getdents: ");
          perror(dirname);
          exit(1);
      }
      if (nread == 0) {
          fprintf(stderr, "\b");
          break;
      }

      // Process each directory entry
 
     for (bpos = 0; bpos < nread;) {
          d = (linux_dirent_t *) (buf + bpos);
          //	  dumpRecord(d,buf+bpos);
          d_type = *(buf + bpos + d->d_reclen - 1);
          switch ( d_type ) {
          case DT_REG:
          case DT_LNK:

              // process optiona -o (older than)
              if ( o_older ) {
                  if ( stat(d->d_name, &statbuf)) {
                      fprintf(stderr,"\n");
                      perror(d->d_name);
                      if ( o_ignore ) {
                          fprintf(stderr, "Ignoring previous error because of -i option.\n");
                      } else {
                          exit(1);
                      
                      }
                  } else {
                      if ( statbuf.st_mtime+o_older >= time(NULL) ) {
                          if ( o_nogo ) printf("%d: NOT unlink(%s)\n", level, d->d_name);
                          break;
                      }
                  }     
              }
                  

              if ( o_nogo ) {
                  printf("%d: unlink(%s)\n", level,  d->d_name);
              } else {
                  if (unlink(d->d_name) ) {
                      fprintf(stderr,"\n");
                      perror(d->d_name);
                      if ( o_ignore ) {
                          fprintf(stderr, "Ignoring previous error because of -i option.\n");
                      } else {
                          exit(1);
                      }
                  } else {
                      DeletedFiles++;
                      fprintf(stderr, ".");
                  }
              }
              break;
          case DT_DIR:
              if ( strcmp(d->d_name, ".")==0 || strcmp(d->d_name, "..")==0 ) break;
              
              if ( !o_nogo ) fprintf(stderr,"d");
              
              process(d->d_name,level+1);
              if ( !o_nogo ) {
                  if (rmdir(d->d_name) ) {
                      fprintf(stderr,"\n");
                      perror(d->d_name);
                      if ( o_ignore ) {
                          fprintf(stderr, "Ignoring previous error because of -i option.\n");
                      } else {
                          exit(1);
                      }
                  } else {
                      DeletedDirectories++;
                      fprintf(stderr,"u");
                  }
              } else {
                  printf("%d: rmdir(%s)\n", level, d->d_name);
              }
              break;
          default:
              fprintf(stderr, "Unknown directory record type, aborting\n");
              dumpRecord(d,buf+bpos);
              exit(1);
          }
          bpos += d->d_reclen;
      }
  }
  close(fd);
  if ( o_nogo ) printf("%d: chdir(..)\n", level);
  if ( chdir("..") ) {
      perror("..");
      exit(1);
  }
  
}

void dumpRecord(linux_dirent_t *d, char *buf)
{
  int i;
  char d_type;

  for ( i=0 ; i<d->d_reclen ; i++ ) {
	fprintf(stderr," %02x", 0xff&buf[i]);
  }
  fprintf(stderr,"\n");
  fprintf(stderr,"%8ld  ", d->d_ino);
  d_type = *(buf + d->d_reclen - 1);
  fprintf(stderr,"%-10s ", (d_type == DT_REG) ?  "regular" :
		 (d_type == DT_DIR) ?  "directory" :
		 (d_type == DT_FIFO) ? "FIFO" :
		 (d_type == DT_SOCK) ? "socket" :
		 (d_type == DT_LNK) ?  "symlink" :
		 (d_type == DT_BLK) ?  "block dev" :
		 (d_type == DT_CHR) ?  "char dev" : "???");
  fprintf(stderr,"%4d %10lx  %s\n", d->d_reclen,
		 (unsigned long) d->d_off, (char *) d->d_name);
}
