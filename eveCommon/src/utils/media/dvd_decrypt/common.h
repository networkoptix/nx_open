/*****************************************************************************
 * common.h: common definitions
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: common.h 235 2010-08-02 15:41:14Z jb $
 *
 * Authors: Sam Hocevar <sam@via.ecp.fr>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * You should have received a copy of the GNU General Public License along
 * with libdvdcss; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *****************************************************************************/

#ifndef DVDCSS_COMMON_H
#define DVDCSS_COMMON_H

/*****************************************************************************
 * Basic types definitions
 *****************************************************************************/
#if defined( WIN32 )

#   ifndef PATH_MAX
#      define PATH_MAX MAX_PATH
#   endif

/* several type definitions */
#   if defined( __MINGW32__ )
#       define lseek_func _lseeki64
#       if !defined( _OFF_T_ )
typedef long long _off_t;
typedef _off_t off_t;
#           define _OFF_T_
#       else
#           define off_t long long
#       endif
#   endif

#   if defined( _MSC_VER )
#       define lseek_func _lseeki64
#       if !defined( _OFF_T_DEFINED )
typedef __int64 off_t;
#           define _OFF_T_DEFINED
#       else
#           define off_t __int64
#       endif
#       define stat _stati64
#   endif

#   ifndef snprintf
#       define snprintf _snprintf  /* snprintf not defined in mingw32 (bug?) */
#   endif
#else
#   define lseek_func lseek
#endif

#endif /* DVDCSS_COMMON_H */
