/* *************
 *** JAVAB ***
 ****************************************************
 *** Copyright (c) 1997                           ***
 *** Aart J.C. Bik             Indiana University ***
 *** All Rights Reserved                          ***
 ****************************************************
 *** Please refer to the LICENSE file distributed ***
 *** with this software for further details on    ***
 *** the licensing terms and conditions.          ***
 ***                                              ***
 *** Please, report all bugs, comments, etc.      ***
 *** to: ajcbik@extreme.indiana.edu               ***
 ****************************************************
 *** byte.c : bytecode manipulations
 ***
 ***
 *** Your courtesy in mentioning the use of this bytecode tool
 *** in any scientific work that presents results obtained
 *** by using (extensions or modifications of) the tool
 *** is highly appreciated.
 ***
 *** */

/* ********************************************************
 *** INCLUDE FILES and DEFINITIONS                    ***
 ********************************************************  */

#include "class.h"

#define CHECK_TABLE
#define GET_IT(a,b) if (valid_cp_entry((a), entry, (b))) {          \
                      n = constant_pool[entry] -> u.indices.index2; \
                      d = constant_pool[n]     -> u.indices.index2; \
                      s = constant_pool[d]     -> u.utf8.s;         \
                    }                                               \
		    else break; 
#define HAS_TARGET(b) (((b)>=1u)&&((b)<=3u))

/* ********************************************************
 *** EXTERNAL VARIABLES                               ***
 ******************************************************** */

/* PRIVATE
 ******* */

static u4_int max_seen;

/* Array Signatures
 **************** */

static char *array_sigs[] = { "[Ljava/lang/Object;", "[Z", "[C", "[F", "[D",
		"[B", "[S", "[I", "[J" };

/* Array Type Table
 **************** */

static char *newarray_tab[] = { "boolean", "char", "float", "double", "byte",
		"short", "int", "long" };

/* global information 
 ****************** */

static attribute_ptr att;

static u4_int len;
static u1_int *byt, opc, bra, exc;
static u2_int pre, pos;
static char *mem;

static u1_int is_wide, reportu;
static u1_int is_instm;

static u4_int target, next;

static u2_int glo_sta, glo_pad, glo_loc, glo_stm;
static s4_int glo_def, glo_npa, glo_low, glo_hig;

static u2_int cur_sp;
static bb_ptr cur_bb;

/* reaching definitions and uses
 ***************************** */

static u1_int rd_pass;

static u2_int *rd_loc = NULL;
static u4_int *rd_add = NULL;
static u2_int rd_cnt;

static char rd_buf[510];
static char *rd_sig[255]; /* fixed arrays */

static u2_int *use_loc = NULL;
static u4_int *use_add = NULL;
static u1_int *use_typ = NULL;
static u2_int use_cnt;

/* stack state information 
 *********************** */

static ref_ptr g_refs = NULL; /* supports g.c. of refs         */
static state_ptr g_states = NULL; /* supports g.c. of stack states */
static state_ptr *s_states = NULL;

/* copy and constant propagation count
 *********************************** */

static u1_int my_prop = 0u;
static u4_int cc_cnt = 0u;

/* loop information
 **************** */

static loop_ptr triv_lp = NULL;

/* bytecode table
 ************** */

static struct bytecode_node {

   u1_int opcode;     /* redundant verify field:
			 bytecode[i].opcode == i
			 *********************** */
   char  *mnemonic;

   u1_int operands;   /* 9 == lookup */
   u1_int stack_pre;  /* 9 == lookup */
   u1_int stack_post; /* 9 == lookup */
		      /* *********** */

   u1_int exception;  /* 0: no exception
			 1: pot. RUN-TIME exception
			 2: pot. RUN-TIME exception + has c.p.-entry
			 3: pot. LINKING exception
			 4: pot. pot. LINKING exception + has c.p.-entry
			 *********************************************** */

