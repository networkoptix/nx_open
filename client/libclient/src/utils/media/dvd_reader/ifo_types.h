/* -*- c-basic-offset: 2; indent-tabs-mode: nil -*- */
#ifndef IFO_TYPES_H_INCLUDED
#define IFO_TYPES_H_INCLUDED

/*
 * Copyright (C) 2000, 2001 Björn Englund <d4bjorn@dtek.chalmers.se>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "dvd_reader.h"


#undef ATTRIBUTE_PACKED
#undef PRAGMA_PACK_BEGIN 
#undef PRAGMA_PACK_END

#if defined(__GNUC__)
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define ATTRIBUTE_PACKED __attribute__ ((packed))
#define PRAGMA_PACK 0
#endif
#endif

#if !defined(ATTRIBUTE_PACKED)
#define ATTRIBUTE_PACKED
#define PRAGMA_PACK 1
#endif

#if PRAGMA_PACK
#pragma pack(1)
#endif


/**
 * Common
 *
 * The following structures are used in both the VMGI and VTSI.
 */


/**
 * DVD Time Information.
 */
typedef struct {
  quint8 hour;
  quint8 minute;
  quint8 second;
  quint8 frame_u; /* The two high bits are the frame rate. */
} ATTRIBUTE_PACKED dvd_time_t;

/**
 * Type to store per-command data.
 */
typedef struct {
  quint8 bytes[8];
} ATTRIBUTE_PACKED vm_cmd_t;
#define COMMAND_DATA_SIZE 8U


/**
 * Video Attributes.
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned char mpeg_version         : 2;
  unsigned char video_format         : 2;
  unsigned char display_aspect_ratio : 2;
  unsigned char permitted_df         : 2;
  
  unsigned char line21_cc_1          : 1;
  unsigned char line21_cc_2          : 1;
  unsigned char unknown1             : 1;
  unsigned char bit_rate             : 1;
  
  unsigned char picture_size         : 2;
  unsigned char letterboxed          : 1;
  unsigned char film_mode            : 1;
#else
  unsigned char permitted_df         : 2;
  unsigned char display_aspect_ratio : 2;
  unsigned char video_format         : 2;
  unsigned char mpeg_version         : 2;
  
  unsigned char film_mode            : 1;
  unsigned char letterboxed          : 1;
  unsigned char picture_size         : 2;
  
  unsigned char bit_rate             : 1;
  unsigned char unknown1             : 1;
  unsigned char line21_cc_2          : 1;
  unsigned char line21_cc_1          : 1;
#endif
} ATTRIBUTE_PACKED video_attr_t;

/**
 * Audio Attributes.
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned char audio_format           : 3;
  unsigned char multichannel_extension : 1;
  unsigned char lang_type              : 2;
  unsigned char application_mode       : 2;
  
  unsigned char quantization           : 2;
  unsigned char sample_frequency       : 2;
  unsigned char unknown1               : 1;
  unsigned char channels               : 3;
#else
  unsigned char application_mode       : 2;
  unsigned char lang_type              : 2;
  unsigned char multichannel_extension : 1;
  unsigned char audio_format           : 3;
  
  unsigned char channels               : 3;
  unsigned char unknown1               : 1;
  unsigned char sample_frequency       : 2;
  unsigned char quantization           : 2;
#endif
  quint16 lang_code;
  quint8  lang_extension;
  quint8  code_extension;
  quint8 unknown3;
  union {
    struct {
#ifdef WORDS_BIGENDIAN
      unsigned char unknown4           : 1;
      unsigned char channel_assignment : 3;
      unsigned char version            : 2;
      unsigned char mc_intro           : 1; /* probably 0: true, 1:false */
      unsigned char mode               : 1; /* Karaoke mode 0: solo 1: duet */
#else
      unsigned char mode               : 1;
      unsigned char mc_intro           : 1;
      unsigned char version            : 2;
      unsigned char channel_assignment : 3;
      unsigned char unknown4           : 1;
