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

#ifndef AREA_H
#define AREA_H

/*************************************************/
/* area.h - Structure definition for Area Block. */
/*************************************************/

#include "bkpub.h"  /* for struct bkhdr */

typedef struct areaBlock_t
{
    struct bkhdr bk_hdr;       /* standard block header */
    /* "Fun Block" - No additional data in this block yet. */

} areaBlock_t;

#endif  /* #ifndef AREA_H */
