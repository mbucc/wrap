/*
**      wrap -- text reformatter
**      common.h
**
**      Copyright (C) 1996-2014  Paul J. Lucas
**
**      This program is free software; you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the Licence, or
**      (at your option) any later version.
** 
**      This program is distributed in the hope that it will be useful,
**      but WITHOUT ANY WARRANTY; without even the implied warranty of
**      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**      GNU General Public License for more details.
** 
**      You should have received a copy of the GNU General Public License
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef wrap_common_H
#define wrap_common_H

/* standard */
#include <stddef.h>                     /* for size_t */

/* local */
#include "config.h"

/*****************************************************************************/

#define LINE_BUF_SIZE             8192  /* hopefully, no one will exceed this */
#define LINE_WIDTH_DEFAULT        80    /* wrap text to this line width */
#define LINE_WIDTH_MINIMUM        1
#define NEWLINES_DELIMIT_DEFAULT  2     /* # newlines that delimit a para */
#define TAB_SPACES_DEFAULT        8     /* number of spaces a tab equals */

/* exit(3) status codes */
#define EXIT_OK             0
#define EXIT_USAGE          1
#define EXIT_CONF_ERROR     2
#define EXIT_OUT_OF_MEMORY  3
#define EXIT_READ_OPEN      10
#define EXIT_READ_ERROR     11
#define EXIT_WRITE_OPEN     12
#define EXIT_WRITE_ERROR    13
#define EXIT_FORK_ERROR     20
#define EXIT_EXEC_ERROR     21
#define EXIT_CHILD_SIGNAL   30
#define EXIT_PIPE_ERROR     40

/* define a "bool" type */
#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#   ifdef __cplusplus
typedef bool _Bool;
#   else
#     define _Bool signed char
#   endif /* __cplusplus */
# endif /* HAVE__BOOL */
# define bool   _Bool
# define false  0
# define true   1
# define __bool_true_false_are_defined 1
#endif /* HAVE_STDBOOL_H */

#define BLOCK(...) \
  do { __VA_ARGS__ } while (0)

#define PERROR_EXIT(STATUS) \
  BLOCK( perror( me ); exit( EXIT_##STATUS ); )

#define PMESSAGE_EXIT(STATUS,FORMAT,...) \
  BLOCK( fprintf( stderr, "%s: " FORMAT, me, __VA_ARGS__ ); exit( EXIT_##STATUS ); )

#define CHECK_FGETX(FILE) \
  BLOCK( if ( ferror( FILE ) ) PERROR_EXIT( READ_ERROR ); )

#define FPRINTF(FILE,...) \
  BLOCK( if ( fprintf( FILE, __VA_ARGS__ ) < 0 ) PERROR_EXIT( WRITE_ERROR ); )

#define FPUTC(C,FILE) \
  BLOCK( if ( putc( (C), FILE ) == EOF ) PERROR_EXIT( WRITE_ERROR ); )

#define FPUTS(S,FILE) \
  BLOCK( if ( fputs( (S), FILE ) == EOF ) PERROR_EXIT( WRITE_ERROR ); )

#define MALLOC(TYPE,N) \
  (TYPE*)check_realloc( NULL, sizeof(TYPE) * (N) )

#define REALLOC(PTR,TYPE,N) \
  (PTR) = (TYPE*)check_realloc( (PTR), sizeof(TYPE) * (N) )

/**
 * Extracts the base portion of a path-name.
 * Unlike \c basename(3):
 *  + Trailing \c '/' characters are not deleted.
 *  + \a path_name is never modified (hence can therefore be \c const).
 *  + Returns a pointer within \a path_name (hence is multi-call safe).
 *
 * @param path_name The path-name to extract the base portion of.
 * @return Returns a pointer to the last component of \a path_name.
 * If \a path_name consists entirely of '/' characters,
 * a pointer to the string "/" is returned.
 */
char const* base_name( char const *path_name );

/**
 * Converts an ASCII string to an unsigned integer.
 * Unlike atoi(3), this function insists that all characters in \a s
 * are digits.
 * If conversion fails, prints an error message and exits.
 *
 * @param s The string to convert.
 * @return Returns the unsigned integer.
 */
int check_atou( char const *s );

/**
 * Calls realloc(3) and checks for failure.
 * If reallocation fails, prints an error message and exits.
 *
 * @param p The pointer to reallocate.  If NULL, new memory is allocated.
 * @param size The number of bytes to allocate.
 * @return Returns a pointer to the allocated memory.
 */
void* check_realloc( void *p, size_t size );

/*****************************************************************************/

#endif /* wrap_common_H */
/* vim:set et sw=2 ts=2: */