#endif
    } ATTRIBUTE_PACKED karaoke;
    struct {
#ifdef WORDS_BIGENDIAN
      unsigned char unknown5           : 4;
      unsigned char dolby_encoded      : 1; /* suitable for surround decoding */
      unsigned char unknown6           : 3;
#else
      unsigned char unknown6           : 3;
      unsigned char dolby_encoded      : 1;
      unsigned char unknown5           : 4;
#endif
    } ATTRIBUTE_PACKED surround;
  } ATTRIBUTE_PACKED app_info;
} ATTRIBUTE_PACKED audio_attr_t;


/**
 * MultiChannel Extension
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned char zero1      : 7;
  unsigned char ach0_gme   : 1;

  unsigned char zero2      : 7;
  unsigned char ach1_gme   : 1;

  unsigned char zero3      : 4;
  unsigned char ach2_gv1e  : 1;
  unsigned char ach2_gv2e  : 1;
  unsigned char ach2_gm1e  : 1;
  unsigned char ach2_gm2e  : 1;

  unsigned char zero4      : 4;
  unsigned char ach3_gv1e  : 1;
  unsigned char ach3_gv2e  : 1;
  unsigned char ach3_gmAe  : 1;
  unsigned char ach3_se2e  : 1;

  unsigned char zero5      : 4;
  unsigned char ach4_gv1e  : 1;
  unsigned char ach4_gv2e  : 1;
  unsigned char ach4_gmBe  : 1;
  unsigned char ach4_seBe  : 1;
#else
  unsigned char ach0_gme   : 1;
  unsigned char zero1      : 7;

  unsigned char ach1_gme   : 1;
  unsigned char zero2      : 7;

  unsigned char ach2_gm2e  : 1;
  unsigned char ach2_gm1e  : 1;
  unsigned char ach2_gv2e  : 1;
  unsigned char ach2_gv1e  : 1;
  unsigned char zero3      : 4;

  unsigned char ach3_se2e  : 1;
  unsigned char ach3_gmAe  : 1;
  unsigned char ach3_gv2e  : 1;
  unsigned char ach3_gv1e  : 1;
  unsigned char zero4      : 4;

  unsigned char ach4_seBe  : 1;
  unsigned char ach4_gmBe  : 1;
  unsigned char ach4_gv2e  : 1;
  unsigned char ach4_gv1e  : 1;
  unsigned char zero5      : 4;
#endif
  quint8 zero6[19];
} ATTRIBUTE_PACKED multichannel_ext_t;


/**
 * Subpicture Attributes.
 */
typedef struct {
  /*
   * type: 0 not specified
   *       1 language
   *       2 other
   * coding mode: 0 run length
   *              1 extended
   *              2 other
   * language: indicates language if type == 1
   * lang extension: if type == 1 contains the lang extension
   */
#ifdef WORDS_BIGENDIAN
  unsigned char code_mode : 3;
  unsigned char zero1     : 3;
  unsigned char type      : 2;
#else
  unsigned char type      : 2;
  unsigned char zero1     : 3;
  unsigned char code_mode : 3;
#endif
  quint8  zero2;
  quint16 lang_code;
  quint8  lang_extension;
  quint8  code_extension;
} ATTRIBUTE_PACKED subp_attr_t;



/**
 * PGC Command Table.
 */ 
typedef struct {
  quint16 nr_of_pre;
  quint16 nr_of_post;
  quint16 nr_of_cell;
  quint16 last_byte;
  vm_cmd_t *pre_cmds;
  vm_cmd_t *post_cmds;
  vm_cmd_t *cell_cmds;
} ATTRIBUTE_PACKED pgc_command_tbl_t;
#define PGC_COMMAND_TBL_SIZE 8U

/**
 * PGC Program Map
 */
typedef quint8 pgc_program_map_t; 

/**
 * Cell Playback Information.
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned char block_mode       : 2;
  unsigned char block_type       : 2;
  unsigned char seamless_play    : 1;
  unsigned char interleaved      : 1;
  unsigned char stc_discontinuity: 1;
  unsigned char seamless_angle   : 1;
  
  unsigned char playback_mode    : 1;  /**< When set, enter StillMode after each VOBU */
  unsigned char restricted       : 1;  /**< ?? drop out of fastforward? */
  unsigned char unknown2         : 6;
