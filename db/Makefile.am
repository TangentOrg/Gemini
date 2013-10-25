
# Copyright (C) 2001 NuSphere Corporation, All Rights Reserved.
#
# This program is open source software.  You may not copy or use this
# file, in either source code or executable form, except in compliance
# with the NuSphere Public License.  You can redistribute it and/or
# modify it under the terms of the NuSphere Public License as published
# by the NuSphere Corporation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# NuSphere Public License for more details.
#
# You should have received a copy of the NuSphere Public License
# along with this program; if not, write to NuSphere Corporation
# 14 Oak Park, Bedford, MA 01730.

libsdir = ../libs

INCLUDES =		-I../incl -I../inclprv -I../msgs

libs_LIBRARIES =	libdb.a

libdb_a_SOURCES =	bkcheck.c bkcrashtest.c bkio.c bkiolk.c bkiotop.c \
			bklog.c bkopen.c bkrec.c bkrepl.c \
			bksubs.c bmapw.c bmbuf.c cx.c cxaux.c cxblock.c \
			cxcompact.c cxdo.c cxfil.c cxgen.c cxkill.c cxky.c \
			cxkyut.c cxmove.c cxnxt.c cxtag.c cxundo.c \
			db.c dbarea.c dbblob.c dbclose.c dbindex.c dblang.c \
			dblksch.c dbobject.c dbopen.c dbquiet.c dbrecord.c \
			dbtable.c dbuser.c dbwdog.c geminifromkey.c \
			geminikey.c keysvc.c keyut.c ky.c kyut.c latctl.c \
			lkmu.c lkrecd.c om.c omundo.c rec.c recformt.c \
			recget.c recput.c rlai.c rlaiovl.c rlaoff.c rlcl.c \
			rlmem.c rlnote.c rlrej.c rlrw.c rlset.c rltl.c rm.c \
			rmdoundo.c semadm.c semmgr.c seqdundo.c \
			seqgen.c shm.c shmatt.c shmcr.c shmdel.c shmpid.c \
			shmptoq.c shmsiz.c stm.c tm.c tmdoundo.c tstset.cc

tstset.o : tstset.cc
	../ctstset

EXTRA_PROGRAMS =	

# Don't update the files from bitkeeper
%::SCCS/s.%

