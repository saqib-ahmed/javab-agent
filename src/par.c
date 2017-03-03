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

#include "class.h"

/* ********************************************************
 *** EXTERNAL VARIABLES                               ***
 ******************************************************** */

/* PRIVATE
 ******* */

static u1_int *new_bytes = NULL;
static u4_int new_pos = 0u;
static u4_int new_len = 0u;
static u4_int save_handler;
static u2_int worker_entry;
static u2_int stack_inc;
static worker_ptr all_workers;

/* ********************************************************
 *** PRIVATE FUNCTIONS                                ***
 ******************************************************** */

/* Add a byte to new bytecode
 ************************** */

static void add_byte(u1_int b) {

	if (new_pos == new_len) {

		/* Allocate More Bytes
		 ******************* */

		new_len += 100u;

		if (new_bytes)
			new_bytes = (u1_int *) more_mem(new_bytes,
					new_len * sizeof(u1_int));
		else
			new_bytes = (u1_int *) make_mem(new_len * sizeof(u1_int));
	}

	new_bytes[new_pos++] = b;
}

/* Release Memory of new Code
 ************************** */

static void free_new_bytes(void) {
	if (new_bytes)
		free(new_bytes);
	new_bytes = NULL;
	new_pos = 0;
	new_len = 0;
}

/* Release Memory of Workers
 ************************* */

static void del_workers(void) {
	while (all_workers) {
		worker_ptr n = all_workers->next;

		if (all_workers->qualified_name)
			free(all_workers->qualified_name);
//    if (all_workers -> constr_d)
//      free(all_workers -> constr_d);

		if (all_workers->load_type)
			free(all_workers->load_type);
		if (all_workers->load_ind)
			free(all_workers->load_ind);

		if (all_workers->load_sig) {
			u4_int i;
			char **s = all_workers->load_sig;
			for (i = 0u; i < all_workers->max_locals; i++)
				if (s[i])
					free(s[i]);
			free(s);
		}

		free(all_workers);
		all_workers = n;
	}
}

/* *********************************************
 *** Generate Appropriate Goto Instruction ***
 ********************************************* */

static void spit_goto(s4_int off) {

	s2_int off2 = (s2_int) off;

	if (off == (s4_int) off2) { /* fits in 2 bytes */
		add_byte(167u); /* goto */
		add_byte(HIGB_U2(off));
		add_byte(LOWB_U2(off));
	} else {
		add_byte(200u); /* goto_w */
		add_byte(HIGB_U4(off));
		add_byte(LOWB_U4(off));
		add_byte(HIGB_U2(off));
		add_byte(LOWB_U2(off));
	}
}

/* ***************************************************
 *** Generate Appropriate Load/Store Instruction ***
 *************************************************** */

static void spit_ls(u1_int tp, u2_int loc, u1_int ls) {

	u1_int b = ((ls) ? 54u : 21u) + (tp - TP_INT);

	if (loc > U1MAX)
		add_byte(196u); /* wide */

	add_byte(b); /* <t>load/<t>store */

	if (loc > U1MAX) {
		add_byte(HIGB_U2(loc));
		add_byte(LOWB_U2(loc));
	} else
		add_byte((u1_int) loc);
}

/* Make Constructor Signature 
 ************************** */

static void make_sig(worker_ptr w) {
	char **oldsig = w->load_sig, *sig;
	u2_int ml = w->max_locals;
	u4_int k = 0u, i, j;

	for (i = 0u; i < ml; i++)
		if (oldsig[i])
			k += strlen(oldsig[i]);
	sig = (char *) make_mem((k + 4u) * sizeof(char));

	k = 0u;
	sig[k++] = '(';
	for (i = 0u; i < ml; i++)
		if (oldsig[i]) {

			u1_int tp = w->load_type[i];
			u2_int l = strlen(oldsig[i]);

			for (j = 0u; j < l; j++)
				sig[k++] = oldsig[i][j];

			/* Record Stack Increment Due to Parameters
			 **************************************** */

			stack_inc += ((tp == TP_LONG) || (tp == TP_DOUBLE)) ? 2u : 1u;
		}

	sig[k++] = ')';
	sig[k++] = 'V';
	sig[k++] = '\0';

	w->constr_d = sig;
}

