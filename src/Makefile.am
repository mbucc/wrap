##
#	wrap -- text reformatter
#	src/Makefile.am
#
#	Copyright (C) 2013-2014  Paul J. Lucas
#
#	This program is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; either version 2 of the Licence, or
#	(at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

bin_PROGRAMS = wrap wrapc

wrap_SOURCES = alias.c common.c getopt.c read_conf.c wrap.c
if am_WITH_PATTERNS
wrap_SOURCES += pattern.c
endif

wrapc_SOURCES = common.c getopt.c wrapc.c

# vim:set noet sw=8 ts=8:
