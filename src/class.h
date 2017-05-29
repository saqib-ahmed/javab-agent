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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Control Switches
#define LOG 0
#define DEBUG 1
#define DEBUG_THREADS 0
#define Analyze_only_hot_method 1 
//#define DEBUG_LOOP
#define AUTO_QUERY
#define COMP_FLAG && compiled_loaded_flag==2		//Uncomment for profiler feedback


#define MAX(a,b)   (((a)>=(b)) ? (a) : (b))
#define MIN(a,b)   (((a)<=(b)) ? (a) : (b))

/* *************************************
   *** JVM-specific Type Definitions ***
   *************************************
   *** u1_int == unsigned 8  bits    ***
   *** s1_int ==   signed 8  bits    ***
   *************************************
   *** u2_int == unsigned 16 bits    ***
   *** s2_int ==   signed 16 bits    ***
   *************************************
   *** u4_int == unsigned 32 bits    ***
   *** s4_int ==   signed 32 bits    ***
   ************************************* */

#define U1MAX 255u
#define U2MAX 65535u
#define U4MAX 4294967295u
#define S4MAX 2147483647

#if ((CHAR_BIT == 8u) && (UCHAR_MAX == U1MAX))
#define U1 "char"
typedef unsigned char u1_int;
typedef signed   char s1_int;
#endif

#if (USHRT_MAX == U2MAX)
#define U2 "short int"
typedef unsigned short int u2_int;
typedef signed   short int s2_int;
#elif (UINT_MAX == U2MAX)
#define U2 "int"
typedef unsigned int u2_int;
typedef signed   int s2_int;
#endif

#if (UINT_MAX == U4MAX)
#define U4 "int"
typedef unsigned int u4_int;
typedef signed   int s4_int;
#elif (ULONG_MAX == U4MAX)
#define U4 "long int"
typedef unsigned long int u4_int;
typedef signed   long int s4_int;
#elif (ULONGLONG_MAX == U4MAX)
#define U4 "long long int"
typedef unsigned long long int u4_int;
typedef signed   long long int s4_int;
#endif

/* byte <-> int conversions (JVM uses big-endian order) 
   **************************************************** */

#define B2U2(b1,b2)       ((((u2_int)(b1))<<8)+((u2_int)(b2)))
#define B2U4(b1,b2,b3,b4) ((((u4_int)(B2U2((b1),(b2)))<<16)) + \
			    ((u4_int)(B2U2((b3),(b4)))))
#define B2S2(b1,b2)       ((s2_int)(B2U2((b1),(b2))))
#define B2S4(b1,b2,b3,b4) ((s4_int)(B2U4((b1),(b2),(b3),(b4))))

#define LOWB_U2(u)        ((u1_int)( (u)        & 0xff))
#define HIGB_U2(u)        ((u1_int)(((u) >> 8)  & 0xff))
#define LOWB_U4(u)        ((u1_int)(((u) >> 16) & 0xff))
#define HIGB_U4(u)        ((u1_int)(((u) >> 24) & 0xff))

/* ********************************************
   *** Class-file-specific Type Definitions ***
   ******************************************** */

/* Access and Modifier Flags 
   ************************* */

#define ACC_PUBLIC       0x0001
#define ACC_PRIVATE      0x0002
#define ACC_PROTECTED    0x0004
#define ACC_STATIC       0x0008
#define ACC_FINAL        0x0010
#define ACC_SUPER        0x0020
#define ACC_SYNCHRONIZED 0x0020 /* overloaded */
#define ACC_VOLATILE     0x0040
#define ACC_TRANSIENT    0x0080
#define ACC_NATIVE       0x0100
#define ACC_INTERFACE    0x0200
#define ACC_ABSTRACT     0x0400

/* Constant Pool Tags
   ****************** */

#define CONSTANT_Utf8                1u
#define CONSTANT_Integer             3u
#define CONSTANT_Float               4u
#define CONSTANT_Long                5u
#define CONSTANT_Double              6u
#define CONSTANT_Class               7u
#define CONSTANT_String              8u
#define CONSTANT_Fieldref            9u
#define CONSTANT_Methodref          10u
#define CONSTANT_InterfaceMethodref 11u
#define CONSTANT_NameAndType        12u

/* Constant Pool Entries 
   ********************* */