/* Construct a new Worker
 ********************** */

static void new_worker(attribute_ptr a, loop_ptr l, u2_int ms, u2_int ml,
		char *method) {
	static u4_int cnt = 0;
	constant_ptr c = constant_pool[this_class];
	constant_ptr u = constant_pool[c->u.indices.index1];
	char *class = (char *) u->u.utf8.s;
	u2_int len = strlen(class) + strlen(method) + 15u;
	char *qn = (char *) make_mem(len * sizeof(char));

	sprintf(qn, "%s_Worker_%s_%u", class, method, cnt++);

	add_cp_entry(CONSTANT_Utf8, qn, 0u, 0u);
	add_cp_entry(CONSTANT_Class, "", constant_pool_count - 1u, 0u);

	worker_entry = constant_pool_count - 1u;

	/* Add a Worker Information Node
	 ***************************** */

	{
		worker_ptr w = (worker_ptr) make_mem(sizeof(struct worker_node));
		u4_int i;
		u1_int *oc = &(a->info[8u + l->min_ad]);

		/* General Info
		 ************ */

		w->qualified_name = qn;

		w->entry_off = l->d->low - l->min_ad;
		w->max_stack = ms;
		w->max_locals = ml;

		if (l->cmp_ad)
			w->exit_off = l->cmp_ad - l->min_ad;
		else
			w->exit_off = 0u;

		w->ind_off = l->ind_add - l->min_ad;
		w->ind_step = l->ind_step;
		w->ind_is_w = l->ind_is_w;

		/* Loop-Body
		 ********* */

		w->l_len = l->max_ad - l->min_ad + 1u;
		w->loop_code = (u1_int *) make_mem(w->l_len * sizeof(u1_int));

		for (i = 0u; i < w->l_len; i++)
			w->loop_code[i] = oc[i];

		/* NOP Old Loop-Code
		 ***************** */

		nop_loop(a, l, w->loop_code);

		/* Copy Load-Type and Load-Signature Information
		 ********************************************* */

		if (l->load_locs != ml)
			javab_out(-1, "lost max locals information in new_worker()");

		w->load_ind = (u2_int *) make_mem(ml * sizeof(u2_int));
		w->load_type = (u1_int *) make_mem(ml * sizeof(u1_int));
		w->load_sig = (char **) make_mem(ml * sizeof(char *));

		for (i = 0u; i < ml; i++) {

			w->load_ind[i] = 0u;
			w->load_type[i] = l->load_type[i];

			if (l->load_type[i] <= (u1_int) TP_2WORD)
				w->load_sig[i] = NULL;
			else if (l->load_type[i] < (u1_int) TP_ERROR) {

				if (l->load_sig[i]) {
					w->load_sig[i] = l->load_sig[i];
					l->load_sig[i] = NULL; /* steal string of loop */
				} else {
					javab_out(1, "unknown type of local %u may cause problems",
							i);
					w->load_sig[i] = strdup("Ljava/lang/Object;");
				}

			} else
				javab_out(-1,
						"should not parallelize loop with unknown local type");
		}

		/* Construct Constructor Signature
		 ******************************* */

		make_sig(w);

		/* Hook to List
		 ************ */

		w->next = all_workers;
		all_workers = w;
	}
}

/* Replace Loop by LoopWorker Invocations
 ************************************** */