#else
  unsigned char seamless_angle   : 1;
  unsigned char stc_discontinuity: 1;
  unsigned char interleaved      : 1;
  unsigned char seamless_play    : 1;
  unsigned char block_type       : 2;
  unsigned char block_mode       : 2;
  
  unsigned char unknown2         : 6;
  unsigned char restricted       : 1;
  unsigned char playback_mode    : 1;
#endif
  quint8 still_time;
  quint8 cell_cmd_nr;
  dvd_time_t playback_time;
  quint32 first_sector;
  quint32 first_ilvu_end_sector;
  quint32 last_vobu_start_sector;
  quint32 last_sector;
} ATTRIBUTE_PACKED cell_playback_t;

#define BLOCK_TYPE_NONE         0x0
#define BLOCK_TYPE_ANGLE_BLOCK  0x1

#define BLOCK_MODE_NOT_IN_BLOCK 0x0
#define BLOCK_MODE_FIRST_CELL   0x1
#define BLOCK_MODE_IN_BLOCK     0x2
#define BLOCK_MODE_LAST_CELL    0x3

/**
 * Cell Position Information.
 */
typedef struct {
  quint16 vob_id_nr;
  quint8  zero_1;
  quint8  cell_nr;
} ATTRIBUTE_PACKED cell_position_t;

/**
 * User Operations.
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned char zero                           : 7; /* 25-31 */
  unsigned char video_pres_mode_change         : 1; /* 24 */
  
  unsigned char karaoke_audio_pres_mode_change : 1; /* 23 */
  unsigned char angle_change                   : 1;
  unsigned char subpic_stream_change           : 1;
  unsigned char audio_stream_change            : 1;
  unsigned char pause_on                       : 1;
  unsigned char still_off                      : 1;
  unsigned char button_select_or_activate      : 1;
  unsigned char resume                         : 1; /* 16 */
  
  unsigned char chapter_menu_call              : 1; /* 15 */
  unsigned char angle_menu_call                : 1;
  unsigned char audio_menu_call                : 1;
  unsigned char subpic_menu_call               : 1;
  unsigned char root_menu_call                 : 1;
  unsigned char title_menu_call                : 1;
  unsigned char backward_scan                  : 1;
  unsigned char forward_scan                   : 1; /* 8 */
  
  unsigned char next_pg_search                 : 1; /* 7 */
  unsigned char prev_or_top_pg_search          : 1;
  unsigned char time_or_chapter_search         : 1;
  unsigned char go_up                          : 1;
  unsigned char stop                           : 1;
  unsigned char title_play                     : 1;
  unsigned char chapter_search_or_play         : 1;
  unsigned char title_or_time_play             : 1; /* 0 */
#else
  unsigned char video_pres_mode_change         : 1; /* 24 */
  unsigned char zero                           : 7; /* 25-31 */
  
  unsigned char resume                         : 1; /* 16 */
  unsigned char button_select_or_activate      : 1;
  unsigned char still_off                      : 1;
  unsigned char pause_on                       : 1;
  unsigned char audio_stream_change            : 1;
  unsigned char subpic_stream_change           : 1;
  unsigned char angle_change                   : 1;
  unsigned char karaoke_audio_pres_mode_change : 1; /* 23 */
  
  unsigned char forward_scan                   : 1; /* 8 */
  unsigned char backward_scan                  : 1;
  unsigned char title_menu_call                : 1;
  unsigned char root_menu_call                 : 1;
  unsigned char subpic_menu_call               : 1;
  unsigned char audio_menu_call                : 1;
  unsigned char angle_menu_call                : 1;
  unsigned char chapter_menu_call              : 1; /* 15 */
  
  unsigned char title_or_time_play             : 1; /* 0 */
  unsigned char chapter_search_or_play         : 1;
  unsigned char title_play                     : 1;
  unsigned char stop                           : 1;
  unsigned char go_up                          : 1;
  unsigned char time_or_chapter_search         : 1;
  unsigned char prev_or_top_pg_search          : 1;
  unsigned char next_pg_search                 : 1; /* 7 */
#endif
} ATTRIBUTE_PACKED user_ops_t;

