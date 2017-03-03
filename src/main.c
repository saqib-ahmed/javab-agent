/*
 * Copyright (c) 2017, Al-Khawarizmi Institute of Computer Sciences
 * and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact KICS, G.T. Road UET, Lahore, Pakistan or visit www.kics.edu.pk
 * if you need additional information or have any questions.
 *
 */

/* ********************************************************
 *** INCLUDE FILES and DEFINITIONS                    ***
 ********************************************************  */

#include <stdarg.h>
#include <unistd.h>
#include "class.h"

#ifdef __cplusplus
extern "C" {
#endif

//#undef AUTO_QUERY

/* ********************************************************
 *** EXTERNAL VARIABLES                               ***
 ******************************************************** */

/* PUBLIC
 ****** */

u1_int s_disassem = 0u, s_basic = 0u, s_par = 0u;
u1_int s_query = 0u, s_linking = 0u, s_nest = 0u, s_stack = 0u;

u1_int my_not = 8u;
u1_int error_1 = 0u;
u4_int n_par = 0u, n_nest = 0u, n_loop = 0u, n_triv = 0u, class_len = 0;

int worker_flag = 0u;
unsigned char* new_class_ptr = NULL;
char* PATH = NULL;
/* PRIVATE
 ******* */

static u1_int s_csum = 0u, s_verbose = 0u, s_output = 0u;
static u1_int tot_err = 0u;
static u4_int files = 0u;
static const unsigned char *class_file = NULL;
static char *filename = NULL;
static char *version = "1.0.7BETA";

/* ********************************************************
 *** PRIVATE FUNCTIONS                                ***
 ******************************************************** */

static void output_internals(void) {

	javab_out(1, "*** javab version %s", version);

	javab_out(1, "*** char=%u,short=%u,int=%u,long=%u,float=%u,double=%u"
			" (bits in char=%u)", sizeof(char), sizeof(short), sizeof(int),
			sizeof(long), sizeof(float), sizeof(double), CHAR_BIT);

	javab_out(1, "*** 1-byte data type = %s", U1);
	javab_out(1, "*** 2-byte data type = %s", U2);
	javab_out(1, "*** 4-byte data type = %s", U4);

#ifdef GIVE_ALL
	javab_out(1, "UCHAR_MAX=%u,USHRT_MAX=%u",
			UCHAR_MAX, USHRT_MAX);
	javab_out(1, "UINT_MAX=%u,ULONG_MAX=%lu",
			UINT_MAX, ULONG_MAX);

#ifdef ULONGLONG_MAX
	javab_out(1, "long long=%u", sizeof(long long));
	javab_out(1, "ULONGLONG_MAX=%llu", ULONGLONG_MAX);
#endif

	{ /* Check byte ordering of target machine */

		u4_int k = 1;
		u1_int *buf = (u1_int *) &k;

		if (buf[0] == 1)
		javab_out(1, "target machine uses little-endian byte order");
		else
		javab_out(1, "target machine uses big-endian byte order");
	}
#endif
}

/* Move original  'file.ext' to 'file.old'
 and open a new 'file.ext' for output
 ************************************************* */

/* Process a Class File
 ******************** */

static void process(void) {

	files++;
	error_1 = 0;

	javab_out(2, "--- processing %s", (filename) ? filename : "stdin");
#if DEBUG>1
	char buf[100];
	filename = (char*)malloc(100);
	sprintf(buf, "~~debug_class_%d~~.class", class_len);
	strcpy(filename, buf);
#else
	filename = NULL;
#endif

	process_classfile(class_file, 0u); /* read   */
	process_classfile(NULL, 1u); /* verify */

	make_shadow_cp();

	if (!error_1) {

		if (s_csum) {
			javab_out(2, " -- class file summary output");
			process_classfile(NULL, 2u);
		}

		javab_out(2, " -- processing bytecode");

		byte_proc();
	}

	/* Unparsing */

	if (s_output) {

		if (error_1) {
			javab_out(1, "no output for %s", (filename) ? filename : "stdin");
		} else {

			if (filename) {

				FILE *outfile = fopen(filename, "w");

				if (outfile) {
					dump_classfile(outfile);
					fclose(outfile);
				}
			} else {
				javab_out(2, " -- output to file stdout");
				dump_classfile(stdout);
			}

		}
	}

	take_shadow_cp();

	/* Delete Internal Representation of current Class File */

	process_classfile(NULL, 3u);

	/* Output of all Worker Classes for current Class File */

	if ((!error_1) && (s_output))
		output_workers(filename);

	elim_shadow_cp();
}

/* ********************************************************
 *** PUBLIC FUNCTIONS                                 ***
 ******************************************************** */

/* ****************************
 *** User input on Query  ***
 ****************************
 *** y/Y : yes            ***
 *** n/N : no             ***
 *** q/Q : no, quit query ***
 **************************** */

u1_int query(void) {
	char str[80];
	u1_int res = 2u;

#ifdef AUTO_QUERY
	fprintf(stderr, "(y/n/q) => Y\n");
	return 1u;
#endif

	do {

		fprintf(stderr, "(y/n/q) => ");
		fflush(stderr);

		fgets(str, 80, stdin);

		if (strlen(str) != 0)
			switch (str[0]) {

			case 'y':
			case 'Y':
				res = 1u;
				break;

			case 'n':
			case 'N':
				res = 0u;
				break;

			case 'q':
			case 'Q':
				s_query = 0u;
				res = 0u;
				break;
			}
	} while (res == 2u);

	return res;
}

/* **********************************
 *** Error and Warning Messages ***
 ***************************************************************
 *** level == -1 : FATAL ERROR,     EXIT PROGRAM             ***
 *** level ==  0 : ERRROR,          SET ERROR FLAG           ***
 *** level ==  1 : STRONG MESSAGE                            ***
 *** level ==  2 : MESSAGE,         PRINT ONLY FOR '-v'      ***
 *************************************************************** */

void javab_out(s1_int level, char *fmt, ...) {
	va_list argv;

	if (level == 0)
		tot_err = error_1 = 1;
	else if (level > 1 && !s_verbose)
		return;

	va_start(argv, fmt);

	if (class_file)
		fprintf(stderr, "%s:: ", (filename) ? filename : "stdin");

	vfprintf(stderr, fmt, argv);
	putc('\n', stderr);

	va_end(argv);

	if (level < 0)
		exit(1);
}

/* Memory Allocation Functions
 [for size <= 0, size == 1 is used
 because some systems return NULL for malloc(0);]
 ************************************************* */

void *make_mem(int size) {
	void *p = calloc(((size > 0) ? size : 4), sizeof(u1_int));
	if (!p)
		javab_out(-1, "Out of Memory");
	return p;
}

void *more_mem(void *p, int size) {
	if (p) {
		p = realloc(p, ((size > 0) ? size : 1));
		if (!p)
			javab_out(-1, "Out of Memory (re-allocation)");
		return p;
	} else
		return make_mem(size);
}
/* ********************
 *** Main Program ***
 ******************** */

void javab_main(int argc, char *argv,
		const unsigned char* class_bytes, int class_data_len) {
	int i;
	class_len = class_data_len;
	/* Process Environment Variable */


	my_not = (u1_int)sysconf(_SC_NPROCESSORS_ONLN);

//	char *env = getenv("JAVAB_THREADS");
//
//	if (env) {
//		my_not = (u1_int) atoi(env);
//
//		if (my_not < 2u)
//			my_not = 8u;
//		else if (my_not > 16u)
//			my_not = 16u;
//	}

	/* Process Optional Flags in Command-line Arguments */

	for (i = 0; i < argc; i++) {

		char c = argv[i];

		switch (c) {

		case 'b':
			s_basic = 1u;
			break;

		case 'c':
			s_csum = 1u;
			break;

		case 'd':
			s_disassem = 1u;
			break;

		case 'i':
			output_internals();
			break;

		case 'l':
			s_linking = 1u;
			break;

		case 'n':
			s_nest = 1u;
			break;

		case 'o':
			s_output = 1u;
			break;

		case 'p':
			s_par = 1u;
			break;

		case 'q':
			s_query = 1u;
			break;

		case 's':
			s_stack = 1u;
			break;

		case 'v':
			s_verbose = 1u;
			break;

		default:
			javab_out(-1, "javab version %s :: "
					"usage: %s [-bcdlnopqsv] [file ...]", version, argv[0]);
		}

	}

	javab_out(2, "*** javab version %s "
			"[written by Aart J.C. Bik (C) 1997]", version);

	class_file = class_bytes;

	process();

	if (s_par) {
		class_file = NULL;
		javab_out(2, "*** FILES: %u, THREADS: %u, NATURAL LOOPS: %u"
				" (TRIV: %u => SER: %u, PAR: %u, NESTED: %u)", files, my_not,
				n_loop, n_triv, ((n_triv - n_par) - n_nest), n_par, n_nest);
	}
	n_loop = 0;
	n_triv=0;
	n_par=0;
	n_nest=0;
}

#ifdef __cplusplus
}
#endif
