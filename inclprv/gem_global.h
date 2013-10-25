/* 
Copyright (C) 2001 NuSphere Corporation, All Rights Reserved.
   
This program is open source software.  You may not copy or use this
file, in either source code or executable form, except in compliance 
with the NuSphere Public License.  You can redistribute it and/or
modify it under the terms of the NuSphere Public License as published
by the NuSphere Corporation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
NuSphere Public License for more details.

You should have received a copy of the NuSphere Public License
along with this program; if not, write to NuSphere Corporation
14 Oak Park, Bedford, MA 01730.
*/

#ifndef _GEM_GLOBAL_H
#define _GEM_GLOBAL_H

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(WIN32)
#include "gem_config_win.h"
#else
#include "gem_config.h"
#endif

#if (!defined (__GNUC__) || (__GNUC__ < 2) \
                         || (__GNUC__ == 2 && __GNUC_MINOR__ < 7))
#define _UNUSED_
#else
#define _UNUSED_ __attribute__((__unused__))
#endif

#endif /* #ifndef _GEM_GLOBAL_H */