/**
 * Program Chain Information.
 */
typedef struct {
  quint16 zero_1;
  quint8  nr_of_programs;
  quint8  nr_of_cells;
  dvd_time_t playback_time;
  user_ops_t prohibited_ops;
  quint16 audio_control[8]; /* New type? */
  quint32 subp_control[32]; /* New type? */
  quint16 next_pgc_nr;
  quint16 prev_pgc_nr;
  quint16 goup_pgc_nr;
  quint8  pg_playback_mode;
  quint8  still_time;
  quint32 palette[16]; /* New type struct {zero_1, Y, Cr, Cb} ? */
  quint16 command_tbl_offset;
  quint16 program_map_offset;
  quint16 cell_playback_offset;
  quint16 cell_position_offset;
  pgc_command_tbl_t *command_tbl;
  pgc_program_map_t  *program_map;
  cell_playback_t *cell_playback;
  cell_position_t *cell_position;
} ATTRIBUTE_PACKED pgc_t;
#define PGC_SIZE 236U

/**
 * Program Chain Information Search Pointer.
 */
typedef struct {
  quint8  entry_id;
#ifdef WORDS_BIGENDIAN
  unsigned char block_mode : 2;
  unsigned char block_type : 2;
  unsigned char unknown1   : 4;
#else
  unsigned char unknown1   : 4;
  unsigned char block_type : 2;
  unsigned char block_mode : 2;
#endif  
  quint16 ptl_id_mask;
  quint32 pgc_start_byte;
  pgc_t *pgc;
} ATTRIBUTE_PACKED pgci_srp_t;
#define PGCI_SRP_SIZE 8U

/**
 * Program Chain Information Table.
 */
typedef struct {
  quint16 nr_of_pgci_srp;
  quint16 zero_1;
  quint32 last_byte;
  pgci_srp_t *pgci_srp;
} ATTRIBUTE_PACKED pgcit_t;
#define PGCIT_SIZE 8U

/**
 * Menu PGCI Language Unit.
 */
typedef struct {
  quint16 lang_code;
  quint8  lang_extension;
  quint8  exists;
  quint32 lang_start_byte;
  pgcit_t *pgcit;
} ATTRIBUTE_PACKED pgci_lu_t;
#define PGCI_LU_SIZE 8U

/**
 * Menu PGCI Unit Table.
 */
typedef struct {
  quint16 nr_of_lus;
  quint16 zero_1;
  quint32 last_byte;
  pgci_lu_t *lu;
} ATTRIBUTE_PACKED pgci_ut_t;
#define PGCI_UT_SIZE 8U

/**
 * Cell Address Information.
 */
typedef struct {
  quint16 vob_id;
  quint8  cell_id;
  quint8  zero_1;
  quint32 start_sector;
  quint32 last_sector;
} ATTRIBUTE_PACKED cell_adr_t;

/**
 * Cell Address Table.
 */
typedef struct {
  quint16 nr_of_vobs; /* VOBs */
  quint16 zero_1;
  quint32 last_byte;
  cell_adr_t *cell_adr_table;  /* No explicit size given. */
} ATTRIBUTE_PACKED c_adt_t;
#define C_ADT_SIZE 8U

/**
 * VOBU Address Map.
 */
typedef struct {
  quint32 last_byte;
  quint32 *vobu_start_sectors;
} ATTRIBUTE_PACKED vobu_admap_t;
#define VOBU_ADMAP_SIZE 4U




/**
 * VMGI
 *
 * The following structures relate to the Video Manager.
 */

/**
 * Video Manager Information Management Table.
 */