static void loop_replacement(loop_ptr l, u4_int clen) {

	u4_int i, j;
	u2_int voidret;
	u2_int method;

	/* Make Method Descriptor "()V"
	 **************************** */

	add_cp_entry(CONSTANT_Utf8, "()V", 0u, 0u);
	voidret = constant_pool_count - 1u;

	/* Make Method Reference for <init>(... sig ...)V
	 ********************************************** */

	add_cp_entry(CONSTANT_Utf8, all_workers->constr_d, 0u, 0u);
	add_cp_entry(CONSTANT_Utf8, "<init>", 0u, 0u);

	add_cp_entry(CONSTANT_NameAndType, NULL, constant_pool_count - 1u,
			constant_pool_count - 2u);

	add_cp_entry(CONSTANT_Methodref, NULL, worker_entry,
			constant_pool_count - 1u);

	method = constant_pool_count - 1u;

	/* Fork all Workers
	 **************** */

	for (i = 0; i < my_not; i++) {

		add_byte(187u); /* new Worker_x */
		add_byte(HIGB_U2(worker_entry));
		add_byte(LOWB_U2(worker_entry));
		add_byte(89u); /* dup */

		/* Pass Parameters
		 *************** */

		for (j = 0u; j < l->load_locs; j++)
			if (l->load_type[j] > (u1_int) TP_2WORD)
				spit_ls(l->load_type[j], j, 0u); /* load */

		add_byte(183u); /* invokespecial */
		add_byte(HIGB_U2(method));
		add_byte(LOWB_U2(method));

		if ((l->ind_loc <= U1MAX) && (l->ind_step <= U1MAX)) {
			add_byte(132u); /* iinc loop-index stride */
			add_byte(l->ind_loc);
			add_byte(l->ind_step);
		} else {
			add_byte(196u); /* wide */
			add_byte(132u); /* iinc loop-index stride */
			add_byte(HIGB_U2(l->ind_loc));
			add_byte(LOWB_U2(l->ind_loc));
			add_byte(HIGB_U2(l->ind_step));
			add_byte(LOWB_U2(l->ind_step));
		}
	}

	/* Make Method Reference for java.lang.Thread.join()V
	 *************************************************** */

	add_cp_entry(CONSTANT_Utf8, "join", 0u, 0u);

	add_cp_entry(CONSTANT_NameAndType, NULL, constant_pool_count - 1, voidret);

	method = constant_pool_count - 1u;

	add_cp_entry(CONSTANT_Utf8, "java/lang/Thread", 0u, 0u);
	add_cp_entry(CONSTANT_Class, "", constant_pool_count - 1u, 0u);

	add_cp_entry(CONSTANT_Methodref, NULL, constant_pool_count - 1u, method);

	method = constant_pool_count - 1u;

	/* Join all Workers
	 **************** */

	for (i = 0; i < my_not; i++) {
		add_byte(182u); /* invokevirtual */
		add_byte(HIGB_U2(method));
		add_byte(LOWB_U2(method));
	}

	/* Goto Back
	 ********* */

	spit_goto(l->exit_ad - (new_pos + clen));

	/* The Exception Handler
	 ********************* */

	save_handler = new_pos;

	add_byte(87u); /* pop */

	spit_goto(l->exit_ad - (new_pos + clen));
}

/* Add constructor 
 *************** */

