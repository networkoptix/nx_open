 
//#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include <errno.h>

#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

#include <QtCore/QDebug>

#include "dvd_reader.h"
#include "dvd_udf.h"
#include "dvdread_internal.h"

#ifndef EMEDIUMTYPE
#define EMEDIUMTYPE ENOENT
#endif

#if defined(Q_CC_MSVC) 
extern "C"
{
    int static strcasecmp(const char * str1, const char * str2) { return strcmpi(str1, str2); }
    int static strncasecmp(const char * str1, const char * str2, size_t n) { return strnicmp(str1, str2, n); }
}
#endif

typedef struct {
  void *start;
  void *aligned;
} dvdalign_ptrs_t;

typedef struct {
  dvdalign_ptrs_t *ptrs;
  quint32 ptrs_in_use;
  quint32 ptrs_max;
} dvdalign_t;

extern void *GetAlignHandle(dvd_reader_t *device);
extern void SetAlignHandle(dvd_reader_t *device, void *align);

/**
 * Allocates aligned memory (for use with reads from raw/O_DIRECT devices).
 * This memory must be freed with dvdalign_free()
 * The size of the memory that is allocate is num_lbs*2048 bytes.
 * The memory will be suitably aligned for use with
 * block reads from raw/O_DIRECT device.
 * @param num_lbs Number of logical blocks (2048 bytes) to allocate.
 * @return Returns pointer to allocated memory, or NULL on failure
 * This isn't supposed to be fast/efficient, if that is needed
 * this function should be rewritten to use posix_memalign or similar.
 * It's just needed for aligning memory for small block reads from
 * raw/O_DIRECT devices. 
 * We assume that 2048 is enough alignment for all systems at the moment.
 * Not thread safe. Only use this from one thread.
 * Depends on sizeof(unsigned long) being at least as large as sizeof(void *)
 */
static void *dvdalign_lbmalloc(dvd_reader_t *device, quint32 num_lbs)
{
  void *m;
  unsigned n;
  dvdalign_t *a;
  
  m = malloc((num_lbs+1)*DVD_VIDEO_LB_LEN);
  if(m == NULL) {
    return m;
  }
  a = (dvdalign_t *)GetAlignHandle(device);
  if(a == NULL) {
    a = (dvdalign_t*) malloc(sizeof(dvdalign_t));
    if(a == NULL) {
      free(m);
      return a;
    }
    a->ptrs = NULL;
    a->ptrs_in_use = 0;
    a->ptrs_max = 0;
    SetAlignHandle(device, (void *)a);
  }
  
  if(a->ptrs_in_use >= a->ptrs_max) {
    a->ptrs = (dvdalign_ptrs_t*) realloc(a->ptrs, (a->ptrs_max+10)*sizeof(dvdalign_ptrs_t));
    if(a->ptrs == NULL) {
      free(m);
      return NULL;
    }
    a->ptrs_max+=10;
    for(n = a->ptrs_in_use; n < a->ptrs_max; n++) {
      a->ptrs[n].start = NULL;
      a->ptrs[n].aligned = NULL;
    }
    n = a->ptrs_in_use;
  } else {
    for(n = 0; n < a->ptrs_max; n++) {
      if(a->ptrs[n].start == NULL) {
        break;
      }
    }
  }

  a->ptrs[n].start = m;
  a->ptrs[n].aligned = DVD_ALIGN(m);

  a->ptrs_in_use++;

  /* If this function starts to be used too much print a warning.
     Either there is a memory leak somewhere or we need to rewrite this to
     a more efficient version.
  */
  if(a->ptrs_in_use > 50) {
    if(dvdread_verbose(device) >= 0) {
      qWarning() << "libdvdread: dvdalign_lbmalloc(), more allocs than supposed:" << a->ptrs_in_use;
    }
  }

  return  a->ptrs[n].aligned;
}

/**
 * Frees memory allocated with dvdalign_lbmemory() 
 * @param ptr Pointer to memory space to free
 * Not thread safe.
 */