typedef struct {
  char     vmg_identifier[12];
  quint32 vmg_last_sector;
  quint8  zero_1[12];
  quint32 vmgi_last_sector;
  quint8  zero_2;
  quint8  specification_version;
  quint32 vmg_category;
  quint16 vmg_nr_of_volumes;
  quint16 vmg_this_volume_nr;
  quint8  disc_side;
  quint8  zero_3[19];
  quint16 vmg_nr_of_title_sets;  /* Number of VTSs. */
  char     provider_identifier[32];
  uint64_t vmg_pos_code;
  quint8  zero_4[24];
  quint32 vmgi_last_byte;
  quint32 first_play_pgc;
  quint8  zero_5[56];
  quint32 vmgm_vobs;             /* sector */
  quint32 tt_srpt;               /* sector */
  quint32 vmgm_pgci_ut;          /* sector */
  quint32 ptl_mait;              /* sector */
  quint32 vts_atrt;              /* sector */
  quint32 txtdt_mgi;             /* sector */
  quint32 vmgm_c_adt;            /* sector */
  quint32 vmgm_vobu_admap;       /* sector */
  quint8  zero_6[32];
  
  video_attr_t vmgm_video_attr;
  quint8  zero_7;
  quint8  nr_of_vmgm_audio_streams; /* should be 0 or 1 */
  audio_attr_t vmgm_audio_attr;
  audio_attr_t zero_8[7];
  quint8  zero_9[17];
  quint8  nr_of_vmgm_subp_streams; /* should be 0 or 1 */
  subp_attr_t  vmgm_subp_attr;
  subp_attr_t  zero_10[27];  /* XXX: how much 'padding' here? */
} ATTRIBUTE_PACKED vmgi_mat_t;

typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned char zero_1                    : 1;
  unsigned char multi_or_random_pgc_title : 1; /* 0: one sequential pgc title */
  unsigned char jlc_exists_in_cell_cmd    : 1;
  unsigned char jlc_exists_in_prepost_cmd : 1;
  unsigned char jlc_exists_in_button_cmd  : 1;
  unsigned char jlc_exists_in_tt_dom      : 1;
  unsigned char chapter_search_or_play    : 1; /* UOP 1 */
  unsigned char title_or_time_play        : 1; /* UOP 0 */
#else
  unsigned char title_or_time_play        : 1;
  unsigned char chapter_search_or_play    : 1;
  unsigned char jlc_exists_in_tt_dom      : 1;
  unsigned char jlc_exists_in_button_cmd  : 1;
  unsigned char jlc_exists_in_prepost_cmd : 1;
  unsigned char jlc_exists_in_cell_cmd    : 1;
  unsigned char multi_or_random_pgc_title : 1;
  unsigned char zero_1                    : 1;
#endif
} ATTRIBUTE_PACKED playback_type_t;

/**
 * Title Information.
 */
typedef struct {
  playback_type_t pb_ty;
  quint8  nr_of_angles;
  quint16 nr_of_ptts;
  quint16 parental_id;
  quint8  title_set_nr;
  quint8  vts_ttn;
  quint32 title_set_sector;
} ATTRIBUTE_PACKED title_info_t;

/**
 * PartOfTitle Search Pointer Table.
 */
typedef struct {
  quint16 nr_of_srpts;
  quint16 zero_1;
  quint32 last_byte;
  title_info_t *title;
} ATTRIBUTE_PACKED tt_srpt_t;
#define TT_SRPT_SIZE 8U


/**
 * Parental Management Information Unit Table.
 * Level 1 (US: G), ..., 7 (US: NC-17), 8
 */
typedef quint16 pf_level_t[8];

/**
 * Parental Management Information Unit Table.
 */
typedef struct {
  quint16 country_code;
  quint16 zero_1;
  quint16 pf_ptl_mai_start_byte;
  quint16 zero_2;
  pf_level_t *pf_ptl_mai; /* table of (nr_of_vtss + 1), video_ts is first */
} ATTRIBUTE_PACKED ptl_mait_country_t;
#define PTL_MAIT_COUNTRY_SIZE 8U

/**
 * Parental Management Information Table.
 */
typedef struct {
  quint16 nr_of_countries;
  quint16 nr_of_vtss;
  quint32 last_byte;
  ptl_mait_country_t *countries;
} ATTRIBUTE_PACKED ptl_mait_t;
#define PTL_MAIT_SIZE 8U

/**
 * Video Title Set Attributes.
 */