static u2_int add_constructor(u2_int voidret, u2_int codea, worker_ptr w) {

	fm_ptr method = (fm_ptr) make_mem(sizeof(struct fm_node));
	attribute_ptr a = new_attribute();
	u4_int k, i;
	u2_int now;
	u2_int ms = 2u;
	char *sig = w->constr_d, fn[15];

	if ((!w->load_type) || (!w->load_sig))
		javab_out(-1, "lost local variable information in add_constructor()");

	/* Add Fields
	 ********** */

	k = 0u;
	now = 1u; /* initial `this' argument */

	for (i = 0u; i < w->max_locals; i++) {

		u1_int tp = w->load_type[i];

		if (tp > (u1_int) TP_2WORD) {

			fm_ptr f = (fm_ptr) make_mem(sizeof(struct fm_node));

			add_cp_entry(CONSTANT_Utf8, w->load_sig[i], 0u, 0u);
			sprintf(fn, "loc_%u", i);
			add_cp_entry(CONSTANT_Utf8, fn, 0u, 0u);

			f->access_flags = ACC_PRIVATE;
			f->name_index = constant_pool_count - 1u;
			f->descr_index = constant_pool_count - 2u;
			f->attributes_count = 0u;

			add_field(f);

			add_cp_entry(CONSTANT_NameAndType, NULL, f->name_index,
					f->descr_index);
			add_cp_entry(CONSTANT_Fieldref, NULL, this_class,
					constant_pool_count - 1u);

			w->load_ind[i] = constant_pool_count - 1u;

			/* Count Number of Fields
			 ********************** */

			k++;

			if ((tp == TP_LONG) || (tp == TP_DOUBLE)) {
				ms = 3u;
				now += 2u;
			} else
				now++;
		}
	}

	if (now > U1MAX)
		javab_out(-1, "cannot pass %u words as argument (should not occur)",
				now);

	/* Construct Method Information Node
	 ********************************* */

	add_cp_entry(CONSTANT_Utf8, "<init>", 0u, 0u);
	add_cp_entry(CONSTANT_Utf8, sig, 0u, 0u);

	method->access_flags = ACC_PUBLIC;
	method->name_index = constant_pool_count - 2u;
	method->descr_index = constant_pool_count - 1u;
	method->attributes_count = 1u;

	/* Construct Code Attribute
	 ************************ */

	a->attribute_name_index = codea;

	add_byte(0u);
	add_byte(ms); /* max_stack */

	add_byte(HIGB_U2(now));
	add_byte(LOWB_U2(now)); /* max_locals */

	{
		u4_int cl = 9u + k * 6u;

		add_byte(HIGB_U4(cl));
		add_byte(LOWB_U4(cl));
		add_byte(HIGB_U2(cl));
		add_byte(LOWB_U2(cl)); /* code_length */
	}

	add_byte(42u); /* aload_0 */

	add_byte(183u); /* invokespecial */

	add_cp_entry(CONSTANT_NameAndType, NULL, method->name_index, voidret);

	add_cp_entry(CONSTANT_Methodref, NULL, super_class,
			constant_pool_count - 1u);

	add_byte(HIGB_U2(constant_pool_count - 1u));
	add_byte(LOWB_U2(constant_pool_count - 1u));

	/* BYTECODE INSERTION
	 ****************** */

	now = 1u; /* first argument is `this' */

	for (i = 0u; i < w->max_locals; i++) {
		if (w->load_sig[i]) {

			u1_int tp = w->load_type[i];

			add_byte(42u); /* aload_0 */

			if (now > U1MAX)
				javab_out(-1,
						"index too large in add_constructor() (should not occur)");

			/* NEXT spit_ls() ALWAYS TAKES 2 BYTES! */

			spit_ls(tp, now, 0u); /* load */

			add_byte(181u); /* putfield */
			add_byte(HIGB_U2(w->load_ind[i]));
			add_byte(LOWB_U2(w->load_ind[i]));

			if ((tp == TP_LONG) || (tp == TP_DOUBLE))
				now += 2;
			else
				now++;
		}
	}

	add_cp_entry(CONSTANT_Utf8, "start", 0u, 0u);
	add_cp_entry(CONSTANT_NameAndType, NULL, constant_pool_count - 1u, voidret);
	add_cp_entry(CONSTANT_Methodref, NULL, super_class,
			constant_pool_count - 1u);

	add_byte(42u); /* aload_0 */

	add_byte(182u); /* invokevirtual */
	add_byte(HIGB_U2(constant_pool_count - 1u));
	add_byte(LOWB_U2(constant_pool_count - 1u));

	add_byte(177u); /* return */

	/* ****************** */

	add_byte(0u); /* exception_table_length */
	add_byte(0u);

	add_byte(0u); /* attributes_count */
	add_byte(0u);

	/* Hook-up Information
	 ******************* */

	a->attribute_length = new_pos;
	a->info = new_bytes;
	new_bytes = NULL;

	if (a->attribute_length != (21u + k * 6u))
		javab_out(-1, "incorrect length");

	method->attributes = (attribute_ptr *) make_mem(1 * sizeof(attribute_ptr));
	method->attributes[0] = a;

	add_method(method);

	free(sig);
	free_new_bytes();

	return ms - 1u;
}

/* Add run()-method
 **************** */