static void dvdalign_lbfree(dvd_reader_t *device, void *ptr)
{
  dvdalign_t *a;

  a = (dvdalign_t *)GetAlignHandle(device);
  if (a && a->ptrs) {
    for (unsigned n = 0; n < a->ptrs_max; n++) {
      if(a->ptrs[n].aligned == ptr) {
        free(a->ptrs[n].start);
        a->ptrs[n].start = NULL;
        a->ptrs[n].aligned = NULL;
        a->ptrs_in_use--;
        if(a->ptrs_in_use == 0) {
          free(a->ptrs);
          a->ptrs = NULL;
          a->ptrs_max = 0;
          free(a);
          a = NULL;
          SetAlignHandle(device, (void *)a);
        }
        return;
      }
    }
  }
  if(dvdread_verbose(device) >= 0) {
        qWarning() << "libdvdread: dvdalign_lbfree(), error trying to free mem";
  }
}


/* Private but located in/shared with dvd_reader.c */
extern int UDFReadBlocksRaw( dvd_reader_t *device, quint32 lb_number,
                             size_t block_count, unsigned char *data, 
                             int encrypted );

/** @internal
 * Its required to either fail or deliver all the blocks asked for. 
 *
 * @param data Pointer to a buffer where data is returned. This must be large
 *   enough to hold lb_number*2048 bytes.
 *   It must be aligned to system specific (2048) logical blocks size when
 *   reading from raw/O_DIRECT device.
 */
static int DVDReadLBUDF( dvd_reader_t *device, quint32 lb_number,
                         size_t block_count, unsigned char *data, 
                         int encrypted )
{
  int ret;
  size_t count = block_count;
  
  while(count > 0) {
    
    ret = UDFReadBlocksRaw(device, lb_number, count, data, encrypted);
        
    if(ret <= 0) {
      /* One of the reads failed or nothing more to read, too bad.
       * We won't even bother returning the reads that went ok. */
      return ret;
    }
    
    count -= (size_t)ret;
    lb_number += (quint32)ret;
  }

  return (int) block_count;
}


#ifndef NULL
#define NULL ((void *)0)
#endif

struct Partition {
  int valid;
  char VolumeDesc[128];
  quint16 Flags;
  quint16 Number;
  char Contents[32];
  quint32 AccessType;
  quint32 Start;
  quint32 Length;
};

struct AD {
  quint32 Location;
  quint32 Length;
  quint8  Flags;
  quint16 Partition;
};

struct extent_ad {
  quint32 location;
  quint32 length;
};

struct avdp_t {
  struct extent_ad mvds;
  struct extent_ad rvds;
};

struct pvd_t {
  quint8 VolumeIdentifier[32];
  quint8 VolumeSetIdentifier[128];
};

struct lbudf {
  quint32 lb;
  quint8 *data;
};

struct icbmap {
  quint32 lbn;
  struct AD file;
  quint8 filetype;
};

struct udf_cache {
  int avdp_valid;
  struct avdp_t avdp;
  int pvd_valid;
  struct pvd_t pvd;
  int partition_valid;
  struct Partition partition;
  int rooticb_valid;
  struct AD rooticb;
  int lb_num;
  struct lbudf *lbs;
  int map_num;
  struct icbmap *maps;
};

typedef enum {
  PartitionCache, RootICBCache, LBUDFCache, MapCache, AVDPCache, PVDCache
} UDFCacheType;

extern void *GetUDFCacheHandle(dvd_reader_t *device);
extern void SetUDFCacheHandle(dvd_reader_t *device, void *cache);


void FreeUDFCache(dvd_reader_t *device, void *cache)
{
  int n;
  
  struct udf_cache *c = (struct udf_cache *)cache;
  if(c == NULL) {
    return;
  }

  for(n = 0; n < c->lb_num; n++) {
    if(c->lbs[n].data) {
      /* free data */
      dvdalign_lbfree(device, c->lbs[n].data);
    }
  }
  c->lb_num = 0;

  if(c->lbs) {
    free(c->lbs);
  }
  if(c->maps) {
    free(c->maps);
  }
  free(c);
}


