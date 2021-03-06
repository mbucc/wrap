/*
**      wrap -- text reformatter
**      wrap.c
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

/* system */
#include <ctype.h>
#include <errno.h>
#include <limits.h>                     /* for INT_MAX */
#include <stdio.h>
#include <stdlib.h>                     /* for exit(), ... */
#include <string.h>
#include <unistd.h>                     /* for STDIN_FILENO, ... */

/* local */
#include "alias.h"
#include "common.h"
#include "getopt.h"
#ifdef WITH_PATTERNS
# include "pattern.h"
#endif /* WITH_PATTERNS */
#include "read_conf.h"

/* global variable definitions */
char        buf[ LINE_BUF_SIZE ];
FILE*       fin = NULL;                 /* file in */
FILE*       fout = NULL;                /* file out */
char const* me;                         /* executable name */

/* option variable definitions */
char const* opt_alias = NULL;
char const* opt_conf_file = NULL;
bool        opt_eos_delimit = false;    /* end-of-sentence delimits para's? */
char const* opt_fin = NULL;             /* file in path */
char const* opt_fin_name = NULL;        /* file in name (only) */
char const* opt_fout = NULL;            /* file out path */
int         opt_hang_spaces = 0;        /* hanging-indent spaces */
int         opt_hang_tabs = 0;          /* hanging-indent tabs */
int         opt_indt_spaces = 0;        /* indent spaces */
int         opt_indt_tabs = 0;          /* indent tabs */
bool        opt_lead_dot_ignore = false;/* ignore lines starting with '.'? */
int         opt_lead_spaces = 0;        /* leading spaces */
int         opt_lead_tabs = 0;          /* leading tabs */
bool        opt_lead_ws_delimit = false;/* leading whitespace delimit para's? */
int         opt_line_width = LINE_WIDTH_DEFAULT;
int         opt_mirror_spaces = 0;
int         opt_mirror_tabs = 0;
int         opt_newlines_delimit = NEWLINES_DELIMIT_DEFAULT;
bool        opt_no_conf = false;        /* do not read conf file */
char const* opt_para_delimiters = NULL; /* additional para delimiter chars */
int         opt_tab_spaces = TAB_SPACES_DEFAULT;
bool        opt_title_line = false;     /* 1st para line is title? */

static char const utf8_len_table[] = {
  /*      0 1 2 3 4 5 6 7 8 9 A B C D E F */ 
  /* 0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 1 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 2 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 3 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 4 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 5 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 6 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 7 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 8 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* continuation bytes         */
  /* 9 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /*        |                   */
  /* A */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /*        |                   */
  /* B */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /*        |                   */
  /* C */ 0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  /* C0 & C1 are overlong ASCII */
  /* D */ 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  /* E */ 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
  /* F */ 4,4,4,4,4,4,4,4,5,5,5,5,6,6,0,0
};

#define UTF8_BOM            "\xEF\xBB\xBF"
#define UTF8_BOM_LEN        (sizeof( UTF8_BOM ) - 1 /* terminating null */)
#define UTF8_LEN(C)         ((int)utf8_len_table[ (unsigned char)(C) ])

/* local functions */
static bool copy_line( FILE *fin, FILE *fout, char *buf, size_t buf_size );
static void clean_up();
static void hang_indent( int *count, int *width );
static void init( int argc, char const *argv[] );
static void print_buf( int len, bool newline );
static void print_lead_chars();
static void process_options( int argc, char const *argv[], char const *opts,
                             int line_no );
static void usage();

/*****************************************************************************/