   u1_int branch;     /* 0: no branch,
			 ---------------------------
			 1: cond.   branch + target,
			 2: uncond. branch + target,
			 3: jsr/jsr_w      + target,
			 ---------------------------
			 4: special        + continue next
			 5: special        + no-continue next
			 ************************************ */
} bytecode[] ={

/*  ***--------------------------------------------> opcode
         **************----------------------------> mnemonic
                          **-----------------------> #operands      (in bytes)
                              **-------------------> stack     pre  (in words)
                                  **---------------> stack     post (in words)
                                      **-----------> exception
                                          **-------> branch     */
/*  ***  **************   **  **  **  **  **  */

  {   0, "nop",            0,  0,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {   1, "aconst_null",    0,  0,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {   2, "iconst_m1",      0,  0,  1,  0,  0  },
  {   3, "iconst_0",       0,  0,  1,  0,  0  },
  {   4, "iconst_1",       0,  0,  1,  0,  0  },
  {   5, "iconst_2",       0,  0,  1,  0,  0  },
  {   6, "iconst_3",       0,  0,  1,  0,  0  },
  {   7, "iconst_4",       0,  0,  1,  0,  0  },
  {   8, "iconst_5",       0,  0,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {   9, "lconst_0",       0,  0,  2,  0,  0  },
  {  10, "lconst_1",       0,  0,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  11, "fconst_0",       0,  0,  1,  0,  0  },
  {  12, "fconst_1",       0,  0,  1,  0,  0  },
  {  13, "fconst_2",       0,  0,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  14, "dconst_0",       0,  0,  2,  0,  0  },
  {  15, "dconst_1",       0,  0,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  16, "bipush",         1,  0,  1,  0,  0  },
  {  17, "sipush",         2,  0,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  18, "ldc",            1,  0,  1,  4,  0  },
  {  19, "ldc_w",          2,  0,  1,  4,  0  },
  {  20, "ldc2_w",         2,  0,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  21, "iload",          1,  0,  1,  0,  0  },
  {  22, "lload",          1,  0,  2,  0,  0  },
  {  23, "fload",          1,  0,  1,  0,  0  },
  {  24, "dload",          1,  0,  2,  0,  0  },
  {  25, "aload",          1,  0,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  26, "iload_0",        0,  0,  1,  0,  0  },
  {  27, "iload_1",        0,  0,  1,  0,  0  },
  {  28, "iload_2",        0,  0,  1,  0,  0  },
  {  29, "iload_3",        0,  0,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  30, "lload_0",        0,  0,  2,  0,  0  },
  {  31, "lload_1",        0,  0,  2,  0,  0  },
  {  32, "lload_2",        0,  0,  2,  0,  0  },
  {  33, "lload_3",        0,  0,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  34, "fload_0",        0,  0,  1,  0,  0  },
  {  35, "fload_1",        0,  0,  1,  0,  0  },
  {  36, "fload_2",        0,  0,  1,  0,  0  },
  {  37, "fload_3",        0,  0,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  38, "dload_0",        0,  0,  2,  0,  0  },
  {  39, "dload_1",        0,  0,  2,  0,  0  },
  {  40, "dload_2",        0,  0,  2,  0,  0  },
  {  41, "dload_3",        0,  0,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  42, "aload_0",        0,  0,  1,  0,  0  },
  {  43, "aload_1",        0,  0,  1,  0,  0  },
  {  44, "aload_2",        0,  0,  1,  0,  0  },
  {  45, "aload_3",        0,  0,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  46, "iaload",         0,  2,  1,  1,  0  },
  {  47, "laload",         0,  2,  2,  1,  0  },
  {  48, "faload",         0,  2,  1,  1,  0  },
  {  49, "daload",         0,  2,  2,  1,  0  },
  {  50, "aaload",         0,  2,  1,  1,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  51, "baload",         0,  2,  1,  1,  0  },
  {  52, "caload",         0,  2,  1,  1,  0  },
  {  53, "saload",         0,  2,  1,  1,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  54, "istore",         1,  1,  0,  0,  0  },
  {  55, "lstore",         1,  2,  0,  0,  0  },
  {  56, "fstore",         1,  1,  0,  0,  0  },
  {  57, "dstore",         1,  2,  0,  0,  0  },
  {  58, "astore",         1,  1,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  59, "istore_0",       0,  1,  0,  0,  0  },
  {  60, "istore_1",       0,  1,  0,  0,  0  },
  {  61, "istore_2",       0,  1,  0,  0,  0  },
  {  62, "istore_3",       0,  1,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  63, "lstore_0",       0,  2,  0,  0,  0  },
  {  64, "lstore_1",       0,  2,  0,  0,  0  },
  {  65, "lstore_2",       0,  2,  0,  0,  0  },
  {  66, "lstore_3",       0,  2,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  67, "fstore_0",       0,  1,  0,  0,  0  },
  {  68, "fstore_1",       0,  1,  0,  0,  0  },
  {  69, "fstore_2",       0,  1,  0,  0,  0  },
  {  70, "fstore_3",       0,  1,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  71, "dstore_0",       0,  2,  0,  0,  0  },
  {  72, "dstore_1",       0,  2,  0,  0,  0  },
  {  73, "dstore_2",       0,  2,  0,  0,  0  },
  {  74, "dstore_3",       0,  2,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  75, "astore_0",       0,  1,  0,  0,  0  },
  {  76, "astore_1",       0,  1,  0,  0,  0  },
  {  77, "astore_2",       0,  1,  0,  0,  0  },
  {  78, "astore_3",       0,  1,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  79, "iastore",        0,  3,  0,  1,  0  },
  {  80, "lastore",        0,  4,  0,  1,  0  },
  {  81, "fastore",        0,  3,  0,  1,  0  },
  {  82, "dastore",        0,  4,  0,  1,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  83, "aastore",        0,  3,  0,  1,  0  },
  {  84, "bastore",        0,  3,  0,  1,  0  },
  {  85, "castore",        0,  3,  0,  1,  0  },
  {  86, "sastore",        0,  3,  0,  1,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  87, "pop",            0,  1,  0,  0,  0  },
  {  88, "pop2",           0,  2,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  89, "dup",            0,  1,  2,  0,  0  },
  {  90, "dup_x1",         0,  2,  3,  0,  0  },
  {  91, "dup_x2",         0,  3,  4,  0,  0  },
  {  92, "dup2",           0,  2,  4,  0,  0  },
  {  93, "dup2_x1",        0,  3,  5,  0,  0  },
  {  94, "dup2_x2",        0,  4,  6,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  95, "swap",           0,  2,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  {  96, "iadd",           0,  2,  1,  0,  0  },
  {  97, "ladd",           0,  4,  2,  0,  0  },
  {  98, "fadd",           0,  2,  1,  0,  0  },
  {  99, "dadd",           0,  4,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 100, "isub",           0,  2,  1,  0,  0  },
  { 101, "lsub",           0,  4,  2,  0,  0  },
  { 102, "fsub",           0,  2,  1,  0,  0  },
  { 103, "dsub",           0,  4,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 104, "imul",           0,  2,  1,  0,  0  },
  { 105, "lmul",           0,  4,  2,  0,  0  },
  { 106, "fmul",           0,  2,  1,  0,  0  },
  { 107, "dmul",           0,  4,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 108, "idiv",           0,  2,  1,  1,  0  },
  { 109, "ldiv",           0,  4,  2,  1,  0  },
  { 110, "fdiv",           0,  2,  1,  0,  0  },
  { 111, "ddiv",           0,  4,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 112, "irem",           0,  2,  1,  1,  0  },
  { 113, "lrem",           0,  4,  2,  1,  0  },
  { 114, "frem",           0,  2,  1,  0,  0  },
  { 115, "drem",           0,  4,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 116, "ineg",           0,  1,  1,  0,  0  },
  { 117, "lneg",           0,  2,  2,  0,  0  },
  { 118, "fneg",           0,  1,  1,  0,  0  },
  { 119, "dneg",           0,  2,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 120, "ishl",           0,  2,  1,  0,  0  },
  { 121, "lshl",           0,  3,  2,  0,  0  },
  { 122, "ishr",           0,  2,  1,  0,  0  },
  { 123, "lshr",           0,  3,  2,  0,  0  },
  { 124, "iushr",          0,  2,  1,  0,  0  },
  { 125, "lushr",          0,  3,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 126, "iand",           0,  2,  1,  0,  0  },
  { 127, "land",           0,  4,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 128, "ior",            0,  2,  1,  0,  0  },
  { 129, "lor",            0,  4,  2,  0,  0  },
  { 130, "ixor",           0,  2,  1,  0,  0  },
  { 131, "lxor",           0,  4,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 132, "iinc",           2,  0,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 133, "i2l",            0,  1,  2,  0,  0  },
  { 134, "i2f",            0,  1,  1,  0,  0  },
  { 135, "i2d",            0,  1,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 136, "l2i",            0,  2,  1,  0,  0  },
  { 137, "l2f",            0,  2,  1,  0,  0  },
  { 138, "l2d",            0,  2,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 139, "f2i",            0,  1,  1,  0,  0  },
  { 140, "f2l",            0,  1,  2,  0,  0  },
  { 141, "f2d",            0,  1,  2,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 142, "d2i",            0,  2,  1,  0,  0  },
  { 143, "d2l",            0,  2,  2,  0,  0  },
  { 144, "d2f",            0,  2,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 145, "i2b",            0,  1,  1,  0,  0  },
  { 146, "i2c",            0,  1,  1,  0,  0  },
  { 147, "i2s",            0,  1,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 148, "lcmp",           0,  4,  1,  0,  0  },
  { 149, "fcmpl",          0,  2,  1,  0,  0  },
  { 150, "fcmpg",          0,  2,  1,  0,  0  },
  { 151, "dcmpl",          0,  4,  1,  0,  0  },
  { 152, "dcmpg",          0,  4,  1,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 153, "ifeq",           2,  1,  0,  0,  1  },
  { 154, "ifne",           2,  1,  0,  0,  1  },
  { 155, "iflt",           2,  1,  0,  0,  1  },
  { 156, "ifge",           2,  1,  0,  0,  1  },
  { 157, "ifgt",           2,  1,  0,  0,  1  },
  { 158, "ifle",           2,  1,  0,  0,  1  },

/*  ***  **************   **  **  **  **  **  */

  { 159, "if_icmpeq",      2,  2,  0,  0,  1  },
  { 160, "if_icmpne",      2,  2,  0,  0,  1  },
  { 161, "if_icmplt",      2,  2,  0,  0,  1  },
  { 162, "if_icmpge",      2,  2,  0,  0,  1  },
  { 163, "if_icmpgt",      2,  2,  0,  0,  1  },
  { 164, "if_icmple",      2,  2,  0,  0,  1  },
  { 165, "if_acmpeq",      2,  2,  0,  0,  1  },
  { 166, "if_acmpne",      2,  2,  0,  0,  1  },

/*  ***  **************   **  **  **  **  **  */

  { 167, "goto",           2,  0,  0,  0,  2  },
  { 168, "jsr",            2,  0,  1,  0,  3  },
  { 169, "ret",            1,  0,  0,  0,  5  },

/*  ***  **************   **  **  **  **  **  */

  { 170, "tableswitch",    9,  1,  0,  0,  5  },
  { 171, "lookupswitch",   9,  1,  0,  0,  5  },

/*  ***  **************   **  **  **  **  **  */

  { 172, "ireturn",        0,  1,  0,  0,  5  },
  { 173, "lreturn",        0,  2,  0,  0,  5  },
  { 174, "freturn",        0,  1,  0,  0,  5  },
  { 175, "dreturn",        0,  2,  0,  0,  5  },
  { 176, "areturn",        0,  1,  0,  0,  5  },
  { 177, "return",         0,  0,  0,  0,  5  },

/*  ***  **************   **  **  **  **  **  */

  { 178, "getstatic",      2,  0,  9,  4,  0  },
  { 179, "putstatic",      2,  9,  0,  4,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 180, "getfield",       2,  1,  9,  2,  0  },
  { 181, "putfield",       2,  9,  0,  2,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 182, "invokevirtual",  2,  9,  9,  2,  4  },
  { 183, "invokespecial",  2,  9,  9,  2,  4  },
  { 184, "invokestatic",   2,  9,  9,  4,  4  },
  { 185, "invokeinterface",4,  9,  9,  2,  4  },

/*  ***  **************   **  **  **  **  **  */

  { 186, "invokedynamic",  2,  9,  9,  2,  4  },

/*  ***  **************   **  **  **  **  **  */

  { 187, "new",            2,  0,  1,  4,  0  },
  { 188, "newarray",       1,  1,  1,  1,  0  },
  { 189, "anewarray",      2,  1,  1,  2,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 190, "arraylength",    0,  1,  1,  1,  0   },
  { 191, "athrow",         0,  1,  0,  1,  5   },

/*  ***  **************   **  **  **  **  **  */

  { 192, "checkcast",      2,  1,  1,  2,  0   },
  { 193, "instanceof",     2,  1,  1,  4,  0   },

/*  ***  **************   **  **  **  **  **  */

  { 194, "monitorenter",   0,  1,  0,  1,  0   },
  { 195, "monitorexit",    0,  1,  0,  1,  0   },

/*  ***  **************   **  **  **  **  **  */

  { 196, "wide",           0,  0,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 197, "multianewarray", 3,  9,  1,  2,  0  },

/*  ***  **************   **  **  **  **  **  */

  { 198, "ifnull",         2,  1,  0,  0,  1  },
  { 199, "ifnonnull",      2,  1,  0,  0,  1  },

/*  ***  **************   **  **  **  **  **  */

  { 200, "goto_w",         4,  0,  0,  0,  2  },
  { 201, "jsr_w",          4,  0,  1,  0,  3  },

/*  ***  **************   **  **  **  **  **  */

    /* reserved opcode: break */

  { 202, "???",            0,  0,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

    /* _quick opcodes */

  { 203, "???",            0,  0,  0,  0,  0  },
  { 204, "???",            0,  0,  0,  0,  0  },
  { 205, "???",            0,  0,  0,  0,  0  },
  { 206, "???",            0,  0,  0,  0,  0  },
  { 207, "???",            0,  0,  0,  0,  0  },
  { 208, "???",            0,  0,  0,  0,  0  },
  { 209, "???",            0,  0,  0,  0,  0  },
  { 210, "???",            0,  0,  0,  0,  0  },
  { 211, "???",            0,  0,  0,  0,  0  },
  { 212, "???",            0,  0,  0,  0,  0  },
  { 213, "???",            0,  0,  0,  0,  0  },
  { 214, "???",            0,  0,  0,  0,  0  },
  { 215, "???",            0,  0,  0,  0,  0  },
  { 216, "???",            0,  0,  0,  0,  0  },
  { 217, "???",            0,  0,  0,  0,  0  },
  { 218, "???",            0,  0,  0,  0,  0  },
  { 219, "???",            0,  0,  0,  0,  0  },
  { 220, "???",            0,  0,  0,  0,  0  },
  { 221, "???",            0,  0,  0,  0,  0  },
  { 222, "???",            0,  0,  0,  0,  0  },
  { 223, "???",            0,  0,  0,  0,  0  },
  { 224, "???",            0,  0,  0,  0,  0  },
  { 225, "???",            0,  0,  0,  0,  0  },
  { 226, "???",            0,  0,  0,  0,  0  },
  { 227, "???",            0,  0,  0,  0,  0  },
  { 228, "???",            0,  0,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

    /* unused */

  { 229, "???",            0,  0,  0,  0,  0  },
  { 230, "???",            0,  0,  0,  0,  0  },
  { 231, "???",            0,  0,  0,  0,  0  },
  { 232, "???",            0,  0,  0,  0,  0  },
  { 233, "???",            0,  0,  0,  0,  0  },
  { 234, "???",            0,  0,  0,  0,  0  },
  { 235, "???",            0,  0,  0,  0,  0  },
  { 236, "???",            0,  0,  0,  0,  0  },
  { 237, "???",            0,  0,  0,  0,  0  },
  { 238, "???",            0,  0,  0,  0,  0  },
  { 239, "???",            0,  0,  0,  0,  0  },
  { 240, "???",            0,  0,  0,  0,  0  },
  { 241, "???",            0,  0,  0,  0,  0  },
  { 242, "???",            0,  0,  0,  0,  0  },
  { 243, "???",            0,  0,  0,  0,  0  },
  { 244, "???",            0,  0,  0,  0,  0  },
  { 245, "???",            0,  0,  0,  0,  0  },
  { 246, "???",            0,  0,  0,  0,  0  },
  { 247, "???",            0,  0,  0,  0,  0  },
  { 248, "???",            0,  0,  0,  0,  0  },
  { 249, "???",            0,  0,  0,  0,  0  },
  { 250, "???",            0,  0,  0,  0,  0  },
  { 251, "???",            0,  0,  0,  0,  0  },
  { 252, "???",            0,  0,  0,  0,  0  },
  { 253, "???",            0,  0,  0,  0,  0  },

/*  ***  **************   **  **  **  **  **  */

    /* reserved opcodes: impdep1 impdep1 */

  { 254, "???",            0,  0,  0,  0,  0  },
  { 255, "???",            0,  0,  0,  0,  0  }
} ;


/* ********************************************************
 *** PRIVATE FUNCTIONS                                ***
 ******************************************************** */

/* *********************************
 *** Global/Local DOM relation ***
 ********************************* */

static u1_int dominates(u4_int a1, u4_int a0) {
	bb_ptr b0 = att->my_bb[a0];
	bb_ptr b1 = att->my_bb[a1];

	if ((!b0) || (!b1))
		javab_out(-1, "lost information in dominates()");

	if ((b0->dominators[b1->name]) && ((b0->name != b1->name) || (a1 < a0)))
		return 1u;
	return 0u;
}

/* ***********************************
 *** Scalar Data Dependence Test ***
 *********************************** */

static void scalar_dep(void) {

	u4_int i, j; /* wide counters */

	if (!triv_lp)
		javab_out(-1, "incorrect invocation of scalar_dep()");

	/* Check Constraint (a) [BikGannon]
	 ******************************** */

	for (i = 0u; i < use_cnt; i++) {
		u2_int l0 = use_loc[i];
		u4_int a0 = use_add[i];
		u1_int tp = use_typ[i];
		bb_ptr v = att->my_bb[a0];

		if ((v) && (triv_lp->nl[v->name]) && (l0 != triv_lp->ind_loc)) {

			/* now, we have a use in v in loop, other than the loop-index */

			u2_int rd_out_loop = 0u;
			u2_int rdf_in_loop = 0u;
			u2_int dom_in_loop = 0u;
			u4_int one_a, one_j, one_dom_a, one_out_a;

			for (j = 0u; j < rd_cnt; j++)
				if (v->rd_in[j]) {
					u2_int l1 = rd_loc[j];
					u4_int a1 = rd_add[j];
					bb_ptr w = (a1 != U4MAX) ? att->my_bb[a1] : NULL;

					if (l0 == l1) {

						if ((w) && (triv_lp->nl[w->name])) {

							/* matching def. in w in loop reaching beginning of v */

							rdf_in_loop++;

							if (dominates(a1, a0)) {
								dom_in_loop++;
								one_dom_a = a1;
							} else {
								one_j = j;
								one_a = a1;
							}
						} else if (v->upw_use[i]) {

							/* matching def. in w outside loop,
							 reaching beginning v *and* the use */

							/* set local-load-type info
							 ************************ */

							u1_int *li = triv_lp->load_type;
							char **ls = triv_lp->load_sig;

							if (l0 >= glo_loc)
								javab_out(-1, "invalid local in scalar_dep()");

							if ((li[l0] == TP_UNUSED)
									|| ((li[l0] == tp) && (!ls[l0]))) {

								li[l0] = tp;

								switch (tp) {
								case TP_INT:
									ls[l0] = strdup("I");
									break;
								case TP_LONG:
									ls[l0] = strdup("J");
									break;
								case TP_FLOAT:
									ls[l0] = strdup("F");
									break;
								case TP_DOUBLE:
									ls[l0] = strdup("D");
									break;
								case TP_REF:

									if (a1 == U4MAX)
										ls[l0] = strdup(rd_sig[j]);
									else {
										u2_int sp = att->sp_before[a1];
										state_ptr store = att->st_state[a1][sp
												- 1u];
										if ((store) && (store->kind == S_REF)
												&& (store->u.ref.sig)) {
											if (!store->u.ref.set)
												ls[l0] =
														strdup(
																&(store->u.ref.sig[store->u.ref.d2]));
											else {
												ls[l0] =
														(char *) make_mem(
																(2
																		+ strlen(
																				store->u.ref.sig))
																		* sizeof(char));
												sprintf(ls[l0], "L%s;",
														store->u.ref.sig);
											}
										}
									}
									break;
								}
							} else if (li[l0] != tp) {
								triv_lp->par = 0u;
								li[l0] = TP_ERROR;
								javab_out(2, "    - PAR-FAIL: inconsistent"
										" type for local %u at %u", l0, a0);
								return;
							}

							one_out_a = a1;
							rd_out_loop++;
						}
					}
				}

			if ((rd_out_loop) && (dom_in_loop)) {
				triv_lp->par = 0u;
				javab_out(0,
						"outer def. at %u gaps dom. def. at %u for use at %u",
						one_out_a, one_dom_a, a0);
				return;
			} else if ((!dom_in_loop) && (rdf_in_loop)) {
				triv_lp->par = 0u;
				javab_out(2, "    - PAR-FAIL: loop-carried flow dependence"
						" d%u->u%u (%u->%u)", one_j + 1, i + 1, one_a, a0);
				return;
			}
		}
	}

	/* Check Constraint (b) [BikGannon]
	 ******************************** */

	for (j = 0u; j < rd_cnt; j++) {
		u2_int l0 = rd_loc[j];
		u4_int a0 = rd_add[j];
		bb_ptr v = (a0 != U4MAX) ? att->my_bb[a0] : NULL;

		if ((v) && (triv_lp->nl[v->name])) {

			/* now, we have an arbitrary def. that occurs in v in the loop */

			for (i = 0u; i < use_cnt; i++) {
				u2_int l1 = use_loc[i];
				u4_int a1 = use_add[i];
				bb_ptr w = att->my_bb[a1];

				if ((l0 == l1) && (!triv_lp->nl[w->name]) && (w->rd_in[j])
						&& (w->upw_use[i])) {

					/* now, we have a matching use in w outside the loop,
					 such that def. reaches w *and* this use */

					triv_lp->par = 0u;
					javab_out(2, "    - PAR-FAIL: loop-exiting flow dependence"
							" d%u->u%u (%u->%u)", j + 1, i + 1, a0, a1);
					return;
				}
			}
		}
	}
}

/* *******************************
 *** Reference Manipulations ***
 ******************************* */

static void del_refs(void) {
	while (g_refs) {
		ref_ptr n = g_refs->gnext;
		free(g_refs);
		g_refs = n;
	}
}

static ref_ptr new_ref(u4_int ad, state_ptr rf, state_ptr in, ref_ptr nx,
		u1_int lhs) {
	ref_ptr new = (ref_ptr) make_mem(sizeof(struct ref_node));

	new->gnext = g_refs;
	g_refs = new;

	new->next = nx;
	new->rf = rf;
	new->in = in;
	new->ad = ad;
	new->lhs = lhs;

	return new;
}

/* *********************************
 *** Stack State Manipulations ***
 ********************************* */

static void del_stack_states(void) {

	del_refs();

	while (g_states) {
		state_ptr n = g_states->gnext;
		if ((g_states->kind == S_EXP) && (g_states->u.exp.rd))
			free(g_states->u.exp.rd);
		free(g_states);
		g_states = n;
	}
}

static state_ptr *new_stack_tuple(u4_int t) {
	u4_int i;
	state_ptr *tup = (state_ptr *) make_mem(t * sizeof(state_ptr));
	for (i = 0u; i < t; i++)
		tup[i] = NULL; /* TOP */
	return tup;
}

/* ************************************************************
 *** Find LOCAL Reaching Defs. from GLOBAL Reaching Defs. ***
 ************************************************************ */

static void find_defs(state_ptr s, u4_int a, bb_ptr b, u2_int l) {
	u1_int *rd = s->u.exp.rd;
	u4_int cdef = rd_cnt, maxa = 0u, j, low, hig;

	if ((!b) || (!b->rd_in) || (!s) || (s->kind != S_EXP) || (!rd))
		javab_out(-1, "incorrect invocation of find_defs()");

	low = b->low;
	hig = b->high;

	for (j = 0u; j < rd_cnt; j++) {

		u1_int my_l = rd_loc[j];
		u4_int my_a = rd_add[j];

		if ((l == my_l) && (low <= my_a) && (my_a <= hig)) {

			/* Find Closest BEFORE use */

			if ((my_a < a) && (my_a >= maxa)) {
				cdef = j;
				maxa = my_a;
			}
		}
	}

	if (cdef < rd_cnt) { /* Found a LOCAL Definition of `l'
	 ****************************** */
		for (j = 0u; j < rd_cnt; j++)
			rd[j] = 0u;
		rd[cdef] = 1u;
	} else { /* Determine GLOBAL RD for variable `l'
	 ************************************ */

		u1_int *rd_in = b->rd_in;
		u2_int cnt = 0u;

		for (j = 0u; j < rd_cnt; j++)
			rd[j] = ((l == rd_loc[j]) && (rd_in[j])) ? 1u : 0u;

		for (j = 0u; j < rd_cnt; j++)
			if (rd[j])
				cnt++;

		if (cnt == 0u)
			javab_out(1, "variable %u at %u used before defined", l, a);
	}
}

/* *************************************
 *** Release Memory of this Module ***
 ************************************* */

static void release_memory(void) {
	if (s_states)
		free(s_states);
	s_states = NULL;

	if (rd_loc)
		free(rd_loc);
	rd_loc = NULL;

	if (rd_add)
		free(rd_add);
	rd_add = NULL;

	if (use_loc)
		free(use_loc);
	use_loc = NULL;

	if (use_add)
		free(use_add);
	use_add = NULL;

	if (use_typ)
		free(use_typ);
	use_typ = NULL;

	del_stack_states();

	bb_delete();
}

/* ****************************************
 *** Processing of Method Descriptors ***
 **************************************** */

static u2_int res_width(u1_int *s) {
	u2_int p = 1u;
	u2_int r = 1u;

	if ((!s) || (s[0] != '('))
		javab_out(-1, "invalid method descriptor");

	while (s[p++] != ')')
		;

	if (s[p] == 'V')
		r = 0u;
	else if ((s[p] == 'D') || (s[p] == 'J'))
		r = 2u;

	return r;
}

static u2_int arg_width(u1_int *s, u1_int set) {
	u2_int p = 1u, i;
	u2_int r = (set == 2u) ? 1u : 0u;
	u2_int b = 0u;

	if ((!s) || (s[0] != '('))
		javab_out(-1, "invalid method descriptor");

	while (s[p] != ')') {

		u2_int oldp = p;

		if (set)
			rd_sig[r] = rd_buf + b;
		r++;

		switch (s[p]) {

		case 'D':
		case 'J':

			/* additional word */

			if (set)
				rd_sig[r] = NULL;
			r++;

			p++;
			break;

		case 'L':

			while (s[p++] != ';')
				; /* skip <classname> */
			break;

		case '[':

			while (s[++p] == '[')
				; /* skip [[[[[ */
			if (s[p++] == 'L')
				while (s[p++] != ';')
					; /* skip <classname> */
			break;

		case 'B':
		case 'C':
		case 'F':
		case 'I':
		case 'S':
		case 'Z':

			p++;
			break;

		default:

			javab_out(0, "invalid character %c (=%i) in method descriptor",
					s[p], s[p]);
			return r;
		}

		if (set) {
			for (i = oldp; i < p; i++)
				rd_buf[b++] = s[i];
			rd_buf[b++] = '\0';
		}
	}
	return r;
}

/* ********************************************************
 *** Computation of ByteContext Sensitive Information ***
 *** (next is (mis-)used as an error flag)            ***
 ******************************************************** */

static u2_int det_ops(u4_int i) {

	u4_int j, lf, lb;

	switch (byt[i]) {

	case 170u: /* tableswitch  */

		glo_pad = (u2_int) (3u - (i % 4u)); /* zero padding */

		for (j = i + 1u; j <= glo_pad; j++)
			if (byt[j]) {
				next = 1;
				javab_out(0, "invalid padding in 'tableswitch' at %u", j);
			}

		glo_def = B2S4(byt[i + glo_pad + 1], byt[i + glo_pad + 2],
				byt[i + glo_pad + 3], byt[i + glo_pad + 4]);
		glo_low = B2S4(byt[i + glo_pad + 5], byt[i + glo_pad + 6],
				byt[i + glo_pad + 7], byt[i + glo_pad + 8]);
		glo_hig = B2S4(byt[i + glo_pad + 9], byt[i + glo_pad + 10],
				byt[i + glo_pad + 11], byt[i + glo_pad + 12]);

		/* Check Validity of all targets */

		if (((u4_int) (i + glo_def)) >= len) {
			next = 1;
			javab_out(0, "invalid default target in 'tableswitch' at %u", i);
		}

		lf = i + glo_pad + 13u;
		lb = glo_hig - glo_low + 1u;

		for (j = 0u; j < lb; j++, lf += 4u) {
			s4_int loc_off = B2S4(byt[lf], byt[lf + 1], byt[lf + 2],
					byt[lf + 3]);

			if (((u4_int) (i + loc_off)) >= len) {
				next = 1u;
				javab_out(0, "invalid target in '%s' at %u", mem, i);
			}
		}

		/* Return number of operands (in bytes) */

		return ((u2_int) (glo_pad + 16u + (glo_hig - glo_low) * 4u));

	case 171u: /* lookupswitch */

		glo_pad = (u2_int) (3u - (i % 4u)); /* zero padding */

		for (j = i + 1u; j <= glo_pad; j++)
			if (byt[j]) {
				next = 1u;
				javab_out(0, "invalid padding in 'lookupswitch' at %u", j);
			}

		glo_def = B2S4(byt[i + glo_pad + 1], byt[i + glo_pad + 2],
				byt[i + glo_pad + 3], byt[i + glo_pad + 4]);
		glo_npa = B2S4(byt[i + glo_pad + 5], byt[i + glo_pad + 6],
				byt[i + glo_pad + 7], byt[i + glo_pad + 8]);

		/* Check Validity of all targets */

		if (((u4_int) (i + glo_def)) >= len) {
			next = 1u;
			javab_out(0, "invalid default target in '%s' at %u", mem, i);
		}

		lf = i + glo_pad + 9u;
		lb = glo_npa;

		for (j = 0u; j < lb; j++, lf += 8u) {
			s4_int loc_off = B2S4(byt[lf + 4], byt[lf + 5], byt[lf + 6],
					byt[lf + 7]);
			if (((u4_int) (i + loc_off)) >= len) {
				next = 1;
				javab_out(0, "invalid target in '%s' at %u", mem, i);
			}
		}

		/* Return number of operands (in bytes) */

		return ((u2_int) (glo_pad + 8u + glo_npa * 8u));

	default:
		javab_out(-1, "error in det_ops %u at %u", byt[i], i);
	}
	return 0u; /* dummy return */
}

static u2_int det_pre(u4_int i) {

	u2_int entry = B2U2(byt[i + 1], byt[i + 2]);
	u2_int n, d;
	u1_int *s;

	switch (byt[i]) {

	case 181u: /* putfield  */

		GET_IT(CONSTANT_Fieldref, mem)

		return (u2_int) ((s[0] == 'D') || (s[0] == 'J')) ? 3u : 2u;

	case 179u: /* putstatic */

		GET_IT(CONSTANT_Fieldref, mem)

		return (u2_int) ((s[0] == 'D') || (s[0] == 'J')) ? 2u : 1u;

	case 185u: /* invokeinterface */

		GET_IT(CONSTANT_InterfaceMethodref, mem)

		if (byt[i + 3u] != (1u + arg_width(s, 0u)))
			javab_out(0, "nargs differs from method descriptor at %u", i);

		return (u2_int) (byt[i + 3u]);

	case 183u: /* invokespecial   */
	case 182u: /* invokevirtual   */

		GET_IT(CONSTANT_Methodref, mem)

		return (u2_int) (1u + arg_width(s, 0u));

	case 184u: /* invokestatic    */

		GET_IT(CONSTANT_Methodref, mem)

		return arg_width(s, 0u);

	case 197u: /* multianewarray */

		valid_cp_entry(CONSTANT_Class, entry, "multianewarray");

		return (u2_int) (byt[i + 3]);

	default:
		javab_out(-1, "error in det_pre %u at %u", byt[i], i);
	}
	return 0u;
}

static u2_int det_pos(u4_int i) {

	u2_int entry = B2U2(byt[i + 1], byt[i + 2]);
	u2_int n, d;
	u1_int *s;

	switch (byt[i]) {

	case 180u: /* getfield  */
	case 178u: /* getstatic */

		GET_IT(CONSTANT_Fieldref, mem)

		return (u2_int) ((s[0] == 'D') || (s[0] == 'J')) ? 2u : 1u;

	case 185u: /* invokeinterface */

		GET_IT(CONSTANT_InterfaceMethodref, mem)

		return (res_width(s));

	case 183u: /* invokespecial */
	case 184u: /* invokestatic  */
	case 182u: /* invokevirtual */

		GET_IT(CONSTANT_Methodref, mem)
		;

		return (res_width(s));

	default:
		javab_out(-1, "error in det_pos %u at %u", byt[i], i);
	}
	return 0u;
}

/* **********************************
 *** General bytecode traversal ***
 ********************************** */

static void byte_trav(u4_int offset, u1_int (*action)(u4_int)) {

	u4_int i; /* wide counter */
	u2_int ops = 0u;

	len = att->code_length;
	byt = &(att->info[8u]);

	for (i = offset; i < len; i += (1u + ops)) {

		/* Determine opcode Information */

		is_wide = 0u;

		back: opc = byt[i];
		bra = bytecode[opc].branch;
		ops = bytecode[opc].operands;
		exc = bytecode[opc].exception;

		pre = bytecode[opc].stack_pre;
		pos = bytecode[opc].stack_post;

		mem = bytecode[opc].mnemonic;

		/* instruction 'wide'-handling
		 *************************** */

		if (is_wide) {
			if (opc == 132u) /* wide + iinc  */
				ops = 4u;
			else if ((21u <= opc) && (opc <= 25u)) /* wide + load  */
				ops = 2u;
			else if ((54u <= opc) && (opc <= 58u)) /* wide + store */
				ops = 2u;
			else if (opc == 169u) /* wide + ret   */
				ops = 2u;
			else {
				javab_out(0, "invalid operand '%s' of 'wide' at %u", mem, i);
				return;
			}
		}

		if (HAS_TARGET(bra)) {

			/* Compute target from 2-, or 4-byte offset
			 **************************************** */

			s4_int off =
					((opc == 200u) || (opc == 201u)) ?
							B2S4(byt[i + 1], byt[i + 2], byt[i + 3], byt[i + 4]) /* 4-bytes */
							:
							B2S2(byt[i + 1], byt[i + 2]); /* 2-bytes */
			target = (u4_int) (i + off);

			if ((target >= len) && (att->reachable[i] == 1u)) {
				javab_out(0, "invalid target %u in '%s' at %u", target, mem, i);
				return;
			}
		}

		/* Determine Context Sensitive Information
		 *************************************** */

		glo_pad = 0u;
		glo_def = glo_npa = glo_low = glo_hig = 0u;

		next = 0u; /* (mis-)uses as error flag */

		if (ops == 9u)
			ops = det_ops(i);

		if (pre == 9u)
			pre = det_pre(i);

		if (pos == 9u)
			pos = det_pos(i);

		if (next)
			return;

		/* Compute Address of next Opcode
		 ****************************** */

		next = (u4_int) (i + ops + 1u);

		if (next > len) {
			javab_out(0, "invalid implicit target %u in '%s' at %u", next, mem,
					i);
			return;
		}

		/* Apply Action, and quit directly on nonzero return
		 Do *NOT* process unreachable statements
		 ************************************************* */

		if ((action) && (att->reachable[i])) {
			if ((*action)(i))
				return;
		}

		/* instruction 'wide'-handling
		 *************************** */

		if (opc == 196u) {
			if (i + 1 < len) {
				i++;
				is_wide = 1;
				goto back;
			} else {
				javab_out(0, "invalid occurrence of '%s' at %u", mem, i);
				return;
			}
		}
	}
}

/* *******************************************************
 *** The actual actions (PRIVATE TRAVERSAL ROUTINES) ***
 ******************************************************** */

/* ****************************************
 *** Reaching Definitions Information ***
 **************************************** */

static void add_rd(u4_int i, u2_int loc) {
	if (rd_pass == 2) {
		if (loc >= glo_loc)
			javab_out(0, "invalid definition of local %u", loc);
		rd_loc[rd_cnt] = loc;
		rd_add[rd_cnt] = i;
	}
	rd_cnt++;
}

static void add_use(u4_int i, u2_int loc, u1_int tp) {
	if (rd_pass == 2) {
		if (loc >= glo_loc)
			javab_out(0, "invalid use of local %u", loc);
		use_loc[use_cnt] = loc;
		use_add[use_cnt] = i;
		use_typ[use_cnt] = tp;
	}
	use_cnt++;
}

static u1_int comp_rd_u_s(u4_int i) {

	if (((opc >= 54u) && (opc <= 78u)) || (opc == 132u)) {

		u2_int loc;

		switch (opc) {

		case 55u: /* lstore */
		case 57u: /* dstore */

			loc = (is_wide) ?
					B2U2(byt[i + 1], byt[i + 2]) : (u1_int) byt[i + 1];

			add_rd(i, loc);
			add_rd(i, loc + 1);
			break;

		case 54u: /* istore */
		case 56u: /* fstore */
		case 58u: /* astore */
		case 132u: /* iinc */

			loc = (is_wide) ?
					B2U2(byt[i + 1], byt[i + 2]) : (u1_int) byt[i + 1];

			add_rd(i, loc);
			break;

		case 59u:
		case 60u:
		case 61u:
		case 62u: /* istore_n */

			loc = ((opc - 59u) % 4);

			add_rd(i, loc);
			break;

		case 63u:
		case 64u:
		case 65u:
		case 66u: /* lstore_n */

			loc = ((opc - 63u) % 4);

			add_rd(i, loc);
			add_rd(i, loc + 1);
			break;

		case 67u:
		case 68u:
		case 69u:
		case 70u: /* fstore_n */

			loc = ((opc - 67u) % 4);

			add_rd(i, loc);
			break;

		case 71u:
		case 72u:
		case 73u:
		case 74u: /* dstore_n */

			loc = ((opc - 71u) % 4);

			add_rd(i, loc);
			add_rd(i, loc + 1);
			break;

		case 75u:
		case 76u:
		case 77u:
		case 78u: /* astore_n */

			loc = ((opc - 75u) % 4);

			add_rd(i, loc);
			break;

		default:
			javab_out(-1, "invalid implementation of comp_rd_u_s()");
		}
	}

	if (((opc >= 21u) && (opc <= 45u)) || (opc == 132u)) {

		u2_int loc;
		u1_int tp = TP_UNUSED;

		switch (opc) {

		case 21u: /* iload */
		case 132u: /* iinc */

			tp = TP_INT;
			loc = (is_wide) ?
					B2U2(byt[i + 1], byt[i + 2]) : (u1_int) byt[i + 1];

			add_use(i, loc, tp);
			break;

		case 22u: /* lload */
		case 24u: /* dload */

			tp = (opc == 22u) ? TP_LONG : TP_DOUBLE;
			loc = (is_wide) ?
					B2U2(byt[i + 1], byt[i + 2]) : (u1_int) byt[i + 1];

			add_use(i, loc, tp);
			add_use(i, loc + 1, TP_2WORD);
			break;

		case 23u: /* fload */
		case 25u: /* aload */

			tp = (opc == 23u) ? TP_FLOAT : TP_REF;
			loc = (is_wide) ?
					B2U2(byt[i + 1], byt[i + 2]) : (u1_int) byt[i + 1];

			add_use(i, loc, tp);
			break;

		case 26u:
		case 27u:
		case 28u:
		case 29u: /* iload_n */

			tp = TP_INT;
			loc = ((opc - 26u) % 4);

			add_use(i, loc, tp);
			break;

		case 30u:
		case 31u:
		case 32u:
		case 33u: /* lload_n */

			tp = TP_LONG;
			loc = ((opc - 30u) % 4);

			add_use(i, loc, tp);
			add_use(i, loc + 1, TP_2WORD);
			break;

		case 34u:
		case 35u:
		case 36u:
		case 37u: /* fload_n */

			tp = TP_FLOAT;
			loc = ((opc - 34u) % 4);

			add_use(i, loc, tp);
			break;

		case 38u:
		case 39u:
		case 40u:
		case 41u: /* dload_n */

			tp = TP_DOUBLE;
			loc = ((opc - 38u) % 4);

			add_use(i, loc, tp);
			add_use(i, loc + 1, TP_2WORD);
			break;

		case 42u:
		case 43u:
		case 44u:
		case 45u: /* aload_n */

			tp = TP_REF;
			loc = ((opc - 42u) % 4);

			add_use(i, loc, tp);
			break;

		default:
			javab_out(-1, "invalid implementation of comp_rd_u_s()");
		}
	}
	return 0u;
}

/* **************************************
 *** Compute sp_before information: ***
 ************************************** */

static u1_int comp_sp_info(u4_int i) {

	if (att->reachable[i] == 1u) { /* alread visited */

		if (att->sp_before[i] != cur_sp)
			javab_out(0, "inconsistent stack size in '%s' at %u (%u vs. %u)",
					mem, i, att->sp_before[i], cur_sp);
		return 1u;
	} else {

		/* Set stack state, and mark as visited */

		att->sp_before[i] = cur_sp;
		att->reachable[i] = 1u;

		if (cur_sp < pre) {
			javab_out(0, "stack underflow in '%s' at %u (%u vs. %u)", mem, i,
					cur_sp, pre);
			return 1;
		}

		cur_sp += (pos - pre);

		if (cur_sp > glo_sta) {
			javab_out(0, "stack overflow in '%s' at %u (%u vs. %u)", mem, i,
					cur_sp, glo_sta);
			glo_sta = cur_sp;
		}

		if (cur_sp > glo_stm)
			glo_stm = cur_sp;

		{ /* RECURSION
		 (Save important 'environment' to allow nested traversals)
		 ********************************************************* */

			u1_int loc_opc = opc;
			u1_int loc_bra = bra;
			u2_int loc_sp = cur_sp;
			u4_int loc_target = target;

			/* Deal Recursively with Switches */

			if (opc == 170u) { /* tableswitch */

				u4_int j, lf = i + glo_pad + 13u, lb = glo_hig - glo_low + 1u;

				cur_sp = loc_sp;

				byte_trav((u4_int) (i + glo_def), comp_sp_info);

				for (j = 0u; j < lb; j++, lf += 4u) {
					s4_int loc_off = B2S4(byt[lf], byt[lf + 1], byt[lf + 2],
							byt[lf + 3]);

					cur_sp = loc_sp;

					byte_trav((u4_int) (i + loc_off), comp_sp_info);
				}

				return 1u;

			} else if (opc == 171u) { /* lookupswitch */

				u4_int j, lf = i + glo_pad + 9u, lb = glo_npa;

				cur_sp = loc_sp;

				byte_trav((u4_int) (i + glo_def), comp_sp_info);

				for (j = 0u; j < lb; j++, lf += 8u) {
					s4_int loc_off = B2S4(byt[lf + 4], byt[lf + 5], byt[lf + 6],
							byt[lf + 7]);

					cur_sp = loc_sp;

					byte_trav((u4_int) (i + loc_off), comp_sp_info);
				}

				return 1u;
			}

			opc = loc_opc;
			cur_sp = loc_sp;

			/* Deal with target of branch */

			if (HAS_TARGET(loc_bra))
				byte_trav(loc_target, comp_sp_info);

			/* Restore Information
			 ******************* */

			opc = loc_opc;
			cur_sp = loc_sp;

			/* Quit traversal for no-next instructions
			 *************************************** */

			if ((loc_bra == 2u) || (loc_bra == 5u)) {

				if ((opc >= 172u) && (opc <= 176u)) { /* <T>return */
					if (cur_sp)
						javab_out(1, "non-empty stack on return at %u", i);
				} else if (opc == 169u) { /* ret */
					if (cur_sp)
						javab_out(0, "non-empty stack on ret at %u", i);
				}

				return 1u;
			}

			/* Pop Return Address AFTER jsr/jsr_w
			 (because link from ret back is not yet available,
			 we are overrestrictive wrt. jsr/ret mechanism:
			 jsr must only leave ret address on operand stack,
			 ret must leave operand stack empty!)
			 ***************************************** */

			if ((opc == 168u) || (opc == 201u)) { /* jsr/jsr_w */
				cur_sp--;
				if (cur_sp)
					javab_out(0,
							"multiple stack words passed to subroutine at %u",
							i);
			}
		}
	}
	return 0u;
}

/* *********************************
 *** Test for Unreachable Code ***
 ********************************* */

static u1_int unreachable(u4_int i) {

	if ((HAS_TARGET(bra)) && (att->reachable[i] == 1u)
			&& (att->reachable[target] != 1u))
		javab_out(0, "inter-code target %u in '%s' at %u", target, mem, i);

	if (att->reachable[i] == 2u) {

		/* mark *unvisited* instruction as *unreachable* */

		att->reachable[i] = 0u;

		if (reportu)
			javab_out(1, "unreachable code '%s' at %u", mem, i);
		reportu = 0u;
	} else
		reportu = 1u;
	return 0u;
}

/* ****************************************
 *** Compute is_leader information:   ***
 *** + directly after:                ***
 ***   * explicit conditional branch  ***
 ***   * explicit uncond.     branch  ***
 ***     (including jsr)              ***
 ***   * return-like instruction      ***
 ***   * implicit branch (exception)  ***
 *** + target of:                     ***
 ***   * conditional branch           ***
 ***   * uncond.     branch           ***
 ***     (including jsr)              ***
 ***   * switch construct             ***
 **************************************** */

static u1_int comp_leaders(u4_int i) {

	if (next > max_seen)
		max_seen = next;

	if ((bra) || (exc)) /* directly after a branch */
		att->is_leader[next] = 1u;

	if (HAS_TARGET(bra)) /* has target */
		att->is_leader[target] = 1u;

	if (opc == 170u) { /* tableswitch */

		u4_int j, lf = i + glo_pad + 13u, lb = glo_hig - glo_low + 1u;

		att->is_leader[i + glo_def] = 1u;

		for (j = 0u; j < lb; j++, lf += 4u) {
			s4_int loc_off = B2S4(byt[lf], byt[lf + 1], byt[lf + 2],
					byt[lf + 3]);
			att->is_leader[i + loc_off] = 1u;
		}
	} else if (opc == 171u) { /* lookupswitch */

		u4_int j, lf = i + glo_pad + 9u, lb = glo_npa;

		att->is_leader[i + glo_def] = 1u;

		for (j = 0u; j < lb; j++, lf += 8u) {
			s4_int loc_off = B2S4(byt[lf + 4], byt[lf + 5], byt[lf + 6],
					byt[lf + 7]);
			att->is_leader[i + loc_off] = 1u;
		}
	} else if (((opc >= 182u) && (opc <= 185u)) || (opc == 169u)) { /* invocation and ret (<- tech. detail) */

		att->is_leader[i] = 1u;

	}
	return 0u;
}

/* *******************************
 *** Construct Basic Blocks ****
 ******************************* */

static u1_int comp_bb1(u4_int i) {

	if (att->is_leader[i]) {

		/* Find Next Leader */

		u4_int j = i;

		while (!att->is_leader[j + 1])
			j++;

		cur_bb = bb_add(i, j, (i == 0u));
	}

	att->my_bb[i] = cur_bb;

	return 0u;
}

static u1_int comp_bb2(u4_int i) {

	if (!att->my_bb[i])
		javab_out(-1, "lost basic block in comp_bb2()");

	if (exc) { /* link bb with exception to
	 *every* exception handler with a code
	 region that covers this instruction,
	 (mark edges as exceptional edges) */

		u4_int j; /* wide counter */
		u4_int b0 = att->code_length;
		u1_int *bs = att->info;
		u2_int e = B2U2(bs[8u + b0], bs[9u + b0]);

		for (j = 0u; j < e; j++) {
			u4_int b = b0 + 10u + j * 8u;
			u2_int s_pc = B2U2(bs[b], bs[b + 1u]);
			u2_int e_pc = B2U2(bs[b + 2u], bs[b + 3u]);
			u2_int h_pc = B2U2(bs[b + 4u], bs[b + 5u]);

			if ((s_pc <= i) && (i < e_pc)) {
				bb_add_succ(att->my_bb[i], att->my_bb[h_pc], (u1_int) 1u);
				bb_add_pred(att->my_bb[h_pc], att->my_bb[i], (u1_int) 1u);
			}
		}
	}

	if (HAS_TARGET(bra)) { /* link bb to target bb */
		bb_add_succ(att->my_bb[i], att->my_bb[target], (u1_int) 0u);
		bb_add_pred(att->my_bb[target], att->my_bb[i], (u1_int) 0u);
	}

	if ((bra != 2u) && (bra != 3u) && (bra != 5u)) { /* may fall through */
		bb_add_succ(att->my_bb[i], att->my_bb[next], 0u);
		bb_add_pred(att->my_bb[next], att->my_bb[i], 0u);
	}

	if (opc == 170u) { /* tableswitch */

		u4_int j, lf = i + glo_pad + 13u, lb = glo_hig - glo_low + 1u;

		bb_add_succ(att->my_bb[i], att->my_bb[i + glo_def], 0u);
		bb_add_pred(att->my_bb[i + glo_def], att->my_bb[i], 0u);

		for (j = 0u; j < lb; j++, lf += 4u) {
			s4_int loc_off = B2S4(byt[lf], byt[lf + 1], byt[lf + 2],
					byt[lf + 3]);
			bb_add_succ(att->my_bb[i], att->my_bb[i + loc_off], 0u);
			bb_add_pred(att->my_bb[i + loc_off], att->my_bb[i], 0u);
		}
	} else if (opc == 171u) { /* lookupswitch */

		u4_int j, lf = i + glo_pad + 9u, lb = glo_npa;

		bb_add_succ(att->my_bb[i], att->my_bb[i + glo_def], 0u);
		bb_add_pred(att->my_bb[i + glo_def], att->my_bb[i], 0u);

		for (j = 0u; j < lb; j++, lf += 8u) {
			s4_int loc_off = B2S4(byt[lf + 4], byt[lf + 5], byt[lf + 6],
					byt[lf + 7]);
			bb_add_succ(att->my_bb[i], att->my_bb[i + loc_off], 0u);
			bb_add_pred(att->my_bb[i + loc_off], att->my_bb[i], 0u);
		}
	}
	return 0u;
}

static u1_int comp_bb3(u4_int i) {

	if (bra == 3u) { /* jsr */
		bb_ptr sub = att->my_bb[target];
		bb_ptr ret = att->my_bb[next];

		bb_link_sub_back(byt, sub, ret);

		if ((byt[i] != 168u) && (byt[i] != 201u))
			javab_out(-1, "not a jsr in comp_bb3()");
	}

	return 0u;
}

/* ********************************************
 *** Local/Global Stack State Computation ***
 ******************************************** */

static void update_stack_state(u4_int i) {
	u4_int j; /* wide counter */
	u2_int sp = att->sp_before[i];
	u2_int minsp = sp - pre;
	u2_int newsp = minsp + pos;

	if (sp > glo_sta)
		javab_out(-1, "invalid stack pointer %u at %u in"
				"update_stack_state()", sp, i);

	switch (opc) {

	/* stack manipulations */

	case 89u: /* dup */
		s_states[sp] = s_states[sp - 1u];
		break;

	case 90u: /* dup_x1 */
		s_states[sp] = s_states[sp - 1u];
		s_states[sp - 1u] = s_states[sp - 2u];
		s_states[sp - 2u] = s_states[sp];
		break;

	case 91u: /* dup_x2 */
		s_states[sp] = s_states[sp - 1u];
		s_states[sp - 1u] = s_states[sp - 2u];
		s_states[sp - 2u] = s_states[sp - 3u];
		s_states[sp - 3u] = s_states[sp];
		break;

	case 92u: /* dup2 */
		s_states[sp + 1u] = s_states[sp - 1u];
		s_states[sp] = s_states[sp - 2u];
		break;

	case 93u: /* dup2_x1 */
		s_states[sp + 1u] = s_states[sp - 1u];
		s_states[sp] = s_states[sp - 2u];
		s_states[sp - 1u] = s_states[sp - 3u];
		s_states[sp - 2u] = s_states[sp + 1u];
		s_states[sp - 3u] = s_states[sp];
		break;

	case 94u: /* dup2_x2 */
		s_states[sp + 1u] = s_states[sp - 1u];
		s_states[sp] = s_states[sp - 2u];
		s_states[sp - 1u] = s_states[sp - 3u];
		s_states[sp - 2u] = s_states[sp - 4u];
		s_states[sp - 3u] = s_states[sp + 1u];
		s_states[sp - 4u] = s_states[sp];
		break;

	case 95u: /* swap */
	{
		state_ptr tmp = s_states[sp - 2u];
		s_states[sp - 2u] = s_states[sp - 1u];
		s_states[sp - 1u] = tmp;
	}
		break;

		/* some simple form of constant folding [Aho] */

	case 116u: /* ineg */

		/* - exp(c,D) = exp(-c,D) if D = emptyset
		 ************************************** */

	{
		state_ptr o1 = (sp >= 1u) ? s_states[sp - 1u] : NULL;

		if ((o1) && (o1->kind == S_EXP) && (!o1->u.exp.rd)) {
			s4_int op1 = 0 - o1->u.exp.con;
			s_states[sp - 1u] = new_stack_state(S_EXP, U4MAX, op1);
		} else
			s_states[sp - 1u] = new_stack_state(S_BOT, 0u, 0);
	}
		break;

	case 96u: /* iadd */
	case 100u: /* isub */
	case 104u: /* imul */

	case 108u: /* idiv */

		/* exp(c,D) + exp(c',D') = exp(c+c',D/D') if D or D'  = emptyset
		 exp(c,D) - exp(c',D') = exp(c-c',D)    if D'       = emptyset
		 exp(c,D) x exp(c',D') = exp(cxc',D)    if D and D' = emptyset

		 exp(c,D) / exp(c',D') = exc(c/c',D)    if D and D' = emptyset
		 and c' != 0
		 ************************************************************* */

	{
		state_ptr o1 = (sp >= 2u) ? s_states[sp - 2u] : NULL;
		state_ptr o2 = (sp >= 1u) ? s_states[sp - 1u] : NULL;

		s_states[sp - 1u] = new_stack_state(S_BOT, 0u, 0);

		if ((o1) && (o1->kind == S_EXP) && (o2) && (o2->kind == S_EXP)) {

			s4_int op1 = o1->u.exp.con;
			s4_int op2 = o2->u.exp.con;

			switch (opc) {

			case 96u: /* iadd */

				if ((o1->u.exp.rd) && (o2->u.exp.rd))
					s_states[sp - 2u] = new_stack_state(S_BOT, 0u, 0);
				else if ((o1->u.exp.rd) || (o2->u.exp.rd)) {
					u1_int *r = (o1->u.exp.rd) ? o1->u.exp.rd : o2->u.exp.rd;
					state_ptr new = new_stack_state(S_EXP, 0u, op1 + op2);

					for (j = 0u; j < rd_cnt; j++)
						new->u.exp.rd[j] = r[j];

					s_states[sp - 2u] = new;
				} else
					s_states[sp - 2u] = new_stack_state(S_EXP, U4MAX,
							op1 + op2);
				break;

			case 100u: /* isub */

				if (o2->u.exp.rd)
					s_states[sp - 2u] = new_stack_state(S_BOT, 0u, 0);
				else if (o1->u.exp.rd) {

					u1_int *r = o1->u.exp.rd;
					state_ptr new = new_stack_state(S_EXP, 0u, op1 - op2);

					for (j = 0u; j < rd_cnt; j++)
						new->u.exp.rd[j] = r[j];

					s_states[sp - 2u] = new;
				} else
					s_states[sp - 2u] = new_stack_state(S_EXP, U4MAX,
							op1 - op2);
				break;

			case 104u: /* imul */

				if ((o1->u.exp.rd) || (o2->u.exp.rd))
					s_states[sp - 2u] = new_stack_state(S_BOT, 0u, 0);
				else
					s_states[sp - 2u] = new_stack_state(S_EXP, U4MAX,
							op1 * op2);
				break;

			case 108u: /* idiv */

				if ((o1->u.exp.rd) || (o2->u.exp.rd) || (op2 == 0))
					s_states[sp - 2u] = new_stack_state(S_BOT, 0u, 0);
				else
					s_states[sp - 2u] = new_stack_state(S_EXP, U4MAX,
							op1 / op2);
				break;

			default:
				javab_out(-1, "incorrrect operator");
			}
		} else
			s_states[sp - 2u] = new_stack_state(S_BOT, 0u, 0);
	}
		break;

		/* load instructions */

	case 26u:
	case 27u:
	case 28u:
	case 29u: /* iload_n */

		s_states[sp] = new_stack_state(S_EXP, 0u, 0);
		find_defs(s_states[sp], i, att->my_bb[i], (u2_int) (opc - 26u));
		break;

	case 42u:
	case 43u:
	case 44u:
	case 45u: /* aload_n */

		s_states[sp] = new_stack_state(S_EXP, 0u, 0);
		find_defs(s_states[sp], i, att->my_bb[i], (u2_int) (opc - 42u));
		break;

	case 21u: /* iload n */
	case 25u: /* aload n */

	{
		u2_int loc = (is_wide) ? (B2U2(byt[i + 1], byt[i + 2])) : byt[i + 1];

		s_states[sp] = new_stack_state(S_EXP, 0u, 0);
		find_defs(s_states[sp], i, att->my_bb[i], loc);
	}
		break;

		/* constant loads */

	case 2u: /* iconst_m1 */
	case 3u: /* iconst_0  */
	case 4u:
	case 5u:
	case 6u:
	case 7u:
	case 8u: /* iconst_c */

		s_states[sp] = new_stack_state(S_EXP, U4MAX, (s4_int) (opc - 3));
		break;

	case 16u: /* bipush */

		s_states[sp] = new_stack_state(S_EXP, U4MAX, (s1_int) byt[i + 1]);
		break;

	case 17u: /* sipush */

		s_states[sp] = new_stack_state(S_EXP, U4MAX,
				B2S2(byt[i + 1], byt[i + 2]));
		break;

		/* array manipulations */

	case 50u: /* aaload */
	case 189u: /* anewarray */
	case 188u: /* newarray */
	case 197u: /* multianewarray */

		for (j = minsp; j < MAX(newsp, sp); j++)
			s_states[j] = new_stack_state(S_BOT, 0u, 0);
		s_states[newsp - 1u] = new_stack_state(S_REF, i, 0);
		break;

		/* constant loads */

	case 18u: /* ldc    */
	case 19u: /* ldc_w  */

	{
		u2_int in = (opc == 18u) ? byt[i + 1] : B2U2(byt[i + 1], byt[i + 2]);

		if ((in < constant_pool_count) && (constant_pool[in])
				&& (constant_pool[in]->tag == CONSTANT_Integer)) {

			s4_int val = (s4_int) constant_pool[in]->u.data.val1;

			s_states[sp] = new_stack_state(S_EXP, U4MAX, val);

			break;
		}
	}
		/* FALL THROUGH */

		/* all other bytecode instructions */

	default:

		for (j = minsp; j < MAX(newsp, sp); j++)
			s_states[j] = new_stack_state(S_BOT, 0u, 0);
		break;
	}
}

static u1_int comp_stack_state(u4_int i) {
	bb_ptr b = (att->my_bb[i]) ? att->my_bb[i] : NULL;

	if (!b)
		javab_out(-1, "lost basic block in comp_stack_state()");

	update_stack_state(i);

	if ((next - 1) == b->high) { /* end of basic block reached */

		u4_int j; /* wide counter */

		/* alloc new tuple */

		b->s_gen = new_stack_tuple(glo_sta);
		b->s_in = new_stack_tuple(glo_sta);
		b->s_out = new_stack_tuple(glo_sta);

		/* initialize new tuples, reset temp. tuple */

		for (j = 0u; j < glo_sta; j++) {
			b->s_gen[j] = s_states[j];
			b->s_out[j] = s_states[j];
			b->s_in[j] = NULL; /* TOP */
			s_states[j] = NULL; /* TOP */
		}
	}
	return 0u;
}

/* **************************************
 *** Global Stack State Computation ***
 ************************************** */

static u1_int all_stack_state(u4_int i) {
	u4_int j; /* wide counter */
	bb_ptr b = att->my_bb[i];

	if ((!b) || (!att->st_state) || (!b->s_in))
		javab_out(-1, "lost information in all_stack_state()");

	if (i == b->low) { /* RESET to s_in at begin of basic block
	 ************************************* */
		for (j = 0u; j < glo_sta; j++)
			s_states[j] = b->s_in[j];
	}

	/* Associate with Instruction */

	att->st_state[i] = new_stack_tuple(glo_sta);

	for (j = 0u; j < glo_sta; j++)
		att->st_state[i][j] = s_states[j];

	update_stack_state(i);

	return 0u;
}

static u1_int prop_stack_state(u4_int i) {
	u4_int j; /* wide counter */
	bb_ptr b = att->my_bb[i];
	state_ptr *old = att->st_state[i];

	if (i == b->low) /* begin of basic block  */
		my_prop = 0u;

	/* Store any improved stack states during propagation
	 ************************************************** */

	if (my_prop)
		for (j = 0u; j < glo_sta; j++) {
			if ((old[j]) && (old[j]->kind == S_BOT))
				old[j] = s_states[j];
		}

	/* Load stack states, possibly start propagation
	 ********************************************* */

	for (j = 0u; j < glo_sta; j++) {

		s_states[j] = old[j];

		if ((old[j]) && (old[j]->prop)) { /* start propagation */
			old[j]->prop = 0u;
			my_prop = 1u;
		}
	}

	if (my_prop)
		update_stack_state(i);

	return 0u;
}

#ifdef OBSOLETED

/* ***************************************
 *** Simple form of Copy Propagation ***
 *************************************** */

static state_ptr copy_prop(state_ptr s0) {
	u4_int i, j; /* wide counters */

	if (s0) {

		if ((s0 -> kind == S_EXP) && (s0 -> u.exp.rd)) { /* Check Sources */

			for (i = 0u; i < rd_cnt; i++)
			if (s0 -> u.exp.rd[i]) {

				u4_int a1 = rd_add[i];

				if ((a1 != U4MAX) && (byt[a1] != 132u)) { /* true store-def. */

					u4_int sp = att -> sp_before[a1];
					state_ptr s1 = (sp >= 1u) ? att -> st_state[a1][sp-1u] : NULL;

					if ((s1) &&
							(s1 -> kind == S_EXP) &&
							(s1 -> u.exp.rd) &&
							(s1 -> u.exp.con == 0) && (! s1 -> u.exp.rd[i])) {

						/* Replace THIS definition by copy-definitions in a FRESH Copy */

						state_ptr nw = new_stack_state(S_EXP, 0u, s0 -> u.exp.con);

						for (j = 0u; j < rd_cnt; j++)
						nw -> u.exp.rd[j] = s0 -> u.exp.rd[j];
						nw -> u.exp.rd[i] = 0u;

						for (j = 0u; j < rd_cnt; j++)
						if (s1 -> u.exp.rd[j])
						nw -> u.exp.rd[j] = 1u;

						cc_cnt++;
						return nw;
					}
				}
			}
		}
	}
	return s0;
}

#endif

/* ************************************************
 *** Simple form of Copy/Constant Propagation ***
 ************************************************ */

static state_ptr cc_prop(state_ptr s0) {

	if (s0) {

		u4_int i; /* wide counter */

		if ((s0->kind == S_EXP) && (s0->u.exp.rd)) {

			/* Check Sources of an exp(c,D) at a */

			u4_int my_ad;
			s4_int my_c;
			u1_int set = 0u; /* 0: unset
			 1: a ref(a)
			 2: a con(c)
			 3: an exp(c,D)
			 4: bottom
			 **************** */

			u1_int *var_comp = NULL;

			for (i = 0u; i < rd_cnt; i++) {

				if (s0->u.exp.rd[i]) {

					u4_int a1 = rd_add[i];

					if ((a1 == U4MAX) || (byt[a1] == 132u)) {
						set = 4u; /* bot */
					} else { /* true store-def. */

						u4_int sp = att->sp_before[a1];
						state_ptr s1 =
								(sp >= 1u) ? att->st_state[a1][sp - 1u] : NULL;

						if ((s1) && (s1->kind == S_REF)) { /* rd: ref(a') */

							if (set == 0u) {
								set = 1u;
								my_ad = s1->u.ref.ad;
							} else if (set == 1u) {
								if (my_ad != s1->u.ref.ad)
									set = 4u; /* bot */
							} else
								set = 4u; /* bot */

						} else if ((s1) && (s1->kind == S_EXP)) { /* rd: exp(c',D') */

							if (set == 0u) { /* first setting */

								set = (!s1->u.exp.rd) ? 2u : 3u;
								my_c = s1->u.exp.con;
								var_comp = s1->u.exp.rd;
							} else if (set == 2u) { /* must be a con(c') */

								if ((my_c != s1->u.exp.con) || (s1->u.exp.rd))
									set = 4u; /* bottom */

							} else if (set == 3u) { /* must be a exp(c',D') */

								u1_int *my_v = s1->u.exp.rd;

								if ((my_c != s1->u.exp.con) || (!my_v))
									set = 4u; /* bottom */
								else {

									u4_int j; /* wide counter */

									for (j = 0u; j < rd_cnt; j++)
										if (my_v[j] != var_comp[j]) {
											set = 4u; /* bottom */
											break;
										}
								}
							} else
								set = 4u; /* bot */
						} else
							set = 4u; /* bot */
					}

				} /* if */
			} /* for */

			/* The Actual Copy/Constant Propagation
			 ************************************** */

			if ((set == 1u) && (s0->u.exp.con == 0)) {

				/* Return a FRESH ref(a') Copy */

				state_ptr nw = new_stack_state(S_REF, my_ad, 0);
				cc_cnt++;
				return nw;
			} else if (set == 2u) {

				/* Return a FRESH con(c') Copy */

				state_ptr nw = new_stack_state(S_EXP, U4MAX,
						s0->u.exp.con + my_c);
				nw->prop = 1u; /* can be propagated locally */
				cc_cnt++;
				return nw;
			} else if (set == 3u) {

				u4_int j; /* wide counter */

				/* Return a FRESH exp(c',D') Copy */

				state_ptr nw = new_stack_state(S_EXP, 0, s0->u.exp.con + my_c);

				for (j = 0; j < rd_cnt; j++)
					nw->u.exp.rd[j] = var_comp[j];

				nw->prop = 1u; /* can be propagated locally */
				cc_cnt++;
				return nw;

			}

		}
	}
	return s0;
}

/* **************************
 *** Propagation Driver ***
 ************************** */

static u1_int simple_prop(u4_int i) {
	u4_int j; /* wide counter */
	for (j = 0u; j < glo_sta; j++) {
		att->st_state[i][j] = cc_prop(att->st_state[i][j]);
#ifdef OBSOLETED
		att -> st_state[i][j] = copy_prop(att -> st_state[i][j]);
#endif
	}
	return 0u;
}

/* ************************************************
 *** Extract Signature from Array Declaration ***
 ************************************************ */

static void det_sig(state_ptr nw, u1_int o, u1_int o1, u1_int o2) {
	switch (o) {
	case 188u: /* newarray */
		nw->u.ref.sig = &(array_sigs[o1 - 3u][0]);
		break;
	case 189u: /* anewarray */
	case 197u: /* multianewarray */
	{
		u2_int e1 = B2U2(o1, o2);
		u2_int e2 = constant_pool[e1]->u.indices.index1;
		nw->u.ref.sig = (char *) constant_pool[e2]->u.utf8.s;
		nw->u.ref.set = (o == 189u); /* process anewarray later */
	}
		break;
	}
}

/* *************************************************************
 *** Change Single Reaching Array Parameter into Reference ***
 *************************************************************
 *** BikGannon: var( { d } ) -> apar(l,m)                  ***
 ************************************************************* */

static state_ptr var_to_ref(state_ptr s, u2_int l) {
	char *tp = rd_sig[l];
	u1_int d = 0u;

	if ((!tp) || (s->kind != S_EXP))
		javab_out(-1, "lost parameter signature in var_to_ref()");

	while (tp[d] == '[')
		d++;

	if (d != 0u) { /* array */

		/* Replace by Array Reference in FRESH COPY
		 **************************************** */

		s = new_stack_state(S_REF, U4MAX, 0);

		s->u.ref.loc = l;
		s->u.ref.d = d;
		s->u.ref.d2 = 0u;
		s->u.ref.set = 0u;
		s->u.ref.sig = tp;
	}
	return s;
}

static state_ptr chase1(state_ptr s) {
	u4_int i; /* wide counter */
	u2_int cnt = 0u, last;
	u1_int *rd = s->u.exp.rd;

	for (i = 0u; i < rd_cnt; i++)
		if (rd[i]) {
			last = (u4_int) i;
			cnt++;
		}
	if ((cnt == 1u) && (rd_add[last] == U4MAX))
		s = var_to_ref(s, last);
	else if (cnt == 0u)
		javab_out(0, "uninitialized variable");
	return s;
}

/* *******************************
 *** Array Reference Chasing ***
 *************************************************
 *** BikGannon: ref(a) ->  bot                 ***
 ***                       apar(l,n)           ***
 ***                       adecl(a,n)          ***
 *************************************************
 *** d > 0, ad == U4MAX : apar(loc,d-d2)       ***
 *** d > 0, ad != U4MAX : adecl(ad,d-d2)       ***
 *** d == 0             : ref(ad)              ***
 ************************************************* */

static state_ptr ref_chase(u4_int olda, state_ptr r, u1_int num, state_ptr c) {

	if ((r) && (r->kind == S_REF)) {

		u4_int ad = r->u.ref.ad;
		u1_int d = r->u.ref.d;
		u2_int l = r->u.ref.loc;
		u1_int d2 = r->u.ref.d2 + num;

		if (d != 0u) {

			/* CHASED BACK TO apar(loc,d) or adecl(ad,d)
			 ***************************************** */

			if (d2 < d) {

				/* Record Chased Array Reference in a FRESH copy */

				state_ptr nw = new_stack_state(S_REF, ad, 0);
				nw->u.ref.sig = r->u.ref.sig;
				nw->u.ref.loc = l;
				nw->u.ref.d = d;
				nw->u.ref.d2 = d2;
				nw->u.ref.set = r->u.ref.set;

				return nw;
			} else if (d2 > d)
				javab_out(1, "deep chasing...");

		} else {

			/* CHASED BACK TO ref(ad)
			 ********************** */

			u2_int sp = att->sp_before[ad];
			state_ptr *stack = att->st_state[ad];

			switch (byt[ad]) {

			case 50u: /* aaload */

				/* CHASED BACK TO AN ARRAY REFERENCE
				 ********************************* */

				if (sp >= 2u) {

					state_ptr s = stack[sp - 2];

					if (s) {
						if ((s->kind == S_EXP) && (s->u.exp.con != 0))
							javab_out(0, "constant used as array"
									" reference at %u from %u", ad, olda);
						else if (s->kind == S_REF)
							return ref_chase(olda, s, num + 1u, c);
					}
				} else
					javab_out(0, "incorrect stack state for aaload at %u",
							olda);
				break;

			case 188u: /* newarray       */
			case 189u: /* anewarray      */
			case 197u: /* multianewarray */

				/* CHASED BACK TO AN ARRAY DECLARATION
				 *********************************** */

				d = (byt[ad] == 197u) ? byt[ad + 3u] : 1u;

				if ((sp >= d) && (num < d)) {

					/* Record Chased Array Reference in a FRESH copy */

					state_ptr nw = new_stack_state(S_REF, ad, 0);
					nw->u.ref.d = d;
					nw->u.ref.d2 = d2;

					det_sig(nw, byt[ad], byt[ad + 1u], byt[ad + 2u]);

					return nw;
				} else if (d2 > d)
					javab_out(1, "deep chasing...");
				break;

			default:
				javab_out(-1, "cannot chase reference %u at %u from %u", opc,
						ad, olda);
			}
		}
	} else
		javab_out(-1, "incorrect invocation of ref_chase() from %u", olda);

	return new_stack_state(S_BOT, 0u, 0);
}

/* Chase Array References */

static u1_int array_pre_chase(u4_int i) {
	u4_int j; /* wide counter */

	for (j = 0u; j < glo_sta; j++) {
		state_ptr s = att->st_state[i][j];
		if ((s) && (s->kind == S_EXP) && (s->u.exp.con == 0) && (s->u.exp.rd))
			att->st_state[i][j] = chase1(s);
	}
	return 0u;
}

static u1_int array_ref_chase(u4_int i) {
	u4_int j; /* wide counter */

	for (j = 0u; j < glo_sta; j++) {
		state_ptr s = att->st_state[i][j];
		if ((s) && (s->kind == S_REF) && (s->u.ref.d == 0u))
			att->st_state[i][j] = ref_chase(i, s, 0u, s);
	}
	return 0u;
}

/* ********************************
 *** getfield on `this' check ***
 ******************************** */

static u1_int field_exception(state_ptr r) {

	if ((r) && (r->kind == S_EXP) && (r->u.exp.con == 0) && (is_instm)) {
		u4_int i, last; /* wide counter */
		u2_int cnt = 0u;
		u1_int *rd = r->u.exp.rd;

		for (i = 0u; i < rd_cnt; i++)
			if (rd[i]) {
				last = i;
				cnt++;
			}

		if ((cnt == 1u) && (rd_add[last] == U4MAX) && (rd_loc[last] == 0u)) {

			/* unique rd of `this' word
			 ************************ */

			return 0u;
		}
	}
	return 1u;
}

/* ******************************
 *** Trivial Loop Detection ***
 ****************************** */

static u1_int triv_loop(u4_int i) {
	bb_ptr b = att->my_bb[i];

	if (!b)
		javab_out(-1, "lost basic block in triv_loop()");

	if (triv_lp->nl[b->name]) { /* Instruction is in Loop
	 ********************** */
		if (i < triv_lp->min_ad)
			triv_lp->min_ad = i;

		if ((next - 1u) > triv_lp->max_ad)
			triv_lp->max_ad = (next - 1u);

		if ((b == triv_lp->d) && (i == b->low)) { /* begin basic block of entry
		 ************************** */
			if (att->sp_before[i] != 0u) {
				triv_lp->par = 0u;
				javab_out(2, "    - PAR-FAIL: non-empty stack on entry %u", i);
			}
		} /* end begin bb of entry */

		if ((b == triv_lp->b)
				&& (((next - 1u) == b->high) || (byt[next] == 200u)
						|| (byt[next] == 167u))) { /* effective end of bb in
				 last bb before back-edge of loop
				 (goto/goto_w back accounted for)
				 *********************************** */
			if (opc == 132u) { /* iinc at end */
				u2_int loc =
						(is_wide) ?
								B2U2(byt[i + 1], byt[i + 2]) :
								(u1_int) byt[i + 1];
				s2_int val =
						(is_wide) ?
								B2S2(byt[i + 3], byt[i + 4]) :
								(s1_int) byt[i + 2];

				if (val > 0) {
					triv_lp->ind_is_w = is_wide;
					triv_lp->ind_loc = loc;
					triv_lp->ind_add = i;
					triv_lp->ind_step = (u2_int) val;

					if ((!is_wide) && ((my_not * val) > U1MAX)) {
						triv_lp->par = 0u;
						javab_out(2, "    - PAR-FAIL: new stride does not fit");
					}
				} else {
					triv_lp->triv = 0u;
					javab_out(2, "    - TRIV-FAIL: non-positive stride at %u",
							i);
				}
			} else if (triv_lp->ind_add == 0u) {
				triv_lp->triv = 0u;
				javab_out(2, "    - TRIV-FAIL: no iinc at end %u", i);
			}
		} /* end effective end of bb in last bb */

		if ((b == triv_lp->d) && ((next - 1u) == b->high)) { /* end bb of entry
		 *************** */
			if (!att->st_state[i])
				javab_out(-1, "lost stack state information in triv_loop()");

			if ((161u <= opc) && (opc <= 164u)) {

				/* Supports `if_icmp<cond>',
				 (instructions if<cond> could be
				 supported in a future implementation) */

				/* **************************************
				 *** Trivial Loop Entry Examination ***
				 *** 0:< 1:>= 2:> 3:<=              ***
				 ************************************** */

				u1_int neg = (triv_lp->nl[att->my_bb[target]->name]) ? 0u : 1u;
				u1_int cmp = opc - 161u;

				u2_int sp = att->sp_before[i];
				state_ptr lhs = (sp >= 2u) ? att->st_state[i][sp - 2u] : NULL;
				state_ptr rhs = (sp >= 1u) ? att->st_state[i][sp - 1u] : NULL;

				if (sp != 2u) {
					triv_lp->par = 0u;
					javab_out(2,
							"    - PAR-FAIL: non-empty stack on exit at %u", i);
				}

				if (neg) { /* true-false branch negation */

					triv_lp->cmp_ad = i;

					/* Unlikely to succeed, but included anyway */

					if (!triv_lp->nl[att->my_bb[next]->name]) {
						triv_lp->triv = 0u;
						javab_out(2, "    - TRIV-FAIL: no loop entry at %u", i);
					} else
						triv_lp->exit_ad = target;

					/* Negate Compare Operator */

					switch (cmp) {
					case 0u:
						cmp = 1u;
						break;
					case 1u:
						cmp = 0u;
						break;
					case 2u:
						cmp = 3u;
						break;
					case 3u:
						cmp = 2u;
						break;
					}
				} else {

					triv_lp->cmp_ad = 0u; /* Fall-Through-Exit-Loop */

					/* Reject Infinite Loops */

					if (triv_lp->nl[att->my_bb[next]->name]) {
						triv_lp->triv = 0u;
						javab_out(2, "    - TRIV-FAIL: no loop exit at %u", i);
					} else
						triv_lp->exit_ad = next;
				}

				if ((lhs) && (lhs->kind == S_EXP) && (rhs)
						&& (rhs->kind == S_EXP)) {

					if ((triv_lp->compare) || (triv_lp->up_bnd))
						javab_out(-1, "illegal reset in triv_loop()");

					if (cmp == 0u) { /* operator < */
						triv_lp->compare = lhs;
						triv_lp->up_bnd = rhs;
						triv_lp->strict = 1u;
					} else if (cmp == 2u) { /* operator > */
						triv_lp->compare = rhs;
						triv_lp->up_bnd = lhs;
						triv_lp->strict = 1u;
					} else if (cmp == 3u) { /* operator <= */
						triv_lp->compare = lhs;
						triv_lp->up_bnd = rhs;
						triv_lp->strict = 0u;
					} else if (cmp == 1u) { /* operator >= */
						triv_lp->compare = rhs;
						triv_lp->up_bnd = lhs;
						triv_lp->strict = 0u;
					} else
						javab_out(-1, "something wrong with cmp=%u\n", cmp);

				} else {
					triv_lp->triv = 0u;
					javab_out(2,
							"    - TRIV-FAIL: complex operands of compare at %u",
							i);
				}
			} else {
				triv_lp->triv = 0u;
				javab_out(2, "    - TRIV-FAIL: no ifcmp<cond> instr at %u", i);
			}

		} /* end bb of entry test */

		/* Check for prohibited Instructions
		 ********************************* */

		switch (opc) {

		case 172u:
		case 173u:
		case 174u:
		case 175u:
		case 176u:
		case 177u: /* <T>return : should probably not occur */

			triv_lp->par = 0u;
			javab_out(1, "%s in trivial loop at %u", mem, i);
			break;

		case 179u: /* putstatic */
		case 181u: /* putfield */

		case 182u: /* invokevirtual   */
		case 183u: /* invokespecial   */
		case 184u: /* invokestatic    */
		case 185u: /* invokeinterface */
		case 186u: /* invokedynamic   */
		{
			u2_int e = B2U2(byt[i + 1u], byt[i + 2u]);
			u2_int c1 = constant_pool[e]->u.indices.index1;
			u2_int n = constant_pool[e]->u.indices.index2;
			u2_int d = constant_pool[n]->u.indices.index1;
			u2_int c2 = constant_pool[c1]->u.indices.index1;

			char *cla = (char *) constant_pool[c2]->u.utf8.s;
			char *met = (char *) constant_pool[d]->u.utf8.s;

			/* List of side-effect free static functions ->
			 can be extended in a future implementation
			 ****************************************** */

			if ((opc == 184u) && (strcmp(cla, "java/lang/Math") == 0u)
					&& (strcmp(met, "log") != 0u) && (strcmp(met, "pow") != 0u)
					&& (strcmp(met, "sqrt") != 0u)
					&& (strcmp(met, "random") != 0u)) {
				javab_out(2, "    - %s.%s is side-effect free", cla, met);
			} else {
				triv_lp->par = 0u;
				javab_out(2, "    - PAR-FAIL: %s %s.%s at %u", mem, cla, met,
						i);
			}
			break;
		}
		}

		if ((triv_lp->par) && (exc) && ((s_linking) || (exc <= 2u))) {

			/* Determine if RUN-TIME exceptions
			 (and possibly LINKING exceptions) may leave loop
			 and Collect ARRAY REFERENCES
			 *********************************************** */

			switch (opc) {

			case 46u:
			case 47u:
			case 48u:
			case 49u:
			case 50u:
			case 51u:
			case 52u:
			case 53u: /* <t>aload */

			{
				u2_int sp = att->sp_before[i];
				state_ptr rf = att->st_state[i][sp - 2u];
				state_ptr in = att->st_state[i][sp - 1u];

				triv_lp->refs = new_ref(i, rf, in, triv_lp->refs, 0u);
			}
				break;

			case 79u:
			case 80u:
			case 81u:
			case 82u:
			case 83u:
			case 84u:
			case 85u:
			case 86u: /* <t>astore */

			{
				u2_int sp = att->sp_before[i];
				state_ptr rf, in;

				if ((opc == 80u) || (opc == 82u)) { /* dastore/lastore */
					rf = att->st_state[i][sp - 4u];
					in = att->st_state[i][sp - 3u];
				} else {
					rf = att->st_state[i][sp - 3u];
					in = att->st_state[i][sp - 2u];
				}
				triv_lp->refs = new_ref(i, rf, in, triv_lp->refs, 1u);
			}
				break;

			case 108u: /* idiv */
			case 112u: /* irem */

			{
				u2_int sp = att->sp_before[i];
				state_ptr s = att->st_state[i][sp - 1u];

				if ((!s) || (s->kind != S_EXP) || (s->u.exp.rd)
						|| (s->u.exp.con == 0u)) {
					triv_lp->par = 0u;
					javab_out(2, "    - PAR-FAIL: %s at %u may exit loop", mem,
							i);
				}
			}
				break;

			case 180u: /* getfield  */

			{
				u2_int sp = att->sp_before[i];
				state_ptr s = (sp >= 1u) ? att->st_state[i][sp - 1u] : NULL;

				if ((s_linking) || (field_exception(s))) {
					triv_lp->par = 0u;
					javab_out(2, "    - PAR-FAIL: %s at %u may exit loop"
							" (not on this)", mem, i);
				}
			}
				break;

			default:
				triv_lp->par = 0u;
				javab_out(2, "    - PAR-FAIL: %s at %u may exit loop", mem, i);
			}

		} /* end exc test */

		if ((triv_lp->triv) && (triv_lp->par)
				&& ((exc == 2u) || (exc == 4u) || (opc == 20u))) { /* uses cp */

			/* MARK USED CP-Entries
			 ******************** */

			u2_int index =
					(opc == 18u) ?
							byt[i + 1u] : (B2U2(byt[i + 1u], byt[i + 2u]));

			mark_shadow_cp(index);
		}

	} /* end in-loop test */

	return (!triv_lp->triv); /* stop on non-triviality */
}

/***********************
 *** SHOW bytecode   ***
 ***********************/

static u1_int show_bytecode(u4_int i) {

//	fprintf(stderr, "\nV%u\n", (*(att->my_bb))->name);
	fprintf(stderr, "%s%c[%2u]%3u:%c %s ",
			(att->is_leader[i]) ? " ----------\n" : "", (exc) ? '*' : ' ',
			att->sp_before[i], i, (is_wide) ? 'W' : ' ', mem);

	if (HAS_TARGET(bra)) { /* show target */

		fprintf(stderr, "%u", target);

	} else if ((exc == 2u) || (exc == 4u) || (opc == 20u)) { /* show cp entry */

		u2_int index =
				(opc == 18u) ? byt[i + 1u] : (B2U2(byt[i + 1u], byt[i + 2u]));

		fprintf(stderr, "#%u ", index);
		show_cp_entry(constant_pool[index]);

	} else if (opc == 170u) { /* tableswitch */

		u4_int j, lf = i + glo_pad + 13u, lb = glo_hig - glo_low + 1u;

		fprintf(stderr, "%i,%i (padding=%u)", glo_low, glo_hig, glo_pad);

		for (j = 0u; j < lb; j++, lf += 4u) {
			s4_int loc_off = B2S4(byt[lf], byt[lf + 1u], byt[lf + 2u],
					byt[lf + 3u]);
			fprintf(stderr, "\n              ::%u", (u4_int) (i + loc_off));
		}

		fprintf(stderr, "\n       default::%u", (u4_int) (i + glo_def));
	} else if (opc == 171u) { /* lookupswitch */

		u4_int j, lf = i + glo_pad + 9u, lb = glo_npa;

		fprintf(stderr, "%i (padding=%u)", glo_npa, glo_pad);

		for (j = 0u; j < lb; j++, lf += 8u) {
			s4_int loc_mat = B2S4(byt[lf], byt[lf + 1u], byt[lf + 2u],
					byt[lf + 3u]);
			s4_int loc_off = B2S4(byt[lf + 4u], byt[lf + 5u], byt[lf + 6u],
					byt[lf + 7u]);
			fprintf(stderr, "\n      %5i::%u", loc_mat, (u4_int) (i + loc_off));
		}

		fprintf(stderr, "\n    default::%u", (u4_int) (i + glo_def));
	} else if (opc == 188u) { /* newarray */

		u1_int tp = byt[i + 1];

		if ((4 <= tp) && (tp <= 11))
			fprintf(stderr, "%s", newarray_tab[tp - 4]);
		else
			javab_out(0, "incorrect type %u in %s", tp, mem);

	} else if ((opc == 16u) || (opc == 17u)) { /* bipush/sipush */

		s2_int s =
				(opc == 16u) ?
						((s1_int) byt[i + 1]) : (B2S2(byt[i + 1], byt[i + 2]));

		fprintf(stderr, "%i ", s);
	} else { /* show operands in signed decimal */

		u2_int j;

		for (j = i + 1u; j < next; j++)
			fprintf(stderr, "%i ", (s1_int) byt[j]);
	}

	if (s_stack) {
		u2_int j;
		fprintf(stderr, "\t\t");
		for (j = 0u; j < glo_sta; j++)
			dump_sta(att->st_state[i][j]);
	}

	fputc('\n', stderr);

	return 0u;
}

/* *******************************************************
 *** Process a Single Code Attributes in .class file ***
 ******************************************************* */

static void byte_codeattr(attribute_ptr a, u2_int w_arg, u1_int *nm, u1_int *tp,
		u2_int w_res) {
	u4_int i; /* wide_counter */
	u1_int *bytes = a->info;
	u2_int max_stack = B2U2(bytes[0], bytes[1]);
	u2_int max_locals = B2U2(bytes[2], bytes[3]);
	u4_int code_length = B2U4(bytes[4], bytes[5], bytes[6], bytes[7]);
	u2_int exc_table_l;

	if (a->attribute_length < 12u + code_length) {
		javab_out(0, "corrupt code atttribute given for %s%s code_length = %u",
				nm, tp, code_length);
		return;
	}

	exc_table_l = B2U2(bytes[8 + code_length], bytes[9 + code_length]);

	if (code_length + 10u + exc_table_l * 8u >= a->attribute_length) {
		javab_out(0, "corrupt exception handler table");
		return;
	}

	/* Set global attribute (for all subsequent processing!)
	 ***************************************************** */

	att = a;

	if (s_disassem) {

		fprintf(stderr, "    max_stack   = %u\n", max_stack);
		fprintf(stderr, "    max_locals  = %u\n", max_locals);
		fprintf(stderr, "    code_length = %u\n", code_length);

		for (i = 0u; i < exc_table_l; i++) {
			u4_int b = code_length + 10u + i * 8u;
			u2_int start_pc = B2U2(bytes[b], bytes[b + 1u]);
			u2_int end_pc = B2U2(bytes[b + 2u], bytes[b + 3u]);
			u2_int handler_pc = B2U2(bytes[b + 4u], bytes[b + 5u]);
			u2_int catch_type = B2U2(bytes[b + 6u], bytes[b + 7u]);

			fprintf(stderr, "    [%u,%u> -> %u %s\n", start_pc, end_pc,
					handler_pc,
					(catch_type) ?
							(char *) constant_pool[constant_pool[catch_type]->u.indices.index1]->u.utf8.s :
							"any");
		}
	}

	/* Quit for empty method body (or for large codelength)
	 or in case too many parameters are passed to method
	 **************************************************** */

	if (code_length == 0u) {
		javab_out(2, "  + empty method %s()", nm);
		return;
	} else if (code_length >= (U4MAX - 1)) {
		javab_out(2,
				"  + skipping method %s() (cannot be processed internally)",
				nm);
		return;
	} else if (w_arg > max_locals) {
		javab_out(0, "%u parameter words exceed %u local words of method %s()",
				w_arg, max_locals, nm);
		return;
	}

	/* Allocate Memory for BYTECODE Information
	 **************************************** */

	a->code_length = code_length;
	a->is_leader = (u1_int *) make_mem((code_length + 1) * sizeof(u1_int));
	a->my_bb = (bb_ptr *) make_mem((code_length + 1) * sizeof(bb_ptr));
	a->reachable = (u1_int *) make_mem(code_length * sizeof(u1_int));
	a->sp_before = (u2_int *) make_mem(code_length * sizeof(u2_int));
	a->st_state = (state_ptr **) make_mem(code_length * sizeof(state_ptr *));

	for (i = 0u; i <= code_length; i++) {
		a->is_leader[i] = 0u;
		a->my_bb[i] = NULL;
	}
	for (i = 0u; i < code_length; i++) {
		a->reachable[i] = 2u;
		a->sp_before[i] = 0u;
		a->st_state[i] = NULL;
	}

	/* Compute Stack Information:
	 traverse entry point of method     (with sp==0 on entry)
	 *and* entry point of every handler (with sp==1 on entry)
	 ******************************************************** */

	glo_sta = max_stack;
	glo_stm = 0u;
	glo_loc = max_locals;

	/* Empty Stack */

	cur_sp = 0u;

	byte_trav(0u, comp_sp_info);

	for (i = 0u; i < exc_table_l; i++) {
		u4_int b = code_length + 10u + i * 8u;
		u2_int handler_pc = B2U2(bytes[b + 4u], bytes[b + 5u]);

		/* Exception Reference on Stack */

		cur_sp = 1u;

		byte_trav((u4_int) handler_pc, comp_sp_info);
	}

	if (glo_stm < glo_sta)
		javab_out(1, "stack size %u in %s() can be reduced to %u", glo_sta, nm,
				glo_stm);

	/* Check Unreachable Code
	 ********************** */

	reportu = 1u;

	byte_trav(0u, unreachable);

	if (error_1)
		return;

	/* Compute and Construct Basic Blocks:
	 is_leader information [Aho, 9.4]
	 entry point is leader *and* entry point of every handler
	 ******************************************************** */

	cur_bb = NULL;

	a->is_leader[0u] = 1u;

	for (i = 0u; i < exc_table_l; i++) {
		u4_int b = code_length + 10u + i * 8u;
		u2_int handler_pc = B2U2(bytes[b + 4u], bytes[b + 5u]);
		a->is_leader[handler_pc] = 1u;
	}

	max_seen = 0u;

	byte_trav(0u, comp_leaders);

	if (a->is_leader[max_seen] == 0u)
		javab_out(1, "code may fall off end at %u", max_seen, code_length);
	else if (!error_1) {

		/* *******************************
		 *** Basic Block Computation ***
		 ******************************* */

		/* Compute basic blocks
		 ******************** */

		byte_trav(0u, comp_bb1);
		byte_trav(0u, comp_bb2);
		byte_trav(0u, comp_bb3);

		/* Compute reaching definition and use information
		 *********************************************** */

		rd_pass = 1;
		rd_cnt = w_arg;
		use_cnt = 0u;

		byte_trav(0u, comp_rd_u_s);

		rd_loc = (u2_int *) make_mem(rd_cnt * sizeof(u2_int));
		rd_add = (u4_int *) make_mem(rd_cnt * sizeof(u4_int));

		use_loc = (u2_int *) make_mem(use_cnt * sizeof(u2_int));
		use_add = (u4_int *) make_mem(use_cnt * sizeof(u4_int));
		use_typ = (u1_int *) make_mem(use_cnt * sizeof(u1_int));

		rd_pass = 2;
		rd_cnt = w_arg;
		use_cnt = 0u;

		/* Set Argument Definitions
		 ************************ */

		for (i = 0u; i < w_arg; i++) {
			rd_loc[i] = (u2_int) i;
			rd_add[i] = U4MAX;
		}

		/* Set other Definitions and Uses
		 ****************************** */

		byte_trav(0u, comp_rd_u_s);

		/* Solve First Set of Data Flow Equations
		 ************************************** */

		bb_first(rd_cnt, rd_loc, rd_add, rd_sig, w_arg, use_cnt, use_loc,
				use_add, use_typ, glo_sta);

		if (error_1)
			goto release;

		/* Compute GLOBAL stack state
		 ************************** */

		s_states = new_stack_tuple(glo_sta);

		byte_trav(0u, comp_stack_state);

		if (error_1)
			goto release;

		/* Solve Second Set of Data Flow Equations
		 *************************************** */

		bb_second(nm);

		/* Compute LOCAL stack state
		 ************************* */

		for (i = 0u; i < glo_sta; i++)
			s_states[i] = NULL;

		byte_trav(0u, all_stack_state);

		{
			u2_int pass = 0u, ctot = 0u;

			do {

				pass++;

				cc_cnt = 0u;

				/* Copy/Constant Propagation
				 ************************* */

				byte_trav(0u, simple_prop);

				/* progate changes locally to improve
				 any subsequent constant folding...
				 (could be done globally as well)
				 ******************************** */

				if (cc_cnt) {
					my_prop = 0u;
					byte_trav(0u, prop_stack_state);
				}

				ctot += cc_cnt;

			} while ((cc_cnt) && (pass < 50u));

			if (pass == 50u)
				javab_out(1,
						"quitting constant/copy propagation -> too many passes");

			if (ctot)
				javab_out(2, "  + copy/constant propagation"
						" (%u, passes: %u)", ctot, pass);
		}

		/* Array Reference Chasing
		 *********************** */

		byte_trav(0u, array_pre_chase);
		byte_trav(0u, array_ref_chase);

		/* Show bytecode
		 ************* */

		if (s_disassem) {
			byte_trav(0u, show_bytecode);
			fprintf(stderr, " ----------\n");
		}

		/* Parallelization
		 **************** */

		if ((s_par) && (!error_1)) {

			javab_out(2, "  + parallelization");

			bb_par(a, nm, tp);
		}

		/* Release Memory
		 ************** */

		release: release_memory();
	}
}

/* ********************************************************
 *** PUBLIC FUNCTIONS                                 ***
 ******************************************************** */

/* *******************
 *** Stack State ***
 *********************************
 *** S_EXP: CON v==U4MAX, c!=0 ***
 ***        VAR v!=U4MAX, c==0 ***
 *** S_REF: REF v==addr,  c==0 ***
 ********************************* */

state_ptr new_stack_state(u1_int kind, u4_int v, s4_int c) {
	u4_int i; /* wide counter */
	state_ptr n = (state_ptr) make_mem(sizeof(struct state_node));

	n->kind = kind;
	n->prop = 0u;
	n->gnext = g_states;
	g_states = n;

	switch (kind) {

	case S_EXP:
		n->u.exp.con = c;

		if (v != U4MAX) {
			u1_int *r = (u1_int *) make_mem(rd_cnt * sizeof(u1_int));
			for (i = 0u; i < v; i++)
				r[i] = 0u;
			n->u.exp.rd = r;
		} else
			n->u.exp.rd = NULL;
		break;

	case S_REF:
		n->u.ref.sig = NULL;
		n->u.ref.ad = v;
		n->u.ref.loc = 0u;
		n->u.ref.d = 0u;
		n->u.ref.d2 = 0u;
		n->u.ref.set = 0u;
		break;

	case S_BOT:
		break;

	default:
		javab_out(-1, "invalid kind %u given in new_stack_state()", kind);
	}
	return n;
}

/* ******************************
 *** Trivial Loop Detection ***
 ****************************** */

void check_triv_loop(loop_ptr l) {

	triv_lp = l;

	if (triv_lp)
		javab_out(2, "  + checking candidate loop v%u->v%u", l->b->name,
				l->d->name);
	else
		javab_out(-1, "lost candidate loop");

	/* Code Analysis
	 ************* */

	triv_lp->min_ad = triv_lp->d->low;
	triv_lp->max_ad = triv_lp->d->high;

	byte_trav(0u, triv_loop);

	/* Compare-Analysis
	 **************** */

	if (triv_lp->triv) {

		if ((!triv_lp->compare) || (triv_lp->compare->kind != S_EXP)
				|| (!triv_lp->compare->u.exp.rd)) {

			triv_lp->triv = 0u;
			javab_out(2, "    - TRIV-FAIL: missing loop-index as operand");

		} else {

			u4_int i; /* wide counter */
			u1_int set = 0u, *rd;
			u2_int lcnt = 0u;

			/* Check Reaching Definitions of Compare Operand
			 ********************************************* */

			rd = triv_lp->compare->u.exp.rd;

			for (i = 0u; i < rd_cnt; i++) {
				if (rd[i]) {

					u4_int a = rd_add[i];
					bb_ptr b = (a != (U4MAX)) ? att->my_bb[a] : NULL;

					/* Check if 'iinc' is only inner loop def. reaching lhs test */

					if ((b) && (triv_lp->nl[b->name])) { /* rd from inside loop */
						if (a == triv_lp->ind_add)
							set = 1u;
						else {
							triv_lp->triv = 0u;
							javab_out(2, "    - TRIV-FAIL: inner-loop reaching"
									" definition of index at %u", a);
							break;
						}
					} else { /* rd from outside loop */

						lcnt++;

						if (lcnt == 1u) {

							if ((a != U4MAX) && (byt[a] != 132u)) { /* true store-def. */

								u4_int sp = att->sp_before[a];

								triv_lp->lw_bnd =
										(sp >= 1u) ? att->st_state[a][sp - 1u] :
										NULL;
							}
						}
					}
				}
			} /* end for-loop */

			if ((triv_lp->triv) && (!set)) {
				triv_lp->triv = 0u;
				javab_out(2,
						"    - TRIV-FAIL: iinc does not reach compare operand");
			} else if ((triv_lp->triv)
					&& (((lcnt != 1u) || (!triv_lp->lw_bnd))
							|| (triv_lp->lw_bnd->kind != S_EXP))) {
				triv_lp->triv = 0u;
				javab_out(2, "    - TRIV-FAIL: unknown lower bound");
			} else if (triv_lp->triv) {

				/* Check if there no other inner loop def. of the loop index
				 ********************************************************* */

				for (i = 0u; i < rd_cnt; i++)
					if (rd_loc[i] == triv_lp->ind_loc) {
						u4_int a = rd_add[i];
						bb_ptr b = (a != (U4MAX)) ? att->my_bb[a] : NULL;

						/* rd in loop other than iinc -> non-trivial loop */

						if ((b) && (triv_lp->nl[b->name])
								&& (a != triv_lp->ind_add)) {
							triv_lp->triv = 0u;
							javab_out(2, "    - TRIV-FAIL: definition of loop"
									" index at %u", a);
						}
					}

				/* Check if upper bound is loop-invariant
				 ************************************** */

				if ((triv_lp->up_bnd) && (triv_lp->up_bnd->kind == S_EXP)
						&& (triv_lp->up_bnd->u.exp.rd)) {

					rd = triv_lp->up_bnd->u.exp.rd;

					for (i = 0u; i < rd_cnt; i++) {
						if (rd[i]) {
							u4_int a = rd_add[i];
							bb_ptr b = (a != (U4MAX)) ? att->my_bb[a] : NULL;

							if ((b) && (triv_lp->nl[b->name])) { /* rd from inside loop */
								triv_lp->triv = 0u;
								javab_out(2, "    - TRIV-FAIL: upper bound "
										"loop-variant at %u", a);
							}
						}
					}
				}
			}
		}
	} /* end compare-analysis */

	if (triv_lp->triv) {

		/* Set Local-Load-Type Information
		 ******************************* */

		u4_int i; /* wide counter */

		u1_int *li = (u1_int *) make_mem(glo_loc * sizeof(u1_int));
		char **ls = (char **) make_mem(glo_loc * sizeof(char *));

		for (i = 0u; i < glo_loc; i++) {
			li[i] = TP_UNUSED;
			ls[i] = NULL;
		}

		/* Mark Use of Initial Value Loop Index */

		li[triv_lp->ind_loc] = TP_INT;
		ls[triv_lp->ind_loc] = strdup("I");

		triv_lp->load_type = li;
		triv_lp->load_sig = ls;
		triv_lp->load_locs = glo_loc;

		/* Scalar Data Dependence Analysis
		 ******************************* */

		scalar_dep();
	}
}

/* ***************************
 *** Bytecode Processing ***
 *************************** */

void byte_proc(void) {

	u4_int i, j; /* wide counters */

#ifdef CHECK_TABLE

	/* Verify bytecode table */

	for (i = 0u; i < 256u; i++)
		if (bytecode[i].opcode != i)
			javab_out(-1, "invalid bytecode initialization at %u", i);

#endif

	/* Scan over methods, and process code-attributes */

	for (i = 0u; i < methods_count; i++) {

		fm_ptr m = methods[i];
		u1_int *nm = constant_pool[m->name_index]->u.utf8.s;
		u1_int *tp = constant_pool[m->descr_index]->u.utf8.s;
		u1_int is_inst = (m->access_flags & ACC_STATIC) ? 0u : 1u;
		attribute_ptr my_code = NULL;
		attribute_ptr my_exc = NULL;

		// uncomment following line to select a single method for
		// analysis. Here mult is selected for analysis. This can be
		// used to analyse only hot methods.
//		if (strstr((char*)nm, "mult") != NULL) {
			char *this_arg_type = NULL;

			/* Determine number of locals that are defined
			 (for Instance Methods: `this' is first-word argument)
			 and number of words pushed back on the caller's operand stack */

			u2_int w_arg = arg_width(tp, (is_inst) ? 2u : 1u);
			u2_int w_res = res_width(tp);

			if (is_inst) /* Determine type of `this': set to java.lang.Object */{

				u2_int e = constant_pool[this_class]->u.indices.index1;
				char *s = (char *) constant_pool[e]->u.utf8.s;
				u2_int l = strlen(s);

				/* Bug fix, allocated space was to small (courtesy Fabian Breg) */

				this_arg_type = (char *) make_mem((l + 4u) * sizeof(char));
				sprintf(this_arg_type, "L%s;", s);
				rd_sig[0u] = this_arg_type;
			}

			is_instm = is_inst;

			javab_out(2, "  - processing %s method %s()",
					(is_inst) ? "instance" : "class", nm);

			if (s_disassem)
				fprintf(stderr, "\n\n*** method 0x%02x %s%s %u->%u\n",
						m->access_flags, nm, tp, w_arg, w_res);

			/* Scan Attributes */

			for (j = 0u; j < m->attributes_count; j++) {
				attribute_ptr a = m->attributes[j];
				constant_ptr ua = constant_pool[a->attribute_name_index];

				if (strcmp((char *) ua->u.utf8.s, "Code") == 0) {
					if (my_code)
						javab_out(0, "multiple code attributes given for %s()",
								nm);
					else
						my_code = a;
				} else if (strcmp((char *) ua->u.utf8.s, "Exceptions") == 0) {
					if (my_exc)
						javab_out(0,
								"multiple exception attributes given for %s()",
								nm);
					else
						my_exc = a;
				}
			}

			/* Process Exception Attribute */

			if ((s_disassem) && (my_exc)) {
				u1_int *locb = my_exc->info;
				u2_int num_of_exc = B2U2(locb[0], locb[1]);

				for (j = 0u; j < num_of_exc; j++) {
					u2_int entry = B2U2(locb[2u + 2u * j], locb[3u + 2u * j]);

					fprintf(stderr, "                   throws ");
					show_cp_entry(constant_pool[entry]);
					fputc('\n', stderr);
				}
			}

			/* Process Code Attribute */

			if (my_code) {
				if (my_code->attribute_length < 12u)
					javab_out(0, "corrupt code attribute given for %s()", nm);
				else
					byte_codeattr(my_code, w_arg, nm, tp, w_res);
			} else
				javab_out(2, "  + no code attribute given for %s()", nm);

			if (this_arg_type)
				free(this_arg_type);

			if (error_1)
				break; /* otherwise, a list of method
				 headers appears for switch `-d' */

//		}
	}
}