static int GetUDFCache(dvd_reader_t *device, UDFCacheType type,
                       quint32 nr, void *data)
{
  int n;
  struct udf_cache *c;

  if(DVDUDFCacheLevel(device, -1) <= 0) {
    return 0;
  }
  
  c = (struct udf_cache *)GetUDFCacheHandle(device);
  
  if(c == NULL) {
    return 0;
  }
  
  switch(type) {
  case AVDPCache:
    if(c->avdp_valid) {
      *(struct avdp_t *)data = c->avdp;
      return 1;
    }    
    break;
  case PVDCache:
    if(c->pvd_valid) {
      *(struct pvd_t *)data = c->pvd;
      return 1;
    }    
    break;
  case PartitionCache:
    if(c->partition_valid) {
      *(struct Partition *)data = c->partition;
      return 1;
    }
    break;
  case RootICBCache:
    if(c->rooticb_valid) {
      *(struct AD *)data = c->rooticb;
      return 1;
    }
    break;
  case LBUDFCache:
    for(n = 0; n < c->lb_num; n++) {
      if(c->lbs[n].lb == nr) {
        *(quint8 **)data = c->lbs[n].data;
        return 1;
      }
    }
    break;
  case MapCache:
    for(n = 0; n < c->map_num; n++) {
      if(c->maps[n].lbn == nr) {
        *(struct icbmap *)data = c->maps[n];
        return 1;
      }
    }
    break;
  default:
    break;
  }
  
  return 0;
}

static int SetUDFCache(dvd_reader_t *device, UDFCacheType type,
                       quint32 nr, void *data)
{
  int n;
  struct udf_cache *c;

  if(DVDUDFCacheLevel(device, -1) <= 0) {
    return 0;
  }

  c = (struct udf_cache *)GetUDFCacheHandle(device);
  
  if(c == NULL) {
    c = (udf_cache*) calloc(1, sizeof(struct udf_cache));    
    if(c == NULL) {
      return 0;
    }
    SetUDFCacheHandle(device, c);
  }
  
  
  switch(type) {
  case AVDPCache:
    c->avdp = *(struct avdp_t *)data; 
    c->avdp_valid = 1;
    break;
  case PVDCache:
    c->pvd = *(struct pvd_t *)data; 
    c->pvd_valid = 1;
    break;
  case PartitionCache:
    c->partition = *(struct Partition *)data; 
    c->partition_valid = 1;
    break;
  case RootICBCache:
    c->rooticb = *(struct AD *)data; 
    c->rooticb_valid = 1;
    break;
  case LBUDFCache:
    for(n = 0; n < c->lb_num; n++) {
      if(c->lbs[n].lb == nr) {
        /* replace with new data */
        c->lbs[n].data = *(quint8 **)data;
        c->lbs[n].lb = nr;
        return 1;
      }
    }
    c->lb_num++;
    c->lbs = (lbudf*) realloc(c->lbs, c->lb_num * sizeof(struct lbudf));
    if(c->lbs == NULL) {
      c->lb_num = 0;
      return 0;
    }
    c->lbs[n].data = *(quint8 **)data;
    c->lbs[n].lb = nr;
    break;
  case MapCache:
    for(n = 0; n < c->map_num; n++) {
      if(c->maps[n].lbn == nr) {
        /* replace with new data */
        c->maps[n] = *(struct icbmap *)data;
        c->maps[n].lbn = nr;
        return 1;
      }
    }
    c->map_num++;
    c->maps = (icbmap*) realloc(c->maps, c->map_num * sizeof(struct icbmap));
    if(c->maps == NULL) {
      c->map_num = 0;
      return 0;
    }
    c->maps[n] = *(struct icbmap *)data;
    c->maps[n].lbn = nr;
    break;
  default:
    return 0;
  }
    
  return 1;
}


/* For direct data access, LSB first */
#define GETN1(p) ((quint8)data[p])
#define GETN2(p) ((quint16)data[p] | ((quint16)data[(p) + 1] << 8))
#define GETN3(p) ((quint32)data[p] | ((quint32)data[(p) + 1] << 8)    \
                  | ((quint32)data[(p) + 2] << 16))
#define GETN4(p) ((quint32)data[p]                     \
                  | ((quint32)data[(p) + 1] << 8)      \
                  | ((quint32)data[(p) + 2] << 16)     \
                  | ((quint32)data[(p) + 3] << 24))
/* This is wrong with regard to endianess */
#define GETN(p, n, targetTypes) memcpy(targetTypes, &data[p], n)

static int Unicodedecode( quint8 *data, int len, char *target ) 
{
  int p = 1, i = 0;

  if( ( data[ 0 ] == 8 ) || ( data[ 0 ] == 16 ) ) do {
    if( data[ 0 ] == 16 ) p++;  /* Ignore MSB of unicode16 */
    if( p < len ) {
      target[ i++ ] = data[ p++ ];
    }
  } while( p < len );

  target[ i ] = '\0';
  return 0;
}

static int UDFDescriptor( quint8 *data, quint16 *TagID ) 
{
  *TagID = GETN2(0);
  return 0;
}