int main( int argc, char const *argv[] ) {
  int c, prev_c = '\0';                 /* current/previous character */
  int buf_count = 0;                    /* number of characters in buffer */
  int buf_width = 0;                    /* actual width of buffer */
  int tmp_buf_count;
  int utf8_len;                         /* bytes comprising UTF-8 character */
  int wrap_pos = 0;                     /* position at which we can wrap */
  int i, from;                          /* work variables */

  /*
   * Number of consecutive newlines.
   */
  int  consec_newlines = 0;

  /*
   * True only when we should do hang-indenting after a title line.
   */
  bool do_hang_indent = false;

  /*
   * True only when opt_lead_dot_ignore = true, the current character is a '.',
   * and the previous character was a '\n' (i.e., the line starts with a '.').
   */
  bool do_ignore_lead_dot = false;

  /*
   * True only when we should do indenting: set initially to indent the first
   * line of a paragraph and after opt_newlines_delimit or more consecutive
   * newlines for subsequent paragraphs.
   */
  bool do_indent = true;

  /*
   * True only when we're handling a "long line" (a sequence of non-whitespace
   * characters longer than opt_line_width) that can't be wrapped.
   */
  bool is_long_line = false;

  /*
   * True when the next line to be read will be a title line.
   */
  bool next_line_is_title;

  /*
   * Number of spaces to output before the next non-whitespace character: set
   * to 1 when we encounter a whitespace character and 2 when was_eos_char is
   * true so as to put 2 characters after the end of a sentence.
   */
  int  put_spaces = 0;

  /*
   * True only if the previous character was an end-of-sentence character.
   */
  bool was_eos_char = false;

  /*
   * True only if the previous character was a paragraph-delimiter character.
   */
  bool was_para_delim_char = false;

  /***************************************************************************/

  init( argc, argv );

  /*
   * The first non-all-whitespace line of the first paragraph is a title line
   * only if opt_title_line (-T) is true.
   */
  next_line_is_title = opt_title_line;

  for ( ; (c = getc( fin )) != EOF; prev_c = c ) {

    /*************************************************************************
     *  HANDLE NEWLINE(s)
     *************************************************************************/

    if ( c == '\n' ) {
      if ( ++consec_newlines >= opt_newlines_delimit ) {
        /*
         * At least opt_newlines_delimit consecutive newlines: set that the
         * next line is a title line and delimit the paragraph.
         */
        next_line_is_title = opt_title_line;
        goto delimit_paragraph;
      }
      if ( opt_eos_delimit && was_eos_char ) {
        /*
         * End-of-sentence characters delimit paragraphs and the previous
         * character was an end-of-sentence character: delimit the paragraph.
         */
        goto delimit_paragraph;
      }
      if ( next_line_is_title && buf_count ) {
        /*
         * The first line of the next paragraph is title line and the buffer
         * isn't empty (there is a title): print the title.
         */
        print_lead_chars();
        print_buf( buf_count, true );
        buf_count = buf_width = 0;
        do_hang_indent = true;
        next_line_is_title = false;
        continue;
      }
      if ( was_eos_char ) {
        /*
         * We are joining a line after the end of a sentence: force 2 spaces.
         */
        put_spaces = 2;
        continue;
      }
    } else {
      consec_newlines = 0;
    }

    /*************************************************************************
     *  HANDLE WHITE-SPACE
     *************************************************************************/

    if ( isspace( c ) ) {
      if ( is_long_line ) {
        /*
         * We've been handling a "long line" and finally got a whitespace
         * character at which we can finally wrap: delimit the paragraph.
         */
        goto delimit_paragraph;
      }
      if ( opt_lead_ws_delimit && prev_c == '\n' ) {
        /*
         * Leading whitespace characters delimit paragraphs and the previous
         * character was a newline which means this whitespace character is at
         * the beginning of a line: delimit the paragraph.
         */
        goto delimit_paragraph;
      }
      if ( opt_eos_delimit && was_eos_char ) {
        /*
         * End-of-sentence characters delimit paragraphs and the previous
         * character was an end-of-sentence character: delimit the paragraph.
         */
        goto delimit_paragraph;
      }
      if ( was_para_delim_char ) {
        /*
         * The previous character was a paragraph-delimiter character (set only
         * if opt_para_delimiters was set): delimit the paragraph.
         */
        goto delimit_paragraph;
      }
      if ( buf_count && put_spaces < 1 + was_eos_char ) {
        /*
         * We are not at the beginning of a line: remember to insert 1 space
         * later amd allow 2 after the end of a sentence.
         */
        ++put_spaces;
      }
      continue;
    }

    /*************************************************************************
     *  DISCARD CONTROL CHARACTERS
     *************************************************************************/

    if ( iscntrl( c ) )
      continue;

    /*************************************************************************
     *  HANDLE LEADING-DOT, END-OF-SENTENCE, AND PARAGRAPH-DELIMITERS
     *************************************************************************/

    if ( opt_lead_dot_ignore && prev_c == '\n' && c == '.' ) {
      do_ignore_lead_dot = true;
      goto delimit_paragraph;
    }

    /*
     * Treat a quote or a closing parenthesis or bracket as an end-of-sentence
     * character if it was preceded by a regular end-of-sentence character.
     */
    if ( !(was_eos_char && strchr( "'\")]", c )) )
      was_eos_char = (strchr( ".?!", c ) != NULL);

    if ( opt_para_delimiters )
      was_para_delim_char = (strchr( opt_para_delimiters, c ) != NULL);

    /*************************************************************************
     *  INSERT SPACES
     *************************************************************************/

    if ( put_spaces ) {
      if ( buf_count ) {
        /*
         * Mark position at a space to perform a wrap if necessary.
         */
        wrap_pos = buf_count;
        buf_width += put_spaces;
        do {
          buf[ buf_count++ ] = ' ';
        } while ( --put_spaces );
      } else {
        /*
         * Never put spaces at the beginning of a line.
         */
        put_spaces = 0;
      }
    }

    /*************************************************************************
     *  PERFORM INDENTATION
     *************************************************************************/

    if ( do_indent ) {
      buf_count = 0;
      for ( i = 0; i < opt_indt_tabs; ++i )
        buf[ buf_count++ ] = '\t';
      for ( i = 0; i < opt_indt_spaces; ++i )
        buf[ buf_count++ ] = ' ';
      buf_width = opt_indt_tabs * opt_tab_spaces + opt_indt_spaces;
      do_indent = false;
    }

    if ( do_hang_indent ) {
      hang_indent( &buf_count, &buf_width );
      do_hang_indent = false;
    }

    /*************************************************************************
     *  INSERT NON-SPACE CHARACTER
     *************************************************************************/

    utf8_len = UTF8_LEN( c );
    if ( !utf8_len )                    /* unexpected UTF-8 continuation byte */
      continue;

    tmp_buf_count = buf_count;          /* save count in case invalid UTF-8 */
    buf[ tmp_buf_count++ ] = c;

    /*
     * If we've just read the start byte of a multi-byte UTF-8 character, read
     * the remaining bytes comprising the character.  The entire muti-byte
     * UTF-8 character is always treated as having a width of 1.
     */
    for ( i = utf8_len; i > 1; --i ) {
      if ( (c = getc( fin )) == EOF )
        goto done;                      /* premature end of UTF-8 character */
      buf[ tmp_buf_count++ ] = c;
      if ( UTF8_LEN( c ) )              /* unexpected UTF-8 start byte */
        goto continue_outer_loop;       /* skip entire UTF-8 character */
    }
    buf_count = tmp_buf_count;          /* UTF-8 is valid */

    if ( utf8_len == UTF8_BOM_LEN ) {
      int const utf8_start_pos = buf_count - UTF8_BOM_LEN;
      if ( strncmp( buf + utf8_start_pos, UTF8_BOM, UTF8_BOM_LEN ) == 0 )
        continue;                       /* discard UTF-8 BOM */
    }

    if ( ++buf_width < opt_line_width ) {
      /*
       * We haven't exceeded the line width yet.
       */
      continue;
    }

    /*************************************************************************
     *  EXCEEDED LINE WIDTH; PRINT LINE OUT
     *************************************************************************/

    if ( !wrap_pos ) {
      /*
       * We've exceeded the line width, but haven't encountered a whitespace
       * character at which to wrap; therefore, we've got a "long line."
       */
      if ( !is_long_line )
        print_lead_chars();
      print_buf( buf_count, false );
      buf_count = buf_width = 0;
      is_long_line = true;
      continue;
    }

    is_long_line = false;
    print_lead_chars();
    print_buf( wrap_pos, true );

    /*
     * Prepare to slide the partial word to the left where we can pick up from
     * where we left off the next time around.
     */
    tmp_buf_count = buf_count;
    buf_count = buf_width = 0;
    hang_indent( &buf_count, &buf_width );

    /*
     * Now slide the partial word to the left.
     */
    for ( from = wrap_pos + 1; from < tmp_buf_count; ++from ) {
      c = buf[ from ];
      if ( !isspace( c ) ) {
        buf[ buf_count++ ] = c;
        ++buf_width;
        for ( utf8_len = UTF8_LEN( c ); utf8_len > 1; --utf8_len )
          buf[ buf_count++ ] = buf[ ++from ];
      }
    }

    wrap_pos = 0;

continue_outer_loop:
    continue;

    /*************************************************************************/

delimit_paragraph:
    if ( buf_count ) {
      /*
       * Print what's in the buffer before delimiting the paragraph.  If we've
       * been handling a "long line," it's now finally ended; otherwise, print
       * the leading characters.
      */
      if ( is_long_line )
        is_long_line = false;
      else
        print_lead_chars();
      print_buf( buf_count, true );
      buf_count = buf_width = 0;
    } else if ( is_long_line )
      FPUTC( '\n', fout );              /* delimit the "long line" */

    put_spaces = 0;
    was_eos_char = was_para_delim_char = false;

    if ( do_ignore_lead_dot ) {
      do_ignore_lead_dot = false;
      /*
       * The line starts with a leading dot and opt_lead_dot_ignore is true:
       * read/write the line as-is.
       */
      FPUTC( '.', fout );
      if ( !copy_line( fin, fout, buf, LINE_BUF_SIZE ) )
        goto done;
    } else {
      if ( consec_newlines == 2 || 
          (consec_newlines > 2 && opt_newlines_delimit == 1) ) {
        FPUTC( c, fout );
      }
      do_indent = true;
    }
  } /* for */

done:
  CHECK_FGETX( fin );
  if ( buf_count ) {                    /* print left-over text */
    if ( !is_long_line )
      print_lead_chars();
    print_buf( buf_count, true );
  }
  exit( EXIT_OK );
}