typedef struct {
  quint32 last_byte;
  quint32 vts_cat;
  
  video_attr_t vtsm_vobs_attr;
  quint8  zero_1;
  quint8  nr_of_vtsm_audio_streams; /* should be 0 or 1 */
  audio_attr_t vtsm_audio_attr;
  audio_attr_t zero_2[7];  
  quint8  zero_3[16];
  quint8  zero_4;
  quint8  nr_of_vtsm_subp_streams; /* should be 0 or 1 */
  subp_attr_t vtsm_subp_attr;
  subp_attr_t zero_5[27];
  
  quint8  zero_6[2];
  
  video_attr_t vtstt_vobs_video_attr;
  quint8  zero_7;
  quint8  nr_of_vtstt_audio_streams;
  audio_attr_t vtstt_audio_attr[8];
  quint8  zero_8[16];
  quint8  zero_9;
  quint8  nr_of_vtstt_subp_streams;
  subp_attr_t vtstt_subp_attr[32];
} ATTRIBUTE_PACKED vts_attributes_t;
#define VTS_ATTRIBUTES_SIZE 542U
#define VTS_ATTRIBUTES_MIN_SIZE 356U

/**
 * Video Title Set Attribute Table.
 */
typedef struct {
  quint16 nr_of_vtss;
  quint16 zero_1;
  quint32 last_byte;
  vts_attributes_t *vts;
  quint32 *vts_atrt_offsets; /* offsets table for each vts_attributes */
} ATTRIBUTE_PACKED vts_atrt_t;
#define VTS_ATRT_SIZE 8U

/**
 * Text Data. (Incomplete)
 */
typedef struct {
  quint32 last_byte;    /* offsets are relative here */
  quint16 offsets[100]; /* == nr_of_srpts + 1 (first is disc title) */
#if 0  
  quint16 unknown; /* 0x48 ?? 0x48 words (16bit) info following */
  quint16 zero_1;
  
  quint8 type_of_info; /* ?? 01 == disc, 02 == Title, 04 == Title part */
  quint8 unknown1;
  quint8 unknown2;
  quint8 unknown3;
  quint8 unknown4; /* ?? allways 0x30 language?, text format? */
  quint8 unknown5;
  quint16 offset; /* from first */
  
  char text[12]; /* ended by 0x09 */
#endif
} ATTRIBUTE_PACKED txtdt_t;

/**
 * Text Data Language Unit. (Incomplete)
 */ 
typedef struct {
  quint16 lang_code;
  quint16 unknown;      /* 0x0001, title 1? disc 1? side 1? */
  quint32 txtdt_start_byte;  /* prt, rel start of vmg_txtdt_mgi  */
  txtdt_t  *txtdt;
} ATTRIBUTE_PACKED txtdt_lu_t;
#define TXTDT_LU_SIZE 8U

/**
 * Text Data Manager Information. (Incomplete)
 */
typedef struct {
  char disc_name[14];            /* how many bytes?? */
  quint16 nr_of_language_units; /* 32bit??          */
  quint32 last_byte;
  txtdt_lu_t *lu;
} ATTRIBUTE_PACKED txtdt_mgi_t;
#define TXTDT_MGI_SIZE 20U


/**
 * VTS
 *
 * Structures relating to the Video Title Set (VTS).
 */

/**
 * Video Title Set Information Management Table.
 */