static int UDFExtentAD( quint8 *data, quint32 *Length, quint32 *Location ) 
{
  *Length   = GETN4(0);
  *Location = GETN4(4);
  return 0;
}

static int UDFShortAD( quint8 *data, struct AD *ad, 
                       struct Partition *partition ) 
{
  ad->Length = GETN4(0);
  ad->Flags = ad->Length >> 30;
  ad->Length &= 0x3FFFFFFF;
  ad->Location = GETN4(4);
  ad->Partition = partition->Number; // use number of current partition
  return 0;
}

static int UDFLongAD( quint8 *data, struct AD *ad )
{
  ad->Length = GETN4(0);
  ad->Flags = ad->Length >> 30;
  ad->Length &= 0x3FFFFFFF;
  ad->Location = GETN4(4);
  ad->Partition = GETN2(8);
  //GETN(10, 6, Use);
  return 0;
}

static int UDFExtAD( quint8 *data, struct AD *ad )
{
  ad->Length = GETN4(0);
  ad->Flags = ad->Length >> 30;
  ad->Length &= 0x3FFFFFFF;
  ad->Location = GETN4(12);
  ad->Partition = GETN2(16);
  //GETN(10, 6, Use);
  return 0;
}

static int UDFICB( quint8 *data, quint8 *FileType, quint16 *Flags )
{
  *FileType = GETN1(11);
  *Flags = GETN2(18);
  return 0;
}


static int UDFPartition( quint8 *data, quint16 *Flags, quint16 *Number,
                         char *Contents, quint32 *Start, quint32 *Length )
{
  *Flags = GETN2(20);
  *Number = GETN2(22);
  GETN(24, 32, Contents);
  *Start = GETN4(188);
  *Length = GETN4(192);
  return 0;
}

/**
 * Reads the volume descriptor and checks the parameters.  Returns 0 on OK, 1
 * on error.
 */
static int UDFLogVolume( quint8 *data, char *VolumeDescriptor )
{
  quint32 lbsize;
  Unicodedecode(&data[84], 128, VolumeDescriptor);
  lbsize = GETN4(212);  // should be 2048
  /*MT_L =*/ GETN4(264);    // should be 6
  /*N_PM =*/ GETN4(268);    // should be 1
  if (lbsize != DVD_VIDEO_LB_LEN) return 1;
  return 0;
}

static int UDFFileEntry( quint8 *data, quint8 *FileType, 
                         struct Partition *partition, struct AD *ad )
{
  quint16 flags;
  quint32 L_EA, L_AD;
  unsigned int p;

  UDFICB( &data[ 16 ], FileType, &flags );
   
  /* Init ad for an empty file (i.e. there isn't a AD, L_AD == 0 ) */
  ad->Length = GETN4( 60 ); // Really 8 bytes a 56
  ad->Flags = 0;
  ad->Location = 0; // what should we put here? 
  ad->Partition = partition->Number; // use number of current partition

  L_EA = GETN4( 168 );
  L_AD = GETN4( 172 );
  p = 176 + L_EA;
  while( p < 176 + L_EA + L_AD ) {
    switch( flags & 0x0007 ) {
    case 0: UDFShortAD( &data[ p ], ad, partition ); p += 8;  break;
    case 1: UDFLongAD( &data[ p ], ad );  p += 16; break;
    case 2: UDFExtAD( &data[ p ], ad );   p += 20; break;
    case 3:
      switch( L_AD ) {
      case 8:  UDFShortAD( &data[ p ], ad, partition ); break;
      case 16: UDFLongAD( &data[ p ], ad );  break;
      case 20: UDFExtAD( &data[ p ], ad );   break;
      }
      p += L_AD;
      break;
    default:
      p += L_AD; break;
    }
  }
  return 0;
}

static int UDFFileIdentifier( quint8 *data, quint8 *FileCharacteristics,
                              char *FileName, struct AD *FileICB )
{
  quint8 L_FI;
  quint16 L_IU;

  *FileCharacteristics = GETN1(18);
  L_FI = GETN1(19);
  UDFLongAD(&data[20], FileICB);
  L_IU = GETN2(36);
  if (L_FI) Unicodedecode(&data[38 + L_IU], L_FI, FileName);
  else FileName[0] = '\0';
  return 4 * ((38 + L_FI + L_IU + 3) / 4);
}