/*****************************************************************************/

static void clean_up() {
  alias_cleanup();
#ifdef WITH_PATTERNS
  pattern_cleanup();
#endif /* WITH_PATTERNS */
}

static bool copy_line( FILE *fin, FILE *fout, char *buf, size_t buf_size ) {
  char *const last = buf + buf_size - 1;
  do {
    *last = '\0';
    if ( !fgets( buf, buf_size, fin ) ) {
      CHECK_FGETX( fin );
      return false;
    }
    FPUTS( buf, fout );
  } while ( *last && *last != '\n' );
  return true;
}

static void hang_indent( int *count, int *width ) {
  int i;
  for ( i = 0; i < opt_hang_tabs; ++i )
    buf[ (*count)++ ] = '\t';
  for ( i = 0; i < opt_hang_spaces; ++i )
    buf[ (*count)++ ] = ' ';
  *width += opt_hang_tabs * opt_tab_spaces + opt_hang_spaces;
}

static void init( int argc, char const *argv[] ) {
  char const opts[] = "a:bc:Cdef:F:h:H:i:I:l:m:M:nNo:p:s:S:t:Tvw:W";

  me = base_name( argv[0] );
  process_options( argc, argv, opts, 0 );
  atexit( clean_up );
  argc -= optind, argv += optind;
  if ( argc )
    usage();

  if ( !opt_no_conf && (opt_alias || opt_fin_name ) ) {
    alias_t const *alias = NULL;
    opt_conf_file = read_conf( opt_conf_file );
    if ( opt_alias ) {
      if ( !(alias = alias_find( opt_alias )) )
        PMESSAGE_EXIT( USAGE,
          "\"%s\": no such alias in %s\n",
          opt_alias, opt_conf_file
        );
    }
#ifdef WITH_PATTERNS
    else if ( opt_fin_name )
      alias = pattern_find( opt_fin_name );
#endif /* WITH_PATTERNS */
    if ( alias )
      process_options( alias->argc, alias->argv, opts, alias->line_no );
  }

  if ( opt_fin && !(fin = fopen( opt_fin, "r" )) )
    PMESSAGE_EXIT( READ_OPEN, "\"%s\": %s\n", optarg, strerror( errno ) );
  if ( opt_fout && !(fout = fopen( opt_fout, "w" )) )
    PMESSAGE_EXIT( WRITE_OPEN, "\"%s\": %s\n", optarg, strerror( errno ) );

  if ( !fin )
    fin = stdin;
  if ( !fout )
    fout = stdout;

  opt_line_width -=
    2 * (opt_mirror_tabs * opt_tab_spaces + opt_mirror_spaces) +
    opt_lead_tabs * opt_tab_spaces + opt_lead_spaces;

  if ( opt_line_width < LINE_WIDTH_MINIMUM )
    PMESSAGE_EXIT( USAGE,
      "line-width (%d) is too small (<%d)\n", opt_line_width, LINE_WIDTH_MINIMUM
    );

  opt_lead_tabs   += opt_mirror_tabs;
  opt_lead_spaces += opt_mirror_spaces;
}