static void add_run(u2_int voidret, u2_int codea, worker_ptr w, u2_int minms) {
	fm_ptr method = (fm_ptr) make_mem(sizeof(struct fm_node));
	attribute_ptr a = new_attribute();

	u2_int ml = MAX(1u, w->max_locals);
	u2_int ms = MAX(minms, w->max_stack);
	u4_int i, ii;

	/* Construct Method Information Node
	 ********************************* */

	add_cp_entry(CONSTANT_Utf8, "run", 0u, 0u);

	method->access_flags = ACC_PUBLIC;
	method->name_index = constant_pool_count - 1u;
	method->descr_index = voidret;
	method->attributes_count = 1u;

	/* Construct Code Attribute
	 ************************ */

	a->attribute_name_index = codea;

	add_byte(HIGB_U2(ms));
	add_byte(LOWB_U2(ms)); /* max_stack */

	add_byte(HIGB_U2(ml));
	add_byte(LOWB_U2(ml)); /* max_locals */

	add_byte(0u);
	add_byte(0u);
	add_byte(0u);
	add_byte(0u); /* code_length (backpatch later) */

	/* BYTECODE INSERTION
	 ****************** */

	/* Get Values for Locals from Fields
	 (reverse order -> destroys local 0 last!) */

	{
		u2_int mlx = w->max_locals;

		for (ii = 1u; ii <= mlx; ii++) {

			i = mlx - ii;

			if (w->load_sig[i]) {
				u1_int tp = w->load_type[i];

				add_byte(42u); /* aload_0 */

				add_byte(180u); /* getfield */
				add_byte(HIGB_U2(w->load_ind[i]));
				add_byte(LOWB_U2(w->load_ind[i]));

				spit_ls(tp, i, 1u); /* store */
			}
		}
	}

	{
		s4_int off = (w->entry_off + 3u);
		s2_int off2 = (s2_int) off;

		if (off == (s4_int) off2) /* fits in 2 bytes */
			spit_goto(w->entry_off + 3u);
		else
			spit_goto(w->entry_off + 5u);
	}

	/* Copy Loop-Body
	 ************** */

	for (i = 0u; i < w->l_len; i++)
		add_byte(w->loop_code[i]);

	/* Change Stride
	 ************* */

	{
		u4_int tb = (new_pos - w->l_len) + w->ind_off;
		u2_int stride = w->ind_step * my_not;

		if (w->ind_is_w) {
			new_bytes[tb + 3u] = HIGB_U2(stride);
			new_bytes[tb + 4u] = LOWB_U2(stride);
		} else {
			if (stride > U1MAX)
				javab_out(-1, "stride too wide (should not occur)");
			new_bytes[tb + 2u] = (u1_int) stride;
		}
	}

	/* FALL-THROUGHT-EXIT: uses 'intermediate' return or return at end
	 OTHER-EXIT        : set branch to final return
	 *************************************************************** */

	if (w->exit_off) {

		/* OTHER-EXIT */

		u4_int tb = (new_pos - w->l_len) + w->exit_off;
		s4_int o = new_pos - tb;

		if (o > U2MAX)
			javab_out(0, "loop-exiting offset too large");

		new_bytes[tb + 1u] = HIGB_U2(o);
		new_bytes[tb + 2u] = LOWB_U2(o);
	}

	add_byte(177u); /* return at end */

	/* Backpatching
	 ************ */

	new_bytes[4u] = HIGB_U4(new_pos - 8u);
	new_bytes[5u] = LOWB_U4(new_pos - 8u);
	new_bytes[6u] = HIGB_U2(new_pos - 8u);
	new_bytes[7u] = LOWB_U2(new_pos - 8u); /* code_length */

	/* Handler and Attributes
	 ********************** */

	add_byte(0u); /* exception_table_length */
	add_byte(0u);

	add_byte(0u); /* attributes_count */
	add_byte(0u);

	/* Hook-up Information
	 ******************* */

	a->attribute_length = new_pos;
	a->info = new_bytes;

	method->attributes = (attribute_ptr *) make_mem(1 * sizeof(attribute_ptr));
	method->attributes[0] = a;

	add_method(method);

	new_bytes = NULL;
	free_new_bytes();
}