struct constant_node {
   union {
     struct {
       u4_int val1;
       u4_int val2;
     } data;
     struct {
       u2_int index1;
       u2_int index2;
     } indices;
     struct {
       u1_int *s;
       u2_int  l;
     } utf8;
   } u ;

   u1_int tag;
} ;

typedef struct constant_node *constant_ptr;

/* Attribute Entries 
   ***************** */

struct attribute_node {
 u2_int  attribute_name_index;
 u4_int  attribute_length;
 u1_int *info;

 /* JAVAB specific information
    ************************** */

 u4_int               code_length;

 u1_int              *reachable;   /* 0u : unreachable
				      1u : reachable/visited
                                      2u : unvisited
				      *********************** */
 u1_int              *is_leader;
 u2_int              *sp_before;
 struct bb_node     **my_bb;
 struct state_node ***st_state;
} ;

typedef struct attribute_node *attribute_ptr;

/* Field/Method Entries 
   ******************** */

struct fm_node {
 u2_int         access_flags;
 u2_int         name_index;
 u2_int         descr_index;
 u2_int         attributes_count;
 attribute_ptr *attributes;
} ;

typedef struct fm_node *fm_ptr;

/* ***************************************
   *** JAVAB-specific Type Definitions ***
   *************************************** */

/* Stack State Component Types
   *************************** */

enum stack_states {
  S_BOT, S_EXP, S_REF
} ;

/* Local Variable Types
   ******************** */

enum types {
 TP_UNUSED, TP_2WORD, TP_INT, TP_LONG, TP_FLOAT, TP_DOUBLE, TP_REF, TP_ERROR
} ;

/* Small Nodes
   *********** */

struct array_node {

  struct array_node *next;

  /* Query Information */

  u1_int            *q;
  s4_int            *c;
  u4_int            *p;

  s4_int            *dep_l;
  s4_int            *dep_u;
  u1_int            *dep_s;  /* 0u: unused
			        1u: used as loop index
				2u: loop-carried
				********************** */
  /* Other Information */

  u4_int             dim_ad;
  u2_int             dim_loc;
  u1_int             dim;
  u1_int             lhs;
} ;

typedef struct array_node *array_ptr;

struct ref_node {

  struct ref_node   *gnext;

  struct ref_node   *next;
  struct state_node *rf;
  struct state_node *in;
  u4_int             ad;
  u1_int             lhs;
} ;

typedef struct ref_node *ref_ptr;

/* Natural Loops
   ************* */

struct loop_node {

  struct   loop_node *next;

  /* Loop Info
     ********* */

  struct   bb_node    *b;
  struct   bb_node    *d;
  struct   bb_node   **nl;      /* nodes in natural loop
	                           (defined by back-edge)
		                   ********************** */
  struct   state_node *compare;
  struct   state_node *up_bnd;
  struct   state_node *lw_bnd;
  u1_int               strict;  /* index/bound information
				   ************************ */
  struct   ref_node   *refs;
  struct   array_node *array;   /* array information
			  	   ***************** */
  u1_int  *load_type;
  char   **load_sig;
  u2_int   load_locs;           /* local var. usage
			           **************** */
  u4_int   min_ad;
  u4_int   max_ad;
  u4_int   exit_ad;
  u4_int   cmp_ad;              /* address information
		                   ******************* */
  u1_int   ind_is_w;
  u2_int   ind_step;
  u2_int   ind_loc;
  u4_int   ind_add;             /* trivial loop index information
                                   ******************************* */
  u4_int   cnt;
  u1_int   triv;
  u1_int   par;                 /* loop information
		                   **************** */
} ;

typedef struct loop_node *loop_ptr;

/* Workers
   ******* */

struct worker_node {

  struct worker_node *next;

  /* Worker Fields
     ************* */

  char    *qualified_name;
  char    *constr_d;

  u2_int  *load_ind;
  u1_int  *load_type;
  char   **load_sig;   /* Local Usage Information
			  *********************** */
  u1_int* loop_code;
  u4_int  l_len;       /* Loop-Body
		 	  ********* */
  u1_int  ind_is_w;
  u2_int  ind_step;
  u4_int  ind_off;     /* iinc Information
		          **************** */
  u4_int  entry_off;
  u4_int  exit_off;
  u2_int  max_stack;
  u2_int  max_locals;  /* Additional Information
		 	  ********************** */
} ;

typedef struct worker_node *worker_ptr;

