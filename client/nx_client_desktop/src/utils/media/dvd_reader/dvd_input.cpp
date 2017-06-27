
//#include "config.h"

#include <stdio.h>
#include <stdlib.h>
//#define __USE_GNU /* to get O_DIRECT in linux */ //already defined in QT
#include <fcntl.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "dvd_reader.h"
#include "dvd_input.h"

#include "dvdread_internal.h"
#include "../dvd_decrypt/dvdcss.h"


/* The DVDinput handle, add stuff here for new input methods. */
struct dvd_input_s {
    /* libdvdcss handle */
    dvdcss_handle dvdcss;

    /* dummy file input */
    int fd;
};


/* The function pointers that is the exported interface of this file. */
dvd_input_t (*dvdinput_open)  (const char *);
int         (*dvdinput_close) (dvd_input_t);
int         (*dvdinput_seek)  (dvd_input_t, int);
int         (*dvdinput_title) (dvd_input_t, int); 
/**
 *  pointer must be aligned to 2048 bytes
 *  if reading from a raw/O_DIRECT file
 */
int         (*dvdinput_read)  (dvd_input_t, void *, int, int);

char *      (*dvdinput_error) (dvd_input_t);

/* linking to libdvdcss */

#define DVDcss_open(a) dvdcss_open((char*)(a))
#define DVDcss_close   dvdcss_close
#define DVDcss_seek    dvdcss_seek
#define DVDcss_title   dvdcss_title
#define DVDcss_read    dvdcss_read
#define DVDcss_error   dvdcss_error


/**
 * initialize and open a DVD device or file.
 */
static dvd_input_t css_open(const char *target)
{
  dvd_input_t dev;
    
  /* Allocate the handle structure */
  dev = (dvd_input_t) malloc(sizeof(struct dvd_input_s));
  if(dev == NULL) {
    /* malloc has set errno to ENOMEM */
    return NULL;
  }
  
  /* Really open it with libdvdcss */
  dev->dvdcss = DVDcss_open(target);
  if(dev->dvdcss == 0) {
    free(dev);
    dev = NULL;
  }
  
  return dev;
}

/**
 * return the last error message
 */
static char *css_error(dvd_input_t dev)
{
  return DVDcss_error(dev->dvdcss);
}

/**
 * seek into the device.
 */
static int css_seek(dvd_input_t dev, int blocks)
{
  /* DVDINPUT_NOFLAGS should match the DVDCSS_NOFLAGS value. */
  return DVDcss_seek(dev->dvdcss, blocks, DVDINPUT_NOFLAGS);
  //return DVDcss_seek(dev->dvdcss, blocks, DVDCSS_SEEK_KEY);
}

/**
 * set the block for the begining of a new title (key).
 */
static int css_title(dvd_input_t dev, int block)
{
  return DVDcss_title(dev->dvdcss, block);
}

/**
 * read data from the device.
 */
static int css_read(dvd_input_t dev, void *buffer, int blocks, int flags)
{
  return DVDcss_read(dev->dvdcss, buffer, blocks, flags);
}

/**
 * close the DVD device and clean up the library.
 */
static int css_close(dvd_input_t dev)
{
  int ret;

  ret = DVDcss_close(dev->dvdcss);

  if(ret < 0)
    return ret;

  free(dev);

  return 0;
}

/* Need to use O_BINARY for WIN32 */
#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0
#endif
#endif

static void *dvdcss_library = NULL;
static int dvdcss_library_init = 0;

/**
 * Free any objects allocated by dvdinput_setup.
 * Should only be called when libdvdread is not to be used any more.
 * Closes dlopened libraries.
 */
void dvdinput_free(void)
{
  /* linked statically, nothing to free */
  return;
}


/**
 * Setup read functions with either libdvdcss or minimal DVD access.
 */
int dvdinput_setup(void)
{
  char **dvdcss_version = NULL;
  int verbose;

  /* dlopening libdvdcss */
  if(dvdcss_library_init) {
    /* libdvdcss is already dlopened, function ptrs set */
    if(dvdcss_library) {
      return 1; /* css available */
    } else {
      return 0; /* css not available */
    }
  }

  verbose = get_verbose();
  
  /* linking to libdvdcss */
  dvdcss_library = &dvdcss_library;  /* Give it some value != NULL */
  /* the DVDcss_* functions have been #defined at the top */
  dvdcss_version = &dvdcss_interface_2;


  dvdcss_library_init = 1;
  
  if(dvdcss_library) 
  {
    /*
      char *psz_method = getenv( "DVDCSS_METHOD" );
      char *psz_verbose = getenv( "DVDCSS_VERBOSE" );
      fprintf(stderr, "DVDCSS_METHOD %s\n", psz_method);
      fprintf(stderr, "DVDCSS_VERBOSE %s\n", psz_verbose);
    */
    if(verbose >= 1) {
      fprintf(stderr, "libdvdread: Using libdvdcss version %s for DVD access\n",
              *dvdcss_version);
    }
    /* libdvdcss wrapper functions */
    dvdinput_open  = css_open;
    dvdinput_close = css_close;
    dvdinput_seek  = css_seek;
    dvdinput_title = css_title;
    dvdinput_read  = css_read;
    dvdinput_error = css_error;
    return 1;
    
  } else {
    if(verbose >= 1) {
      fprintf(stderr, "libdvdread: Encrypted DVD support unavailable.\n");
    }
    /* libdvdcss replacement functions */
    /*
    dvdinput_open  = file_open;
    dvdinput_close = file_close;
    dvdinput_seek  = file_seek;
    dvdinput_title = file_title;
    dvdinput_read  = file_read;
    dvdinput_error = file_error;
    */
    return 0;
  }
}