/**
 * Maps ICB to FileAD
 * ICB: Location of ICB of directory to scan
 * FileType: Type of the file
 * File: Location of file the ICB is pointing to
 * return 1 on success, 0 on error;
 */
static int UDFMapICB( dvd_reader_t *device, struct AD ICB, quint8 *FileType,
                      struct Partition *partition, struct AD *File ) 
{
  quint8 *LogBlock;
  quint32 lbnum;
  quint16 TagID;
  struct icbmap tmpmap;

  lbnum = partition->Start + ICB.Location;
  tmpmap.lbn = lbnum;
  if(GetUDFCache(device, MapCache, lbnum, &tmpmap)) {
    *FileType = tmpmap.filetype;
    *File = tmpmap.file;
    return 1;
  }

  LogBlock = (quint8*) dvdalign_lbmalloc(device, 1);
  if(!LogBlock) {
    return 0;
  }
    
  do {
    if( DVDReadLBUDF( device, lbnum++, 1, LogBlock, 0 ) <= 0 ) {
      TagID = 0;
    } else {
      UDFDescriptor( LogBlock, &TagID );
    }

    if( TagID == 261 ) {
      UDFFileEntry( LogBlock, FileType, partition, File );
      tmpmap.file = *File;
      tmpmap.filetype = *FileType;
      SetUDFCache(device, MapCache, tmpmap.lbn, &tmpmap);
      dvdalign_lbfree(device, LogBlock);
      return 1;
    };
  } while( ( lbnum <= partition->Start + ICB.Location + ( ICB.Length - 1 )
             / DVD_VIDEO_LB_LEN ) && ( TagID != 261 ) );

  dvdalign_lbfree(device, LogBlock);
  return 0;
}

/**
 * Dir: Location of directory to scan
 * FileName: Name of file to look for
 * FileICB: Location of ICB of the found file
 * return 1 on success, 0 on error;
 */
static int UDFScanDir( dvd_reader_t *device, struct AD Dir, char *FileName,
                       struct Partition *partition, struct AD *FileICB,
                       int cache_file_info) 
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  quint8 *directory;
  quint32 lbnum;
  quint16 TagID;
  quint8 filechar;
  unsigned int p;
  quint8 *cached_dir = NULL;
  quint32 dir_lba;
  struct AD tmpICB;
  int found = 0;
  int in_cache = 0;

  /* Scan dir for ICB of file */
  lbnum = partition->Start + Dir.Location;
    
  if(DVDUDFCacheLevel(device, -1) > 0) {
    /* caching */
      
    if(!GetUDFCache(device, LBUDFCache, lbnum, &cached_dir)) {
      dir_lba = (Dir.Length + DVD_VIDEO_LB_LEN) / DVD_VIDEO_LB_LEN;
      if((cached_dir = (quint8*) dvdalign_lbmalloc(device, dir_lba)) == NULL) {
        return 0;
      }
      if( DVDReadLBUDF( device, lbnum, dir_lba, cached_dir, 0) <= 0 ) {
        dvdalign_lbfree(device, cached_dir);
        cached_dir = NULL;
      }
      SetUDFCache(device, LBUDFCache, lbnum, &cached_dir);
    } else {
      in_cache = 1;
    }
      
    if(cached_dir == NULL) {
      return 0;
    }
      
    p = 0;
      
    while( p < Dir.Length ) {
      UDFDescriptor( &cached_dir[ p ], &TagID );
      if( TagID == 257 ) {
        p += UDFFileIdentifier( &cached_dir[ p ], &filechar,
                                filename, &tmpICB );
        if(cache_file_info && !in_cache) {
          quint8 tmpFiletype;
          struct AD tmpFile;
            
          if( !strcasecmp( FileName, filename ) ) {
            *FileICB = tmpICB;
            found = 1;
              
          }
          UDFMapICB(device, tmpICB, &tmpFiletype,
                    partition, &tmpFile);
        } else {
          if( !strcasecmp( FileName, filename ) ) {
            *FileICB = tmpICB;
            return 1;
          }
        }
      } else {
        if(cache_file_info && (!in_cache) && found) {
          return 1;
        }
        return 0;
      }
    }
    if(cache_file_info && (!in_cache) && found) {
      return 1;
    }
    return 0;
  }

  directory = (quint8*) dvdalign_lbmalloc(device, 2);
  if(!directory) {
    return 0;
  }
  if( DVDReadLBUDF( device, lbnum, 2, directory, 0 ) <= 0 ) {
    dvdalign_lbfree(device, directory);
    return 0;
  }

  p = 0;
  while( p < Dir.Length ) {
    if( p > DVD_VIDEO_LB_LEN ) {
      ++lbnum;
      p -= DVD_VIDEO_LB_LEN;
      Dir.Length -= DVD_VIDEO_LB_LEN;
      if( DVDReadLBUDF( device, lbnum, 2, directory, 0 ) <= 0 ) {
        dvdalign_lbfree(device, directory);
        return 0;
      }
    }
    UDFDescriptor( &directory[ p ], &TagID );
    if( TagID == 257 ) {
      p += UDFFileIdentifier( &directory[ p ], &filechar,
                              filename, FileICB );
      if( !strcasecmp( FileName, filename ) ) {
        dvdalign_lbfree(device, directory);
        return 1;
      }
    } else {
      dvdalign_lbfree(device, directory);
      return 0;
    }
  }

  dvdalign_lbfree(device, directory);
  return 0;
}


