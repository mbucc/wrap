#define VERSION "2.9"
/*
**	wrapc -- comment reformatter
**	wrapc.c: implementation
**
**	Copyright (C) 1996  Paul J. Lucas
**
**	This program is free software; you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation; either version 2 of the Licence, or
**	(at your option) any later version.
** 
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
** 
**	You should have received a copy of the GNU General Public License
**	along with this program; if not, write to the Free Software
**	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* system */
#include <stdlib.h>			/* for exit() */
#include <stdio.h>
#include <string.h>			/* for str...() */
#include <unistd.h>			/* for close(), fork(), ... */

/* local */
#include "c_compat.h"

#define	BUF_SIZE	1000		/* hopefully, no one will exceed this */
#define	TAB_EQUAL	8		/* tabs are equal to 8 spaces */

#define	ERROR( e )	{ perror( argv[0] ); exit( e ); }
#define	CLOSE( p )	{ close( pipes[p][0] ); close( pipes[p][1] ); }
#define	REDIRECT( s,p )	{ close( s ); dup( pipes[p][s] ); CLOSE( p ); }

char const	*me;
int		text_length = 80;	/* wrap text to this length */

void	process_options PJL_PROTO(( int argc, char *argv[] ));

/*************************/
	int
main PJL_ARG_LIST(( argc, argv ))
	PJL_ARG_DEF( int argc )
	PJL_ARG_END( char *argv[] )
{
	char buf[ BUF_SIZE ];
	/*
	** Two pipes: pipes[0] goes between child 1 and child 2
	**            pipes[1] goes between child 2 and parent
	*/
	int pipes[2][2];
	int pid;			/* child process-id */
	FILE *from_wrap;		/* file used by parent */

	/*
	** Read the first line of input and obtain a string of leading
	** characters to be removed from all lines.
	**
	** The characters are: spaces and tabs, # (shell comments), / and * (C
	** and C++ comments), % (PostScript comments), ; (assember and Lisp
	** comments), and > (mail forward indicator).
	*/
	char *leading_chars = "\t #%*/;>";
	char leader[ BUF_SIZE ];	/* string segment removed/prepended */
	int lead_length;		/* number of leading characters */

	process_options( argc, argv );

	if ( !fgets( buf, BUF_SIZE, stdin ) )
		exit( 0 );
	strcpy( leader, buf );
	leader[ lead_length = strspn( buf, leading_chars ) ] = '\0';

	/* open pipes */
	if ( pipe( pipes[0] ) == -1 || pipe( pipes[1] ) == -1 )
		ERROR( 1 );

	/*********************************************************************/
	/*
	** Child 1
	** 
	** Close both ends of pipes[1] since it doesn't use it; read from our
	** original stdin and write to pipes[0].
	**
	** Print first an all subsequent lines with the leading characters
	** removed from the beginning of each line.  
	*/
	if ( ( pid = fork() ) == 0 ) {
		FILE *to_wrap;
		CLOSE( 1 );
		if ( !( to_wrap = fdopen( pipes[0][1], "w" ) ) ) {
			fprintf( stderr, "%s: child can't write to pipe\n", me);
			exit( -11 );
		}

		fputs( buf + lead_length, to_wrap );
		while ( fgets( buf, BUF_SIZE, stdin ) )
			fputs( buf + lead_length, to_wrap );
		exit( 0 );
	} else if ( pid == -1 )
		ERROR( -10 );

	/*********************************************************************/
	/*
	** Child 2
	**
	** Compute the actual length of the leader: tabs are equal to 8 spaces
	** and all other are equal to 1.  Subtract this from 80 to obtain the
	** wrap length.
	**
	** Read from pipes[0] and write to pipes[1]; exec into wrap.
	*/
	if ( ( pid = fork() ) == 0 ) {
		char *c, s[4];

		for ( c = leader; *c; ++c )
			if ( *c == '\t' )
				text_length -= TAB_EQUAL;
			else
				--text_length;
		sprintf( s, "%d", text_length );

		REDIRECT( 0, 0 );
		REDIRECT( 1, 1 );
		execlp( "wrap", "wrap", "-l", s, (char*)0 );
		ERROR( -21 );
	} else if ( pid == -1 )
		ERROR( -20 );

	/*********************************************************************/
	/*
	** Parent
	**
	** Close both ends of pipes[0] since it doesn't use it; close the
	** write end of pipes[1].
	**
	** Read from pipes[1] (the output from wrap) and prepend the leading
	** text to each line.
	*/
	CLOSE( 0 );
	close( pipes[1][1] );

	from_wrap = fdopen( pipes[1][0], "r" );
	if ( !from_wrap ) {
		fprintf( stderr, "%s: parent can't read from pipe\n", me );
		exit( 1 );
	}

	while ( fgets( buf, BUF_SIZE, from_wrap ) )
		printf( "%s%s", leader, buf );
	exit( 0 );
}

/*************************/
	void
process_options PJL_ARG_LIST(( argc, argv ))
	PJL_ARG_DEF( int argc )
	PJL_ARG_END( char *argv[] )
{
	extern char *optarg;
	extern int optind, opterr;
	int opt;			/* command-line option */

	/* strip pathname from argv[0] */
	if ( me = strrchr( argv[0], '/' ) )
		++me;
	else
		me = argv[0];

#define	SET_OPT( c, option )	case c: option = atoi( optarg ); break

	opterr = 1;
	while ( ( opt = getopt( argc, (char**)argv, "l:v" ) ) != EOF )
		switch ( opt ) {
			SET_OPT( 'l', text_length );
			case 'v': goto version;
			case '?': goto usage;
		}
	argc -= optind, argv += optind;
	if ( argc )
		goto usage;
	return;

version:
	fprintf( stderr, "%s: version %s\n", me, VERSION );
	exit( 0 );

usage:
	fprintf( stderr, "usage: %s [-l text-length]\n", me );
	exit( 1 );
}