typedef struct {
  char vts_identifier[12];
  quint32 vts_last_sector;
  quint8  zero_1[12];
  quint32 vtsi_last_sector;
  quint8  zero_2;
  quint8  specification_version;
  quint32 vts_category;
  quint16 zero_3;
  quint16 zero_4;
  quint8  zero_5;
  quint8  zero_6[19];
  quint16 zero_7;
  quint8  zero_8[32];
  uint64_t zero_9;
  quint8  zero_10[24];
  quint32 vtsi_last_byte;
  quint32 zero_11;
  quint8  zero_12[56];
  quint32 vtsm_vobs;       /* sector */
  quint32 vtstt_vobs;      /* sector */
  quint32 vts_ptt_srpt;    /* sector */
  quint32 vts_pgcit;       /* sector */
  quint32 vtsm_pgci_ut;    /* sector */
  quint32 vts_tmapt;       /* sector */
  quint32 vtsm_c_adt;      /* sector */
  quint32 vtsm_vobu_admap; /* sector */
  quint32 vts_c_adt;       /* sector */
  quint32 vts_vobu_admap;  /* sector */
  quint8  zero_13[24];
  
  video_attr_t vtsm_video_attr;
  quint8  zero_14;
  quint8  nr_of_vtsm_audio_streams; /* should be 0 or 1 */
  audio_attr_t vtsm_audio_attr;
  audio_attr_t zero_15[7];
  quint8  zero_16[17];
  quint8  nr_of_vtsm_subp_streams; /* should be 0 or 1 */
  subp_attr_t vtsm_subp_attr;
  subp_attr_t zero_17[27];
  quint8  zero_18[2];
  
  video_attr_t vts_video_attr;
  quint8  zero_19;
  quint8  nr_of_vts_audio_streams;
  audio_attr_t vts_audio_attr[8];
  quint8  zero_20[17];
  quint8  nr_of_vts_subp_streams;
  subp_attr_t vts_subp_attr[32];
  quint16 zero_21;
  multichannel_ext_t vts_mu_audio_attr[8];
  /* XXX: how much 'padding' here, if any? */
} ATTRIBUTE_PACKED vtsi_mat_t;

/**
 * PartOfTitle Unit Information.
 */
typedef struct {
  quint16 pgcn;
  quint16 pgn;
} ATTRIBUTE_PACKED ptt_info_t;

/**
 * PartOfTitle Information.
 */
typedef struct {
  quint16 nr_of_ptts;
  ptt_info_t *ptt;
} ATTRIBUTE_PACKED ttu_t;

/**
 * PartOfTitle Search Pointer Table.
 */
typedef struct {
  quint16 nr_of_srpts;
  quint16 zero_1;
  quint32 last_byte;
  ttu_t  *title;
  quint32 *ttu_offset; /* offset table for each ttu */
} ATTRIBUTE_PACKED vts_ptt_srpt_t;
#define VTS_PTT_SRPT_SIZE 8U


/**
 * Time Map Entry.
 */
/* Should this be bit field at all or just the quint32? */
typedef quint32 map_ent_t;

/**
 * Time Map.
 */
typedef struct {
  quint8  tmu;   /* Time unit, in seconds */
  quint8  zero_1;
  quint16 nr_of_entries;
  map_ent_t *map_ent;
} ATTRIBUTE_PACKED vts_tmap_t;
#define VTS_TMAP_SIZE 4U

/**
 * Time Map Table.
 */
typedef struct {
  quint16 nr_of_tmaps;
  quint16 zero_1;
  quint32 last_byte;
  vts_tmap_t *tmap;
  quint32 *tmap_offset; /* offset table for each tmap */
} ATTRIBUTE_PACKED vts_tmapt_t;
#define VTS_TMAPT_SIZE 8U


#if PRAGMA_PACK
#pragma pack()
#endif


/**
 * The following structure defines an IFO file.  The structure is divided into
 * two parts, the VMGI, or Video Manager Information, which is read from the
 * VIDEO_TS.[IFO,BUP] file, and the VTSI, or Video Title Set Information, which
 * is read in from the VTS_XX_0.[IFO,BUP] files.
 */
typedef struct {
  dvd_file_t *file;
  
  /* VMGI */
  vmgi_mat_t     *vmgi_mat;
  tt_srpt_t      *tt_srpt;
  pgc_t          *first_play_pgc;    
  ptl_mait_t     *ptl_mait;
  vts_atrt_t     *vts_atrt;
  txtdt_mgi_t    *txtdt_mgi;
  
  /* Common */
  pgci_ut_t      *pgci_ut;
  c_adt_t        *menu_c_adt;
  vobu_admap_t   *menu_vobu_admap;
  
  /* VTSI */
  vtsi_mat_t     *vtsi_mat;
  vts_ptt_srpt_t *vts_ptt_srpt;
  pgcit_t        *vts_pgcit;
  vts_tmapt_t    *vts_tmapt;
  c_adt_t        *vts_c_adt;
  vobu_admap_t   *vts_vobu_admap;
} ifo_handle_t;

#endif /* IFO_TYPES_H_INCLUDED */