static int UDFGetAVDP( dvd_reader_t *device,
                       struct avdp_t *avdp)
{
  quint8 *Anchor;
  quint32 lbnum, MVDS_location, MVDS_length;
  quint16 TagID;
  quint32 lastsector;
  int terminate;
  struct avdp_t; 
  
  if(GetUDFCache(device, AVDPCache, 0, avdp)) {
    return 1;
  }
  
  /* Find Anchor */
  lastsector = 0;
  lbnum = 256;   /* Try #1, prime anchor */
  terminate = 0;
  
  Anchor = (quint8*) dvdalign_lbmalloc(device, 1);
  if(!Anchor) {
    return 0;
  }
  for(;;) {
    if( DVDReadLBUDF( device, lbnum, 1, Anchor, 0 ) > 0 ) {
      UDFDescriptor( Anchor, &TagID );
    } else {
      TagID = 0;
    }
    if (TagID != 2) {
      /* Not an anchor */
      if( terminate ) {
        dvdalign_lbfree(device, Anchor);
        errno = EMEDIUMTYPE;
        return 0; /* Final try failed */
      } 
      
      if( lastsector ) {
        /* We already found the last sector.  Try #3, alternative
         * backup anchor.  If that fails, don't try again.
         */
        lbnum = lastsector;
        terminate = 1;
      } else {
        if( lastsector ) {
          /* Try #2, backup anchor */
          lbnum = lastsector - 256;
        } else {
          /* Unable to find last sector */
          dvdalign_lbfree(device, Anchor);
          errno = EMEDIUMTYPE;
          return 0;
        }
      }
    } else {
      /* It's an anchor! We can leave */
      break;
    }
  }
  /* Main volume descriptor */
  UDFExtentAD( &Anchor[ 16 ], &MVDS_length, &MVDS_location );
  avdp->mvds.location = MVDS_location;
  avdp->mvds.length = MVDS_length;
  
  /* Backup volume descriptor */
  UDFExtentAD( &Anchor[ 24 ], &MVDS_length, &MVDS_location );
  avdp->rvds.location = MVDS_location;
  avdp->rvds.length = MVDS_length;
  
  SetUDFCache(device, AVDPCache, 0, avdp);
  
  dvdalign_lbfree(device, Anchor);
  return 1;
}

/**
 * Looks for partition on the disc.  Returns 1 if partition found, 0 on error.
 *   partnum: Number of the partition, starting at 0.
 *   part: structure to fill with the partition information
 */
static int UDFFindPartition( dvd_reader_t *device, int partnum,
                             struct Partition *part ) 
{
  quint8 *LogBlock;
  quint32 lbnum, MVDS_location, MVDS_length;
  quint16 TagID;
  int i, volvalid;
  struct avdp_t avdp;

    
  if(!UDFGetAVDP(device, &avdp)) {
    return 0;
  }

  LogBlock = (quint8*) dvdalign_lbmalloc(device, 1);
  if(!LogBlock) {
    return 0;
  }
  /* Main volume descriptor */
  MVDS_location = avdp.mvds.location;
  MVDS_length = avdp.mvds.length;