/* Class File Generation for one Loop Worker
 ***************************************** */

static void worker_dump(FILE *file, worker_ptr w) {

	u2_int voidret, codea;

	dump_shadow_cp(); /* Set Required Part of Constant Pool
	 ********************************** */

	/* Set Class File Items
	 ******************** */

	access_flags = (ACC_PUBLIC | ACC_FINAL | ACC_SUPER);

	add_cp_entry(CONSTANT_Utf8, w->qualified_name, 0u, 0u);
	add_cp_entry(CONSTANT_Class, "", constant_pool_count - 1u, 0u);

	this_class = constant_pool_count - 1u;

	add_cp_entry(CONSTANT_Utf8, "java/lang/Thread", 0u, 0u);
	add_cp_entry(CONSTANT_Class, "", constant_pool_count - 1u, 0u);

	super_class = constant_pool_count - 1u;

	/* Code Attribute Indicator
	 ************************ */

	add_cp_entry(CONSTANT_Utf8, "Code", 0u, 0u);

	codea = constant_pool_count - 1u;

	/* Make Method Descriptor "()V"
	 **************************** */

	add_cp_entry(CONSTANT_Utf8, "()V", 0u, 0u);

	voidret = constant_pool_count - 1u;

	/* Add Methods
	 *********** */

	{
		u2_int ms = add_constructor(voidret, codea, w);
		add_run(voidret, codea, w, ms);
	}

	/* Check Integrity of Generated Class File,
	 Output Class File, and Release Memory
	 **************************************** */

	process_classfile(NULL, 1u);
	dump_classfile(file);
	process_classfile(NULL, 3u);
}

/* *********************************************
 *** Determine PATH prefix and SIMPLE name ***
 ********************************************* */

static char *cut_pos(char *s) { /* returns FRESH copy */
	int len = (s) ? strlen(s) : 0;
	while ((len > 0) && (s[len - 1] != '/'))
		len--;
	if (len > 0) {
		char *new = strdup(s);
		new[len] = '\0';
		return new;
	}
	return NULL;
}

static char *cut_pre(char *s) { /* uses old storage */
	int len = (s) ? strlen(s) : 0;
	while ((len > 0) && (s[len - 1] != '/'))
		len--;
	return (len > 0) ? s + len : s;
}

/* ********************************************************
 *** PUBLIC FUNCTIONS                                 ***
 ******************************************************** */

/* Output of all Loop Workers for *one* Class
 ****************************************** */

void output_workers(char *filename) {

	worker_ptr scan = all_workers;
	char *path = cut_pos(filename);
	u1_int pl = (PATH) ? strlen(PATH) : 0u;
	worker_flag = 1;

	javab_out(2, " ++ workers placed in %s*.class", (pl) ? path : "");

	for (; scan; scan = scan->next) {

		char *simple = cut_pre(scan->qualified_name);
		u2_int len = 8u + pl + strlen(simple);
		char *filen = (char *) make_mem(len*sizeof(char));
		FILE *newfile = NULL;

		if (PATH)
			sprintf(filen, "%s/%s.class", PATH, simple);
		else
			sprintf(filen, "%s.class", simple);

		javab_out(2, "  + worker `%s' (simple `%s')", scan->qualified_name,
				simple);

		newfile = fopen(filen, "w");
		if (newfile) {
			worker_dump(newfile, scan);
			fclose(newfile);
		} else
			javab_out(0, "cannot open %s", filen);

		if (filen)
			free(filen);
	}

	if (path)
		free(path);

	del_workers();
	worker_flag=0;
}

/* Actual Loop Parallelization
 *************************** */