/* Stack States
   ************ */

struct state_node {

  struct state_node *gnext;

  union {

    struct {
      char   *sig;
      u4_int  ad;
      u2_int  loc;
      u1_int  d, d2, set;
    } ref;

    struct {
     u1_int *rd;
     s4_int  con;
    } exp;

  } u;

  u1_int prop;
  u1_int kind;
} ;

typedef struct state_node *state_ptr;

/* Basic Blocks
   ************ */

struct bbh_node {

 struct   bb_node  *head;
 struct   bbh_node *tail;

 loop_ptr loop;
 u1_int   exc;    /* exception flag */
} ;

typedef struct bbh_node *bbh_ptr;

struct bb_node {

  struct  bb_node *gnext;

  /* Data Flow Information
     ********************* */

  u1_int *dominators;

  u1_int *rd_gen;
  u1_int *rd_kill;
  u1_int *rd_in;
  u1_int *rd_out;

  u1_int *upw_use;

  state_ptr *s_gen;
  state_ptr *s_in;
  state_ptr *s_out;

  /* Control Flow Information
     ************************ */

  struct  bbh_node *pred;
  struct  bbh_node *succ;

  u4_int  name;
  u4_int  low, high;

  u1_int  visited;
} ;

typedef struct bb_node *bb_ptr;


/* *****************************************
   *** Prototypes and External Variables ***
   ***************************************** */

/* main.c 
   ****** */
extern u1_int my_not;
extern u1_int s_disassem, s_basic, s_par, s_stack;
extern u1_int s_linking,  s_query, s_nest;
extern u1_int error_1;
extern u4_int n_par,      n_nest,  n_loop, n_triv, class_len;
extern u1_int* new_class_ptr;
extern int worker_flag;
extern char* PATH;
extern const char* HOT_METHOD;
u1_int query(void);
void   javab_out(s1_int, char *, ...);
void  *make_mem(int);
void  *more_mem(void *, int);
int check_valid_CP(const unsigned char*, int );
void javab_main(int, char* argv, const unsigned char*, int , const char *);

/* class.c 
   ******* */

extern u4_int         magic;
extern u2_int         minor_version, major_version;
extern u2_int         constant_pool_count;
extern constant_ptr  *constant_pool;
extern u2_int         access_flags, this_class, super_class;
extern u2_int         interfaces_count;
extern u2_int        *interfaces;
extern u2_int         fields_count;
extern fm_ptr        *fields;
extern u2_int         methods_count;
extern fm_ptr        *methods;
extern u2_int         attributes_count;
extern attribute_ptr *attributes;

void          make_shadow_cp(void);
void          mark_shadow_cp(u2_int);
void          take_shadow_cp(void);
void          dump_shadow_cp(void);
void          elim_shadow_cp(void);

void          process_classfile(const unsigned char *, u1_int);
u1_int        valid_cp_entry(u1_int, u2_int, char *);
void          show_cp_entry(constant_ptr);

attribute_ptr new_attribute(void);

void          add_cp_entry(u1_int, char *, u2_int, u2_int);
void          add_field(fm_ptr);
void          add_method(fm_ptr);

/* byte.c 
   ****** */

state_ptr new_stack_state(u1_int, u4_int, s4_int);
void      check_triv_loop(loop_ptr);
void      byte_proc(void);

/* basic.c
   ******* */

void      nop_loop(attribute_ptr, loop_ptr, u1_int *);

void      bb_link_sub_back(u1_int *, bb_ptr, bb_ptr);
bb_ptr    bb_add(u2_int, u2_int, u1_int);
void      bb_add_pred(bb_ptr, bb_ptr, u1_int);
void      bb_add_succ(bb_ptr, bb_ptr, u1_int);

void      bb_first(u2_int, u2_int *, u4_int *, char  **, u2_int,
	           u2_int, u2_int *, u4_int *, u1_int *, u2_int);
void      bb_second(u1_int *);
void      bb_par(attribute_ptr, u1_int *, u1_int *);
void      bb_delete(void);

void      dump_sta(state_ptr);

/* dump.c 
   ****** */
extern int global_pos;
void dump_classfile(FILE *);

/* par.c 
   ***** */
extern char** worker_array;
extern u4_int num_workers;
void output_workers(char *);
void parallelize_loop(attribute_ptr, loop_ptr, u1_int *);