  part->valid = 0;
  volvalid = 0;
  part->VolumeDesc[ 0 ] = '\0';
  i = 1;
  do {
    /* Find Volume Descriptor */
    lbnum = MVDS_location;
    do {

      if( DVDReadLBUDF( device, lbnum++, 1, LogBlock, 0 ) <= 0 ) {
        TagID = 0;
      } else {
        UDFDescriptor( LogBlock, &TagID );
      }

      if( ( TagID == 5 ) && ( !part->valid ) ) {
        /* Partition Descriptor */
        UDFPartition( LogBlock, &part->Flags, &part->Number,
                      part->Contents, &part->Start, &part->Length );
        part->valid = ( partnum == part->Number );
      } else if( ( TagID == 6 ) && ( !volvalid ) ) {
        /* Logical Volume Descriptor */
        if( UDFLogVolume( LogBlock, part->VolumeDesc ) ) {  
        } else {
          volvalid = 1;
        }
      }

    } while( ( lbnum <= MVDS_location + ( MVDS_length - 1 )
               / DVD_VIDEO_LB_LEN ) && ( TagID != 8 )
             && ( ( !part->valid ) || ( !volvalid ) ) );

    if( ( !part->valid) || ( !volvalid ) ) {
      /* Backup volume descriptor */
      MVDS_location = avdp.mvds.location;
      MVDS_length = avdp.mvds.length;
    }
  } while( i-- && ( ( !part->valid ) || ( !volvalid ) ) );

  dvdalign_lbfree(device, LogBlock);
  /* We only care for the partition, not the volume */
  return part->valid;
}

quint32 UDFFindFile( dvd_reader_t *device, const char *filename,
                      quint32 *filesize )
{
  quint8 *LogBlock;
  quint32 lbnum;
  quint16 TagID;
  struct Partition partition;
  struct AD RootICB, File, ICB;
  char tokenline[ MAX_UDF_FILE_NAME_LEN ];
  char *token;
  quint8 filetype;
  
  if(filesize) {
    *filesize = 0;
  }
  tokenline[0] = '\0';
  strcat( tokenline, filename );

    
  if(!(GetUDFCache(device, PartitionCache, 0, &partition) &&
       GetUDFCache(device, RootICBCache, 0, &RootICB))) {
    /* Find partition, 0 is the standard location for DVD Video.*/
    if( !UDFFindPartition( device, 0, &partition ) ) {
      return 0;
    }
    SetUDFCache(device, PartitionCache, 0, &partition);
    
    LogBlock = (quint8*) dvdalign_lbmalloc(device, 1);
    if(!LogBlock) {
      return 0;
    }
    /* Find root dir ICB */
    lbnum = partition.Start;
    do {
      if( DVDReadLBUDF( device, lbnum++, 1, LogBlock, 0 ) <= 0 ) {
        TagID = 0;
      } else {
        UDFDescriptor( LogBlock, &TagID );
      }

      /* File Set Descriptor */
      if( TagID == 256 ) {  // File Set Descriptor
        UDFLongAD( &LogBlock[ 400 ], &RootICB );
      }
    } while( ( lbnum < partition.Start + partition.Length )
             && ( TagID != 8 ) && ( TagID != 256 ) );

    dvdalign_lbfree(device, LogBlock);
      
    /* Sanity checks. */
    if( TagID != 256 ) {
      return 0;
    }
    if( RootICB.Partition != 0 ) {
      return 0;
    }
    SetUDFCache(device, RootICBCache, 0, &RootICB);
  }

  /* Find root dir */
  if( !UDFMapICB( device, RootICB, &filetype, &partition, &File ) ) {
    return 0;
  }
  if( filetype != 4 ) {
    return 0;  /* Root dir should be dir */
  }
  {
    int cache_file_info = 0;
    /* Tokenize filepath */
    token = strtok(tokenline, "/");
    
    while( token != NULL ) {
      
      if( !UDFScanDir( device, File, token, &partition, &ICB,
                       cache_file_info)) {
        return 0;
      }
      if( !UDFMapICB( device, ICB, &filetype, &partition, &File ) ) {
        return 0;
      }
      if(!strcmp(token, "VIDEO_TS")) {
        cache_file_info = 1;
      }
      token = strtok( NULL, "/" );
    }
  } 

  /* Sanity check. */
  if( File.Partition != 0 ) {
    return 0;
  }

  if(filesize) {
    *filesize = File.Length;
  }
  /* Hack to not return partition.Start for empty files. */
  if( !File.Location ) {
    return 0;
  } else {
    return partition.Start + File.Location;
  }
}



