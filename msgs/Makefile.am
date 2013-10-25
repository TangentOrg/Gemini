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

noinst_SCRIPTS =	msgcomp.pl

targets =		gemmsgs.db

BUILT_SOURCES =		$(targets) gemmsgs.txt
EXTRA_DIST =		$(noinst_SCRIPTS) $(BUILT_SOURCES) INSTALL-BINARY

all:			$(targets)

CLEAN_FILES:		$(BUILD_SOURCES)
	touch $(BUILD_SOURCES)

#install the Gemini message file in the share directory
#install-data-local:
#	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)
#	$(INSTALL_DATA) $(srcdir)/gemmsgs.db \
#		$(DESTDIR)$(pkgdatadir)/gemmsgs.db;
 
gemmsgs.db: gemmsgs.txt
	perl msgcomp.pl gemmsgs.txt gemmsgs.db

# Don't update the files from bitkeeper
%::SCCS/s.%