static void print_buf( int len, bool newline ) {
  buf[ len ] = '\0';
  FPRINTF( fout, newline ? "%s\n" : "%s", buf );
}

static void print_lead_chars() {
  int i;
  for ( i = 0; i < opt_lead_tabs; ++i )
    FPUTC( '\t', fout );
  for ( i = 0; i < opt_lead_spaces; ++i )
    FPUTC( ' ', fout );
}

static void process_options( int argc, char const *argv[], char const *opts,
                             int line_no ) {
  int opt;                              /* command-line option */

  optind = opterr = 1;
  while ( (opt = pjl_getopt( argc, argv, opts )) != EOF ) {
    if ( line_no && strchr( "acCfFov", opt ) )
      PMESSAGE_EXIT( CONF_ERROR,
        "%s:%d: '%c': option not allowed in configuration file\n",
        opt_conf_file, line_no, opt
      );
    switch ( opt ) {
      case 'a': opt_alias             = optarg;               break;
      case 'c': opt_conf_file         = optarg;               break;
      case 'C': opt_no_conf           = true;                 break;
      case 'd': opt_lead_dot_ignore   = true;                 break;
      case 'e': opt_eos_delimit       = true;                 break;
      case 'f': opt_fin               = optarg;         /* no break; */
      case 'F': opt_fin_name          = base_name( optarg );  break;
      case 'h': opt_hang_tabs         = check_atou( optarg ); break;
      case 'H': opt_hang_spaces       = check_atou( optarg ); break;
      case 'i': opt_indt_tabs         = check_atou( optarg ); break;
      case 'I': opt_indt_spaces       = check_atou( optarg ); break;
      case 'm': opt_mirror_tabs       = check_atou( optarg ); break;
      case 'M': opt_mirror_spaces     = check_atou( optarg ); break;
      case 'n': opt_newlines_delimit  = INT_MAX;              break;
      case 'N': opt_newlines_delimit  = 1;                    break;
      case 'o': opt_fout              = optarg;               break;
      case 'p': opt_para_delimiters   = optarg;               break;
      case 's': opt_tab_spaces        = check_atou( optarg ); break;
      case 'S': opt_lead_spaces       = check_atou( optarg ); break;
      case 't': opt_lead_tabs         = check_atou( optarg ); break;
      case 'T': opt_title_line        = true;                 break;
      case 'v': fprintf( stderr, "%s\n", PACKAGE_STRING );  exit( EXIT_OK );
      case 'l': /* deprecated: now synonym for -w */
      case 'w': opt_line_width        = check_atou( optarg ); break;
      case 'b': /* deprecated: now synonym for -W */
      case 'W': opt_lead_ws_delimit   = true;                 break;
      default : usage();
    } /* switch */
  } /* while */
}

static void usage() {
  fprintf( stderr,
"usage: %s [-CdenNTvW] [-w line-width] [-p para-delim-chars] [-s tab-spaces]\n"
"       [-{fF} input-file] [-o output-file] [-c conf-file] [-a alias]\n"
"       [-t leading-tabs]  [-S leading-spaces]\n"
"       [-m mirror-tabs]   [-M mirror-spaces]\n"
"       [-i indent-tabs]   [-I indent-spaces]\n"
"       [-h hanging-tabs]  [-H hanging-spaces]\n"
    "", me
  );
  exit( EXIT_USAGE );
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