/**
 * Gets a Descriptor .
 * Returns 1 if descriptor found, 0 on error.
 * id, tagid of descriptor
 * bufsize, size of BlockBuf (must be >= DVD_VIDEO_LB_LEN)
 * and aligned for raw/O_DIRECT read.
 */
static int UDFGetDescriptor( dvd_reader_t *device, int id,
                             quint8 *descriptor, int bufsize) 
{
  quint32 lbnum, MVDS_location, MVDS_length;
  struct avdp_t avdp;
  quint16 TagID;
  int i;
  int desc_found = 0;
  /* Find Anchor */
  lbnum = 256;   /* Try #1, prime anchor */
  if(bufsize < DVD_VIDEO_LB_LEN) {
    return 0;
  }
  
  if(!UDFGetAVDP(device, &avdp)) {
    return 0;
  }

  /* Main volume descriptor */
  MVDS_location = avdp.mvds.location;
  MVDS_length = avdp.mvds.length;
  
  i = 1;
  do {
    /* Find  Descriptor */
    lbnum = MVDS_location;
    do {
      
      if( DVDReadLBUDF( device, lbnum++, 1, descriptor, 0 ) <= 0 ) {
        TagID = 0;
      } else {
        UDFDescriptor( descriptor, &TagID );
      }
      
      if( (TagID == id) && ( !desc_found ) ) {
        /* Descriptor */
        desc_found = 1;
      }
    } while( ( lbnum <= MVDS_location + ( MVDS_length - 1 )
               / DVD_VIDEO_LB_LEN ) && ( TagID != 8 )
             && ( !desc_found) );
    
    if( !desc_found ) {
      /* Backup volume descriptor */
      MVDS_location = avdp.rvds.location;
      MVDS_length = avdp.rvds.length;
    }
  } while( i-- && ( !desc_found )  );

  
  return desc_found;
}


static int UDFGetPVD(dvd_reader_t *device, struct pvd_t *pvd)
{
  quint8 *pvd_buf;
  
  if(GetUDFCache(device, PVDCache, 0, pvd)) {
    return 1;
  }
  
  pvd_buf = (quint8*) dvdalign_lbmalloc(device, 1);
  if(!pvd_buf) {
    return 0;
  }
  if(!UDFGetDescriptor( device, 1, pvd_buf, 1*DVD_VIDEO_LB_LEN)) {
    dvdalign_lbfree(device, pvd_buf);
    return 0;
  }
  
  memcpy(pvd->VolumeIdentifier, &pvd_buf[24], 32);
  memcpy(pvd->VolumeSetIdentifier, &pvd_buf[72], 128);
  SetUDFCache(device, PVDCache, 0, pvd);
  
  dvdalign_lbfree(device, pvd_buf);

  return 1;
}

/**
 * Gets the Volume Identifier string, in 8bit unicode (latin-1)
 * volid, place to put the string
 * volid_size, size of the buffer volid points to
 * returns the size of buffer needed for all data
 */
int UDFGetVolumeIdentifier(dvd_reader_t *device, char *volid,
                           unsigned int volid_size)
{
  struct pvd_t pvd;
  unsigned int volid_len;

  /* get primary volume descriptor */
  if(!UDFGetPVD(device, &pvd)) {
    return 0;
  }

  volid_len = pvd.VolumeIdentifier[31];
  if(volid_len > 31) {
    /* this field is only 32 bytes something is wrong */
    volid_len = 31;
  }
  if(volid_size > volid_len) {
    volid_size = volid_len;
  }
  Unicodedecode(pvd.VolumeIdentifier, volid_size, volid);
  
  return volid_len;
}

/**
 * Gets the Volume Set Identifier, as a 128-byte dstring (not decoded)
 * WARNING This is not a null terminated string
 * volsetid, place to put the data
 * volsetid_size, size of the buffer volsetid points to 
 * the buffer should be >=128 bytes to store the whole volumesetidentifier
 * returns the size of the available volsetid information (128)
 * or 0 on error
 */
int UDFGetVolumeSetIdentifier(dvd_reader_t *device, quint8 *volsetid,
                              unsigned int volsetid_size)
{
  struct pvd_t pvd;

  /* get primary volume descriptor */
  if(!UDFGetPVD(device, &pvd)) {
    return 0;
  }


  if(volsetid_size > 128) {
    volsetid_size = 128;
  }
  
  memcpy(volsetid, pvd.VolumeSetIdentifier, volsetid_size);
  
  return 128;
}