void parallelize_loop(attribute_ptr a, loop_ptr l, u1_int *method) {

	u1_int *bytes = a->info, *new;
	u2_int max_stack = B2U2(bytes[0], bytes[1]);
	u2_int max_locals = B2U2(bytes[2], bytes[3]);
	u4_int code_length = B2U4(bytes[4], bytes[5], bytes[6], bytes[7]);
	u4_int exc_base = code_length + 8u, i, j;
	u2_int exc_table_l = B2U2(bytes[exc_base], bytes[exc_base + 1u]);

	javab_out(2, "    - SUCCESS: parallel loop %u-%u", l->min_ad, l->max_ad);

	if (a->code_length != code_length)
		javab_out(2, "    - Subsequent Transformation");

	/* additional stack size: threads/parameters */

	stack_inc = 1u + (u2_int) my_not;

	/* Construct New Code
	 ****************** */

	new_worker(a, l, max_stack, max_locals, (char *) method);
	loop_replacement(l, code_length);

	/* Change Max Stack
	 **************** */

	if (stack_inc > max_stack) {
		max_stack = stack_inc;
		bytes[0] = HIGB_U2(max_stack);
		bytes[1] = LOWB_U2(max_stack);
	}

	/* Insert goto new code (cannot use spit_goto())
	 ********************************************* */

	{
		u4_int lbase = l->d->low + 8u;
		s4_int off = code_length - l->d->low;
		s2_int off2 = (s2_int) off;

		if (off == (s4_int) off2) { /* fits in 2 bytes */
			bytes[lbase++] = 167u; /* goto */
			bytes[lbase++] = HIGB_U2(off);
			bytes[lbase++] = LOWB_U2(off);
		} else {
			bytes[lbase++] = 200u; /* goto_w */
			bytes[lbase++] = HIGB_U4(off);
			bytes[lbase++] = LOWB_U4(off);
			bytes[lbase++] = HIGB_U2(off);
			bytes[lbase++] = LOWB_U2(off);
		}
	}

	/* Change Code Length
	 ****************** */

	code_length += new_pos;

	bytes[4] = HIGB_U4(code_length);
	bytes[5] = LOWB_U4(code_length);
	bytes[6] = HIGB_U2(code_length);
	bytes[7] = LOWB_U2(code_length);

	/* Change Exception Length
	 *********************** */

	exc_table_l++;

	/* Change Attribute Length (strip nested attributes)
	 ****************************************** */

	a->attribute_length = 12u + code_length + exc_table_l * 8u;

	/* Construct new Code Attributes (strip nested attributes)
	 ******************************************************* */

	new = (u1_int *) make_mem(a->attribute_length * sizeof(u1_int));

	/* Copy Old Code
	 ************* */

	for (i = 0u; i < exc_base; i++)
		new[i] = bytes[i];

	/* Copy New Code
	 ************* */

	for (i = 0u; i < new_pos; i++)
		new[exc_base + i] = new_bytes[i];

	/* Old Exception Handler and New Exception Handler
	 *********************************************** */

	i = exc_base + new_pos; /* pointer into new table */

	new[i++] = HIGB_U2(exc_table_l);
	new[i++] = LOWB_U2(exc_table_l);

	for (j = 0u; j < (exc_table_l - 1u) * 8u; j++)
		new[i++] = bytes[exc_base + 2u + j];

	save_handler += (exc_base - 8u);

	if (save_handler > U2MAX)
		javab_out(0, "sorry, code size has become too large for handler_pc");

	/* Next addresses are computed relative to code segment */

	new[i++] = HIGB_U2(exc_base - 8u);
	new[i++] = LOWB_U2(exc_base - 8u); /* start_pc */
	new[i++] = HIGB_U2(save_handler);
	new[i++] = LOWB_U2(save_handler); /* end_pc */
	new[i++] = HIGB_U2(save_handler);
	new[i++] = LOWB_U2(save_handler); /* handler_pc */

	add_cp_entry(CONSTANT_Utf8, "java/lang/InterruptedException", 0u, 0u);
	add_cp_entry(CONSTANT_Class, "", constant_pool_count - 1u, 0u);

	new[i++] = HIGB_U2(constant_pool_count - 1u);
	new[i++] = LOWB_U2(constant_pool_count - 1u); /* catch_type */

	/* Attribute Count */

	new[i++] = 0u;
	new[i++] = 0u;

	if (i != a->attribute_length)
		javab_out(-1, "lost bytes in parallelize_loop()");

	a->info = new;

	free(bytes);
	free_new_bytes();
}
