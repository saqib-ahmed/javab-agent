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

static u1_int q_set;

static bb_ptr bb_glink = NULL, bb_entry = NULL;
static u1_int bb_busy;
static u4_int bb_name;

static loop_ptr all_loops = NULL;

static u1_int *bb_copy = NULL;

static u2_int bb_max_sta;

static u2_int bb_w_arg;

static u2_int bb_ndefs;
static char **bb_rd_sig;
static u2_int *bb_rd_loc = NULL;
static u4_int *bb_rd_add = NULL;

static u2_int bb_nuses;
static u2_int *bb_use_loc = NULL;
static u4_int *bb_use_add = NULL;
static u1_int *bb_use_typ = NULL;

static u4_int nl_cnt;

/* range variables 
 *************** */

static s4_int set_lw, set_up;
static u1_int *set_lv, *set_uv;

/* ********************************************************
 *** PRIVATE FUNCTIONS                                ***
 ******************************************************** */

/* ******************************************************** 
 ** Link `ret' basic block at the end of a subroutine ***
 ******************************************************** */

static void link_sub_back(u1_int *lb, bb_ptr sub, bb_ptr ret) {
	if ((sub) && (ret) && (!sub->visited)) {

		bbh_ptr s = sub->succ;
		sub->visited = 1u;

		if (lb[sub->low] == 169u) {

			/* basic block ending with ret */

			bb_add_succ(sub, ret, (u1_int) 0u);
			bb_add_pred(ret, sub, (u1_int) 0u);
		}

		for (; s; s = s->tail)
			link_sub_back(lb, s->head, ret);
	}
}

/* ***************************
 *** Array Manipulations ***
 *************************** */

static void del_array(array_ptr sa) {

	if (sa) {
		array_ptr n;

		while (sa) {
			n = sa->next;

			if (sa->q)
				free(sa->q);
			if (sa->c)
				free(sa->c);
			if (sa->p)
				free(sa->p);

			if (sa->dep_s)
				free(sa->dep_s);
			if (sa->dep_l)
				free(sa->dep_l);
			if (sa->dep_u)
				free(sa->dep_u);

			free(sa);
			sa = n;
		}
	}
}

static array_ptr new_array(u4_int ad, u2_int loc, u1_int d, array_ptr next) {

	u4_int i;

	array_ptr a = (array_ptr) make_mem(sizeof(struct array_node));

	u1_int *q = (u1_int *) make_mem(d * sizeof(u1_int));
	s4_int *c = (s4_int *) make_mem(d * sizeof(s4_int));
	u4_int *p = (u4_int *) make_mem(d * sizeof(u4_int));

	u1_int *ds = (u1_int *) make_mem(d * sizeof(u1_int));
	s4_int *dl = (s4_int *) make_mem(d * sizeof(s4_int));
	s4_int *du = (s4_int *) make_mem(d * sizeof(s4_int));

	for (i = 0u; i < d; i++) {
		q[i] = 0u;
		c[i] = 0;
		p[i] = U4MAX;

		ds[i] = 0u;
		dl[i] = 0;
		ds[i] = 0;
	}

	a->q = q;
	a->c = c;
	a->p = p;

	a->dep_s = ds;
	a->dep_l = dl;
	a->dep_u = du;

	a->dim_ad = ad;
	a->dim_loc = loc;
	a->dim = d;
	a->lhs = 0u;

	a->next = next;

	return a;
}

/* **************************
 *** Loop Manipulations ***
 ************************** */

/* Delete All Loop Information Nodes */

static void loop_del(void) {
	loop_ptr next = NULL;

	for (; all_loops; all_loops = next) {

		next = all_loops->next;

		/* Free Fields */

		if (all_loops->nl)
			free(all_loops->nl);

		if (all_loops->load_type)
			free(all_loops->load_type);

		if (all_loops->load_sig) {
			u4_int i;
			char **s = all_loops->load_sig;
			for (i = 0u; i < all_loops->load_locs; i++)
				if (s[i])
					free(s[i]);
			free(all_loops->load_sig);
		}

		/* DELETE Array Information Nodes
		 ****************************** */

		del_array(all_loops->array);

		free(all_loops);
	}
}

static loop_ptr make_loop(bb_ptr *nl, bb_ptr b, bb_ptr d) {
	u4_int cnt = 0u, i;
	u1_int has_exit = 0u;

	loop_ptr l = (loop_ptr) make_mem(sizeof(struct loop_node));

	l->b = b;
	l->d = d;
	l->nl = nl;

	l->compare = NULL;
	l->up_bnd = NULL;
	l->lw_bnd = NULL;
	l->strict = 0u;

	l->refs = NULL;
	l->array = NULL;

	l->load_type = NULL;
	l->load_sig = NULL;
	l->load_locs = 0u;

	l->min_ad = 0u;
	l->max_ad = 0u;
	l->exit_ad = 0u;
	l->cmp_ad = 0u;

	l->ind_is_w = 0u;
	l->ind_loc = 0u;
	l->ind_add = 0u; /* reset index info */

	/* Check for *NORMAL* Loop-Exits
	 ***************************** */

	for (i = 0u; i < bb_name; i++) {

		bb_ptr n = nl[i];

		if (n) {

			if (n->name != i)
				javab_out(-1, "something wrong with natural loop");

			cnt++;

			if (n != d) {

				/* Check Successors */

				bbh_ptr s;

				for (s = n->succ; s; s = s->tail)
					if ((s->head) && (!s->exc) && (!nl[s->head->name])) {
						has_exit = 1u;
#ifdef DEBUG_LOOP
						javab_out(2, "  + loop v%u->v%u has exit to %u",
								l->b->name, l->d->name, s->head->low);
#endif
						break;
					}

				/* Check Predecessors (just a verification) */

				for (s = n->pred; s; s = s->tail)
					if ((s->head) && (!nl[s->head->name]))
						javab_out(0, "invalid loop entry in natural loop");

			}
		}
	}

	l->cnt = cnt;
	l->triv = (!has_exit);
	l->par = l->triv;

	/* Insert in order in Loop List
	 **************************** */

	{
		loop_ptr scan = all_loops, prev = NULL;

		while ((scan) && (scan->cnt > cnt)) {
			prev = scan;
			scan = scan->next;
		}

		if (prev) {
			l->next = prev->next;
			prev->next = l;
		} else {
			l->next = all_loops;
			all_loops = l;
		}
	}

	return l;
}

/* *******************************
 *** Delete all Basic Blocks ***
 ******************************* */

static void bbh_del(bbh_ptr s) {
	bbh_ptr n = NULL;
	for (; s; s = n) {
		n = s->tail;
		free(s);
	}
}

static void bb_del(void) {
	bb_ptr next = NULL;

	for (; bb_glink; bb_glink = next) {
		next = bb_glink->gnext;

		/* Free Fields */

		if (bb_glink->dominators)
			free(bb_glink->dominators);

		if (bb_glink->rd_gen)
			free(bb_glink->rd_gen);
		if (bb_glink->rd_kill)
			free(bb_glink->rd_kill);
		if (bb_glink->rd_in)
			free(bb_glink->rd_in);
		if (bb_glink->rd_out)
			free(bb_glink->rd_out);

		if (bb_glink->upw_use)
			free(bb_glink->upw_use);

		if (bb_glink->s_gen)
			free(bb_glink->s_gen);
		if (bb_glink->s_in)
			free(bb_glink->s_in);
		if (bb_glink->s_out)
			free(bb_glink->s_out);

		bbh_del(bb_glink->pred);
		bbh_del(bb_glink->succ);

		free(bb_glink);
	}

	if (bb_copy) {
		free(bb_copy);
		bb_copy = NULL;
	}
	bb_entry = NULL;
}

/* ***************************************
 *** Logical AND/OR of 'bit'-vectors ***
 *************************************************************
 *** in a future implementation, these operations could be ***
 *** made more efficient by using true bit-vectors and     ***
 *** bit-operations (cf. [Aho, Ch.10.5])                   ***
 ************************************************************* */

static void bb_and(u1_int *b1, u1_int *b2, u4_int len) {
	if ((b1) && (b2)) {
		u4_int i;
		for (i = 0u; i < len; i++)
			b1[i] = (b1[i] && b2[i]);
	} else
		javab_out(-1, "incorrect invocation of bb_and()");
}

static void bb_or(u1_int *b1, u1_int *b2, u4_int len) {
	if ((b1) && (b2)) {
		u4_int i;
		for (i = 0u; i < len; i++)
			b1[i] = (b1[i] || b2[i]);
	} else
		javab_out(-1, "incorrect invocation of bb_or()");
}

static void bb_min(u1_int *b1, u1_int *b2, u4_int len) {
	if ((b1) && (b2)) {
		u4_int i;
		for (i = 0u; i < len; i++)
			if (b2[i])
				b1[i] = 0u;
	} else
		javab_out(-1, "incorrect invocation of bb_min()");
}

/* ******************************
 *** Construct Natural Loop ***
 ****************************** */

static void bb_make_natl(bb_ptr *nl, bb_ptr b) {
	if ((b) && (!nl[b->name])) {
		bbh_ptr p;

		nl[b->name] = b;

		/* Visit Predecessors */

		for (p = b->pred; p; p = p->tail)
			bb_make_natl(nl, p->head);
	}
}

/* ************************************** 
 *** Basic Block Traversal Routines ***
 ************************************** */

/* Output
 ****** */

static void dump_la(u4_int i) {
#ifdef SHOW_ALL
	if (bb_rd_add[i] == U4MAX)
	fprintf(stderr, " %u@arg", bb_rd_loc[i]);
	else
	fprintf(stderr, " %u@%u", bb_rd_loc[i], bb_rd_add[i]);
#else
	fprintf(stderr, " d%u", i + 1);
#endif
}

static void bb_show(bb_ptr b) {
	bbh_ptr s;
	u4_int i;

	fprintf(stderr, "\n*** v%u, [%u,%u]", b->name, b->low, b->high);

	/* Control Flow Information
	 ************************ */

	fprintf(stderr, "\nsuccessors  :");

	for (s = b->succ; s; s = s->tail) {

		fprintf(stderr, " %sv%u", (s->exc) ? "*" : "", s->head->name);

		if ((s->loop) && (s->loop->nl)) {
			bb_ptr* l = s->loop->nl;

			fputc(' ', stderr);
			fputc('<', stderr);
			for (i = 0u; i < bb_name; i++)
				if (l[i])
					fprintf(stderr, " v%u", i);
			fputc(' ', stderr);
			fputc('>', stderr);
			fputc(' ', stderr);
		}
	}

	fprintf(stderr, "\npredecessors:");

	for (s = b->pred; s; s = s->tail)
		fprintf(stderr, " %sv%u", (s->exc) ? "*" : "", s->head->name);

	if (b->dominators) {
		fprintf(stderr, "\ndominators  :");
		for (i = 0u; i < bb_name; i++)
			if (b->dominators[i])
				fprintf(stderr, " v%u", i);
	}

	/* Reaching Definitions and Uses
	 ***************************** */

	if (b->rd_gen) {
		fprintf(stderr, "\nrd_gen      :");
		for (i = 0u; i < bb_ndefs; i++)
			if (b->rd_gen[i])
				dump_la(i);
	}

	if (b->rd_kill) {
		fprintf(stderr, "\nrd_kill     :");
		for (i = 0u; i < bb_ndefs; i++)
			if (b->rd_kill[i])
				dump_la(i);
	}

	if (b->rd_in) {
		fprintf(stderr, "\nrd_in       :");
		for (i = 0u; i < bb_ndefs; i++)
			if (b->rd_in[i])
				dump_la(i);
	}

	if (b->rd_out) {
		fprintf(stderr, "\nrd_out      :");
		for (i = 0u; i < bb_ndefs; i++)
			if (b->rd_out[i])
				dump_la(i);
	}

	if (b->upw_use) {
		fprintf(stderr, "\nupw_use     :");
		for (i = 0u; i < bb_nuses; i++)
			if (b->upw_use[i])
#ifdef SHOW_ALL
				fprintf(stderr, " %u@%u", bb_use_loc[i], bb_use_add[i]);
#else
				fprintf(stderr, " u%u", i + 1);
#endif
	}

	/* Stack State Information
	 *********************** */

	if (b->s_gen) {
		fprintf(stderr, "\ns_mod       : <");
		for (i = 0u; i < bb_max_sta; i++)
			dump_sta(b->s_gen[i]);
		fputc('>', stderr);
	}

	if (b->s_in) {
		fprintf(stderr, "\ns_in        : <");
		for (i = 0u; i < bb_max_sta; i++)
			dump_sta(b->s_in[i]);
		fputc('>', stderr);
	}

	if (b->s_out) {
		fprintf(stderr, "\ns_out       : <");
		for (i = 0u; i < bb_max_sta; i++)
			dump_sta(b->s_out[i]);
		fputc('>', stderr);
	}

	fputc('\n', stderr);
}

/* Names 
 ***** */

static void bb_give_name(bb_ptr b) {
	b->name = bb_name++;
}

/* Dominators
 ********** */

static void bb_dom_init(bb_ptr b) {
	u4_int i;

	b->dominators = (u1_int *) make_mem(bb_name * sizeof(u1_int));

	if (b->name == 0u) {

		/* Initial Basic Block */

		for (i = 0u; i < bb_name; i++)
			b->dominators[i] = 0u;
		b->dominators[0u] = 1u;
	} else {

		/* All other Basic Blocks */

		for (i = 0u; i < bb_name; i++)
			b->dominators[i] = 1u;
	}
}

static void bb_dom_iterate(bb_ptr b) {
	u4_int i;
	bbh_ptr p = b->pred;
	u1_int *d = b->dominators;

	for (i = 0u; i < bb_name; i++) {
		bb_copy[i] = d[i];
		d[i] = 0u;
	}

	/* Take Intersection of Dominator Sets of Predecessors
	 for each basic block that is *not* the initial block */

	if ((b->name != 0u) && (p)) {
		u1_int *p1d = p->head->dominators;

		if (!p1d)
			javab_out(-1, "lost dominator set of v%u in bb_dom_iterate()",
					b->name);

		for (i = 0u; i < bb_name; i++)
			d[i] = p1d[i];
		for (p = p->tail; p; p = p->tail)
			bb_and(d, p->head->dominators, bb_name);
	}

	d[b->name] = 1u;

	/* Detect Changes */

	for (i = 0u; i < bb_name; i++)
		if (d[i] != bb_copy[i])
			bb_busy = 1u;
}

/* Back-Edges and Natural Loops
 **************************** */

static void bb_back(bb_ptr b) {
	bbh_ptr s = b->succ;

	/* Check all succeding edges */

	for (; s; s = s->tail) {

		bb_ptr d = s->head;

		if (b->dominators[d->name]) {

			/* Construct Natural Loop for Back Edge b -> d
			 ******************************************** */

			u4_int i;
			bb_ptr *nl = (bb_ptr *) make_mem(bb_name * sizeof(bb_ptr));

			nl_cnt++;

			for (i = 0u; i < bb_name; i++)
				nl[i] = NULL;
			nl[d->name] = d;

			bb_make_natl(nl, b);

			s->loop = make_loop(nl, b, d);
		}
	}
}

/* Reaching Definitions and Uses
 ***************************** */

static void bb_rd_init(bb_ptr b) {

	u4_int i, j;

	u4_int low = b->low;
	u4_int hig = b->high;

	b->rd_in = (u1_int *) make_mem(bb_ndefs * sizeof(u1_int));
	b->rd_out = (u1_int *) make_mem(bb_ndefs * sizeof(u1_int));
	b->rd_gen = (u1_int *) make_mem(bb_ndefs * sizeof(u1_int));
	b->rd_kill = (u1_int *) make_mem(bb_ndefs * sizeof(u1_int));

	b->upw_use = (u1_int *) make_mem(bb_nuses * sizeof(u1_int));

	/* upward exposed uses */

	for (i = 0u; i < bb_nuses; i++)
		b->upw_use[i] = 0u;

	for (i = 0u; i < bb_nuses; i++) {

		u2_int add = bb_use_add[i];

		if ((low <= add) && (add <= hig))
			b->upw_use[i] = 1u;
	}

	/* init reaching definitions */

	for (i = 0u; i < bb_ndefs; i++)
		b->rd_in[i] = b->rd_gen[i] = b->rd_kill[i] = 0u;

	/* report unreachable b.b. */

	if ((!b->pred) && (b->name != 0u))
		javab_out(1, "basic block %u-%u unreachable", b->low, b->high);

	/* compute reaching definitions */

	for (i = 0u; i < bb_ndefs; i++) {

		u2_int loc = bb_rd_loc[i];
		u4_int add = bb_rd_add[i];

		if ((low <= add) && (add <= hig)) {

			/* def. in this basic block
			 (this method relies on the fact that definitions
			 in the array appear *in order* that
			 corresponds to the order in the program text)  */

			for (j = 0u; j < bb_ndefs; j++)
				if (loc == bb_rd_loc[j]) {
					b->rd_kill[j] = 1u;
					b->rd_gen[j] = 0u; /* discard previous gen. in same block */
				}
			b->rd_kill[i] = 0u;
			b->rd_gen[i] = 1u;

			for (j = 0u; j < bb_nuses; j++)
				if ((loc == bb_use_loc[j]) && (add < bb_use_add[j]))
					b->upw_use[j] = 0u; /* def kills upw. exp. use */
		}
	}

	for (i = 0u; i < bb_ndefs; i++)
		b->rd_out[i] = b->rd_gen[i];
}

static void bb_rd_iterate(bb_ptr b) {
	u4_int i;
	bbh_ptr p = b->pred;

	u1_int *in = b->rd_in;
	u1_int *out = b->rd_out;

	if ((!in) || (!out))
		javab_out(-1, "lost in/out sets of v%u in bb_rd_iterate()", b->name);

	for (i = 0u; i < bb_ndefs; i++) {
		bb_copy[i] = out[i];
		out[i] = in[i] = 0u;
	}

	/* For initial     bb: add argument definitions
	 For unreachable bb: add *all* definitions to avoid
	 `annoying' used-before-defined errors
	 ********************************************************* */

	if (b->name == 0u) {
		for (i = 0u; i < bb_w_arg; i++)
			in[i] = 1u;
	} else if (!b->pred) {
		for (i = 0u; i < bb_ndefs; i++)
			in[i] = 1u;
	}

	/* Take Union of Out Sets of Predecessors */

	for (; p; p = p->tail)
		bb_or(in, p->head->rd_out, bb_ndefs);

	/* Evaluate Data Flow Equations */

	bb_or(out, in, bb_ndefs);
	bb_min(out, b->rd_kill, bb_ndefs);
	bb_or(out, b->rd_gen, bb_ndefs);

	/* Detect Changes */

	for (i = 0u; i < bb_ndefs; i++)
		if (out[i] != bb_copy[i])
			bb_busy = 1u;
}

/* Stack State Computation
 *********************** */

static state_ptr bb_state_meet(state_ptr c1, state_ptr c2) {
	state_ptr res = NULL;
	u4_int i;

	if (c1) {

		switch (c1->kind) {

		case S_BOT:
			res = c1;
			break;

		case S_EXP:

			if (c2) {

				if ((c2->kind == S_EXP) && (c1->u.exp.con == c2->u.exp.con)) {

					/* exp(c,D) MEET exp(c',D') */

					if ((c1->u.exp.rd) && (c2->u.exp.rd)) {

						/* Compute Union in FRESH Stack State
						 for c == c'and D != 0 en D' != 0
						 ********************************** */

						res = new_stack_state(S_EXP, 0u, 0);

						for (i = 0u; i < bb_ndefs; i++)
							res->u.exp.rd[i] = c1->u.exp.rd[i];
						bb_or(res->u.exp.rd, c2->u.exp.rd, bb_ndefs);

					} else if ((!c1->u.exp.rd) && (!c2->u.exp.rd)) {

						/* Use old Stack States in case
						 c == c' and D == 0 and D' == 0
						 (is con(c), con(c') meet)
						 ****************************** */

						res = c1;
					} else
						/* con(c) <-> exp(c',D') or exp(c,D) <-> con(c')
						 ********************************************* */
						res = new_stack_state(S_BOT, 0u, 0);
				} else
					res = new_stack_state(S_BOT, 0u, 0);

			} else
				res = c1; /* c2 == top, c1 = exp(c,D)
				 ************************ */
			break;

		case S_REF:

			res =
					((c2)
							&& ((c2->kind != S_REF)
									|| (c2->u.ref.ad != c1->u.ref.ad))) ?
							new_stack_state(S_BOT, 0u, 0) : c1;
			break;

		default:
			javab_out(-1, "invalid kind %u in bb_state_meet()", c1->kind);
		}
	} else
		res = c2;

	return res;
}

static void bb_state_iterate(bb_ptr b) {
	u4_int i;
	bbh_ptr p = b->pred;

	state_ptr *s_out = b->s_out;
	state_ptr *s_in = b->s_in;
	state_ptr *s_gen = b->s_gen;

	if ((!s_out) || (!s_in) || (!s_gen))
		javab_out(-1, "lost information in bb_state_iterate() for %u, %u-%u",
				b->name, b->low, b->high);

	for (i = 0u; i < bb_max_sta; i++)
		s_in[i] = NULL;

	/* Take MEET of Out Sets of Predecessors [BikGannon]
	 ************************************************* */

	for (; p; p = p->tail) {
		if (p->head->s_out)
			for (i = 0; i < bb_max_sta; i++)
				s_in[i] = bb_state_meet(s_in[i], p->head->s_out[i]);
	}

	/* O := PSI(s_gen, s_in); [BikGannon]
	 ********************************** */

	for (i = 0; i < bb_max_sta; i++)
		if (!s_gen[i]) {

			if (((!s_out[i]) && (s_in[i])) || ((s_out[i]) && (!s_in[i]))
					|| ((s_out[i]) && (s_in[i])
							&& (s_out[i]->kind != s_in[i]->kind)))
				bb_busy = 1u;
			s_out[i] = s_in[i];
		}
}

/* *****************************
 *** Basic Block Traversal ***
 **********************************************************************
 *** bb_traverse(): apply bb_action() to every bb in the flow graph ***
 ********************************************************************** */

static void bb_trav_aux(bb_ptr b, void (*bb_action)(bb_ptr)) {

	if ((b) && (!b->visited)) {

		bbh_ptr s;

		b->visited = 1u;

		if (bb_action)
			(*bb_action)(b);

		/* Visit Successors */

		for (s = b->succ; s; s = s->tail)
			bb_trav_aux(s->head, bb_action);
	}
}

static void bb_traverse(void (*bb_action)(bb_ptr)) {

	bb_ptr b;

	/* Reset visited-bits */

	for (b = bb_glink; b; b = b->gnext)
		b->visited = 0u;

	/* Perform a DFS of the FOREST (entry first) */

	bb_trav_aux(bb_entry, bb_action);

	for (b = bb_glink; b; b = b->gnext)
		if (!b->visited)
			bb_trav_aux(b, bb_action);
}

/* *********************************************
 *** Compute Dominators [Aho, algo. 10.16] ***
 ********************************************* */

static void bb_dominators(void) {

	u2_int iter = 0u;

	bb_traverse(bb_dom_init);

	bb_busy = 1u;
	while (bb_busy) {

		iter++;

		if (iter > 300u) {
			javab_out(0, "too many iterations required to find dominators");
			return;
		}

		bb_busy = 0u;
		bb_traverse(bb_dom_iterate);
	}

	javab_out(2, "  + dominators found in %u iterations", iter);
}

/* ******************************************************
 *** Compute Reaching Definitions [Aho, algo. 10.2] ***
 ****************************************************** */

static void bb_reaching_defs(void) {

	u2_int iter = 0u;

	bb_traverse(bb_rd_init);

	bb_busy = 1u;
	while (bb_busy) {

		iter++;

		if (iter > 300u) {
			javab_out(0,
					"too many iterations required to find reaching definitions");
			return;
		}

		bb_busy = 0u;
		bb_traverse(bb_rd_iterate);
	}

	javab_out(2, "  + reaching definitions found in %u iterations", iter);
}

/* *******************************
 *** Stack State Computation ***
 ******************************* */

static void bb_stack_state(void) {
	u2_int iter = 0u;

	bb_busy = 1u;
	while (bb_busy) {

		iter++;

		if (iter > 300u) {
			javab_out(0, "too many iterations required to find stack states");
			return;
		}

		bb_busy = 0u;
		bb_traverse(bb_state_iterate);
	}

	javab_out(2, "  + stack state found in %u iterations", iter);
}

/* ****************************************
 *** Find Trivial Loop of a Subscript ***
 **************************************** */

static loop_ptr get_loop(bb_ptr b, state_ptr ind) {

	if ((b) && (ind) && (ind->kind == S_EXP) && (ind->u.exp.rd)) { /* exp(c,D), D<>0
	 ************** */
		u4_int i;
		u1_int *rd1 = ind->u.exp.rd;
		loop_ptr scan = all_loops;

		for (; scan; scan = scan->next)
			if ((scan->triv) && (scan->nl[b->name])) {

				state_ptr co = scan->compare;
				u1_int eq = 1u, *rd2;

				if ((!co) || (co->kind != S_EXP) || (!co->u.exp.rd))
					javab_out(-1, "lost index of trivial loop in get_loop()");

				rd2 = co->u.exp.rd;

				for (i = 0u; i < bb_ndefs; i++)
					if (rd1[i] != rd2[i]) {
						eq = 0u;
						break;
					}

				if (eq)
					return scan;
			}
	}
	return NULL;
}

/* ****************************************
 *** Construct RANGE of Subscript exp ***
 **************************************** */

static u1_int set_urange(bb_ptr b, state_ptr exp) {

	u1_int suc = 0u;

	if ((exp) && (exp->kind == S_EXP)) {

		loop_ptr lp = get_loop(b, exp);

		if (!lp) { /* exp(c,D), D does not belong to loop index */

			suc = 1u;
			set_up = exp->u.exp.con + 1;
			set_uv = exp->u.exp.rd; /* RANGE [*,c+1)
			 ************** */
		} else { /* exp(c,D), D belongs to loop */

			if (set_urange(b, lp->up_bnd)) {
				suc = 1u;
				set_up += exp->u.exp.con;
//	 set_uv  = set_uv;

				if (lp->strict)
					set_up -= 1; /* RANGE [*,(c+u)  +v)
					 or [*,(c+u-1)+v)
					 ******************* */
			}
		}
	}

#ifdef DEBUG_LOOP
	dump_sta(exp);
	if (!suc)
		fprintf(stderr, "> range not found\n");
	else
		fprintf(stderr, "> range found [*,%d%s)\n", set_up,
				(set_uv) ? "+v" : "");
#endif

	return suc;
}

static u1_int set_lrange(bb_ptr b, state_ptr exp) {

	u1_int suc = 0u;

	if ((exp) && (exp->kind == S_EXP)) {

		loop_ptr lp = get_loop(b, exp);

		if (!lp) { /* exp(c,D), D does not belong to loop index */

			suc = 1u;
			set_lw = exp->u.exp.con;
			set_lv = exp->u.exp.rd; /* RANGE [c+v,*)
			 ************** */
		} else { /* exp(c,D), D belongs to loop */

			if (set_lrange(b, lp->lw_bnd)) {
				suc = 1u;
				set_lw += exp->u.exp.con;
//	set_lv  = set_lv;
				/* RANGE [(c+l)+v,*)
				 ***************** */
			}
		}
	}

#ifdef DEBUG_LOOP
	dump_sta(exp);
	if (!suc)
		fprintf(stderr, "> range not found\n");
	else if (set_lv)
		fprintf(stderr, "> range found [%d+v,*)\n", set_lw);
	else
		fprintf(stderr, "> range found [%d,*)\n", set_lw);
#endif

	return suc;
}
/* ****************************************
 *** Simple Array Dependence Analysis ***
 **************************************** */

static array_ptr record_ref(loop_ptr lp, u4_int ad, u2_int loc, u1_int d,
		u2_int d2, u1_int is_ind, s4_int off, u1_int lhs) {

	array_ptr scan = lp->array;

	for (; scan; scan = scan->next)
		if ((scan->dim_ad == ad) && (scan->dim_loc == loc))
			break;

	/* Make New Array Node
	 ******************* */

	if (!scan) {
		scan = new_array(ad, loc, d, lp->array);
		lp->array = scan;
	}

	/* Process Access Information */

	if ((scan->dim == d) && (scan->dim_ad == ad) && (scan->dim_loc == loc)) {

		if (is_ind) { /* dimension d2 used as `i + off' */

			if (scan->dep_s[d2] == 0u) { /* First Set */
				scan->dep_s[d2] = 1u;
				scan->dep_l[d2] = off;
				scan->dep_u[d2] = off;
			} else {
				scan->dep_l[d2] = MIN(scan->dep_l[d2], off);
				scan->dep_u[d2] = MAX(scan->dep_u[d2], off);
			}

		} else
			scan->dep_s[d2] = 2u;

		/* Used at LHS?
		 ************ */

		if (lhs)
			scan->lhs = lhs;
	} else
		javab_out(-1, "lost array in record_ref()");

	return scan;
}

/* ********************************
 *** Perform Recorded Queries ***
 ******************************** */

static u1_int query_array(array_ptr a) {
	u4_int i;
	u4_int ad = a->dim_ad;
	u2_int loc = a->dim_loc;
	u1_int dim = a->dim, set = 0u;

	u1_int *quer = a->q;
	s4_int *cons = a->c;
	u4_int *pars = a->p;

	/* Determine if a Query is Required */

	for (i = 0u; i < dim; i++)
		if (quer[i]) {
			set = 1u;
			break;
		}

	/* Perform Query */

	if (set) {

		if (ad != U4MAX)
			javab_out(-1, "should not query on array declarations");
		else if (!s_query) {
			javab_out(2, "    - PAR-FAIL: cannot query for array parameter %u",
					loc);
			return 0u;
		}
#if DEBUG
		fprintf(stderr,
				"\n  > Queries for the %u-dimensional array parameter `%s'"
						" in local %u\n", dim, bb_rd_sig[loc], loc);
#endif
		for (i = 0u; i < dim; i++) {

			/* Recorded Query */

			if (quer[i]) {
#if DEBUG
				if (pars[i] == U4MAX)
					fprintf(stderr, "    * dim-%u: is range [0,%3i) valid?\n",
							i + 1u, cons[i]);
				else if (cons[i])
					fprintf(stderr, "    * dim-%u: is range [0,%3i+n) valid"
							" where n = parameter in local %u?\n", i + 1u,
							cons[i], pars[i]);
				else
					fprintf(stderr, "    * dim-%u: is range [0,n) valid"
							" where n = parameter in local %u?\n", i + 1u,
							pars[i]);
#endif
				if (!query())
					return 0u;
			}
		}
	}
	return 1u;
}

/* ************************************
 *** Array Subscript Verification ***
 ************************************ */

static u1_int verify_sub(attribute_ptr a, ref_ptr r, loop_ptr loop) {

	u4_int ls_ad = r->ad;
	state_ptr ref = r->rf;
	state_ptr ind = r->in;
	u1_int lhs = r->lhs;

#ifdef DEBUG_LOOP
	fprintf(stderr, ">>> <t>a%s at %u\n", (lhs) ? "store" : "load", ls_ad);
#endif

	if ((ref) && (ref->kind == S_REF) && (ref->u.ref.d != 0u) && (ind)
			&& (ind->kind == S_EXP)) {

		u4_int ad = ref->u.ref.ad;
		u1_int d = ref->u.ref.d;
		u1_int d2 = ref->u.ref.d2;
		u1_int rem = d - d2;
		u2_int loc = ref->u.ref.loc;

		loop_ptr my_l = get_loop(a->my_bb[ls_ad], ind);
		array_ptr my_a = NULL;

		if (d2 >= d)
			javab_out(-1, "completely dereferenced array (should not occur)");

		if ((!set_urange(a->my_bb[ls_ad], ind))
				|| (!set_lrange(a->my_bb[ls_ad], ind))) {
			javab_out(2, "    - PAR-FAIL: bounds at %u cannot be verified",
					ls_ad);
		} else if ((set_lw < 0) || (set_lv)) {
			javab_out(2, "    - PAR-FAIL: potential negative subscript at %u",
					ls_ad);
		} else { /* known RANGE [>=0, u)
		 ********************* */

#ifdef DEBUG_LOOP
			if (my_l)
				fprintf(stderr, "    subscript loop : %u-%u\n", my_l->min_ad,
						my_l->max_ad);
#endif

			/* Array Data Dependence Analysis
			 ****************************** */

			my_a = record_ref(loop, ad, loc, d, d2, (my_l == loop),
					ind->u.exp.con, lhs);

			if (ad == U4MAX) {

				/* ****************************************
				 *** Chased Back to apar(loc, d - d2) ***
				 **************************************** */

				u4_int par = U4MAX, i; /* wide counters */
				u2_int cnt = 0u;

#ifdef DEBUG_LOOP
				fprintf(stderr, "    chased back to apar(%u,%u)\n", loc, rem);
#endif

				if (set_uv)
					for (i = 0u; i < bb_ndefs; i++)
						if (set_uv[i]) {
							par = i;
							cnt++;
						}

				if ((set_uv) && ((cnt != 1u) || (bb_rd_add[par] != U4MAX))) {
					javab_out(2, "    - PAR-FAIL: complex upperbound for"
							" array parameter %u at %u", loc, ls_ad);
				} else if ((!set_uv) && (set_up <= 0)) {
					javab_out(2, "    - PAR-FAIL: potential negative subscript"
							" for array parameter %u at %u", loc, ls_ad);
				} else if (!my_a->q[d2]) { /* First set */

					q_set = 1u;

					my_a->q[d2] = 1u;
					my_a->c[d2] = set_up;
					my_a->p[d2] = par;

					return 1u;
				} else { /* Second Set */

					q_set = 1u;

					if (my_a->p[d2] == par) {
						my_a->c[d2] = MAX(my_a->c[d2], set_up);
						return 1u;
					} else
						javab_out(2, "    - PAR-FAIL: differing upperbound for"
								" array parameter %u at %u", loc, ls_ad);
				}
			} else {

				/* ***************************************
				 *** Chased Back to decl(ad, d - d2) ***
				 *************************************** */

				u1_int dim = 0u;
				u2_int sp = a->sp_before[ad];
				u1_int *byt = &(a->info[8u]);
				state_ptr *stack = a->st_state[ad];
				state_ptr def = (sp >= rem) ? stack[sp - rem] : NULL;

#ifdef DEBUG_LOOP
				fprintf(stderr, "    chased back to adecl(%u,%u)\n    ", ad,
						rem);
				dump_sta(def);
				fprintf(stderr, " < declaration %u of %u-dim array\n", d2, d);
#endif

				/* Verify Declaration (not really required)
				 **************************************** */

				switch (byt[ad]) {
				case 189u: /* anewarray      */
				case 188u: /* newarray       */
				case 197u: /* multianewarray */
					dim = (byt[ad] == 197u) ? byt[ad + 3u] : 1u;
					break;
				default:
					javab_out(-1, "lost array declaration in verify_sub()");
				}

				if (dim != d)
					javab_out(-1, "lost array dimension in verify_sub()");

				/* Verify Array Reference
				 ********************** */

				if ((def) && (def->kind == S_EXP)) {

					s4_int s = def->u.exp.con; /* dim. decl. info */
					u1_int *rd = def->u.exp.rd;

					if ((s < 0) && (!rd)) {
						javab_out(0, "negative dimension declaration of %u "
								"used at %u", ad, ls_ad);
					} else if (!rd) { /* Constant Dimension
					 ****************** */
						if ((!set_uv) && (set_up <= s))
							return 1u;
						else
							javab_out(2,
									"    - PAR-FAIL: potential constant subscript "
											"violation at %u", ls_ad);
					} else { /* Variable Dimension
					 ****************** */

						if ((!set_uv) || (set_up > s))
							javab_out(2,
									"    - PAR-FAIL: potential variable subscript "
											"violation at %u", ls_ad);
						else {
							u4_int i, last;
							u2_int cnt = 0u;

							for (i = 0u; i < bb_ndefs; i++)
								if (rd[i] != set_uv[i]) {
									cnt = 2u;
									break;
								} else if (rd[i]) {
									cnt++;
									last = i;
								}

							if ((cnt == 1u) && (bb_rd_add[last] == U4MAX))
								return 1u;
							else
								javab_out(2,
										"    - PAR-FAIL: complex subscript at %u",
										ls_ad);
						}
					}
				} else
					javab_out(2, "    - PAR-FAIL: unknown declaration "
							"of dimension %u for reference at %u", d2 + 1u,
							ls_ad);
			}
		}
	} else
		javab_out(2, "    - PAR-FAIL: reference at %u cannot be verified",
				ls_ad);

	return 0u;
}

/* ********************************************************
 *** PUBLIC FUNCTIONS                                 ***
 ******************************************************** */

/* *******************************************************
 *** Replace Old Inner-Loop Instructions with NOP    ***
 *** Replace New Outer-Loop Instructions with RETURN ***
 ******************************************************* */

void nop_loop(attribute_ptr a, loop_ptr lp, u1_int *newcode) {

	u1_int *oldcode = &(a->info[8u]);
	u4_int i; /* wide counter */
	bb_ptr b;

	for (b = bb_glink; b; b = b->gnext) {

		u4_int min = lp->min_ad;
		u4_int max = lp->max_ad;
		u4_int l = b->low;
		u4_int h = b->high;

		if (lp->nl[b->name]) {
			for (i = l; i <= h; i++)
				oldcode[i] = 0u; /* nop */
		} else if ((min <= l) && (h <= max)) {
			for (i = l; i <= h; i++)
				newcode[i - min] = 177u; /* return */
		}
	}
}

/* *******************************************************
 *** Link RET of Subroutine to Instruction after JSR ***
 ******************************************************* */

void bb_link_sub_back(u1_int *lb, bb_ptr sub, bb_ptr ret) {

	/* Reset visited-bits */

	bb_ptr b;

	for (b = bb_glink; b; b = b->gnext)
		b->visited = 0u;

	link_sub_back(lb, sub, ret);
}

/* *********************************
 *** Basic Block Manipulations ***
 ******************************************************
 *** bb_add()      : make a new basic block         ***
 *** bb_add_pred() : add predecessor to basic block ***
 *** bb_add_succ() : add successor   to basic block ***
 ****************************************************** */

bb_ptr bb_add(u2_int l, u2_int h, u1_int is_entry) {

	bb_ptr b = (bb_ptr) make_mem(sizeof(struct bb_node));

	if (is_entry)
		bb_entry = b;

	b->gnext = bb_glink;
	bb_glink = b;

	b->dominators = NULL;

	b->rd_gen = NULL;
	b->rd_kill = NULL;
	b->rd_in = NULL;
	b->rd_out = NULL;

	b->upw_use = NULL;

	b->s_gen = NULL;
	b->s_in = NULL;
	b->s_out = NULL;

	b->pred = NULL;
	b->succ = NULL;

	b->name = 0u;
	b->low = l;
	b->high = h;

	b->visited = 0u;

	return b;
}

void bb_add_pred(bb_ptr b, bb_ptr pred, u1_int exc) {
	if ((b) && (pred) && (b != pred)) {
		bbh_ptr p = (bbh_ptr) make_mem(sizeof(struct bbh_node));
		p->head = pred;
		p->tail = b->pred;
		b->pred = p;
		p->loop = NULL;
		p->exc = exc; /* exception flag */
	}
}

void bb_add_succ(bb_ptr b, bb_ptr succ, u1_int exc) {
	if ((b) && (succ) && (b != succ)) {
		bbh_ptr s = (bbh_ptr) make_mem(sizeof(struct bbh_node));
		s->head = succ;
		s->tail = b->succ;
		b->succ = s;
		s->loop = NULL;
		s->exc = exc; /* exception flag */
	}
}

/* ******************************
 *** Basic Block Processing ***
 ****************************** */

void bb_first(u2_int ndefs, u2_int *rd_loc, u4_int *rd_add, char **rd_sig,
		u2_int w_arg, u2_int nuses, u2_int *use_loc, u4_int *use_add,
		u1_int *use_typ, u2_int max_sta) {

	if (!bb_entry)
		javab_out(-1, "lost basic block entry");

	/* ******************
	 *** FIRST PASS ***
	 ****************** */

	bb_ndefs = ndefs;
	bb_rd_loc = rd_loc;
	bb_rd_add = rd_add;
	bb_rd_sig = rd_sig;
	bb_w_arg = w_arg;

	bb_nuses = nuses;
	bb_use_loc = use_loc;
	bb_use_add = use_add;
	bb_use_typ = use_typ;

	bb_max_sta = max_sta;

	/* Label Basic Blocks
	 ****************** */

	bb_name = 0u;

	bb_traverse(bb_give_name);

	bb_copy = (u1_int *) make_mem(MAX(bb_name, ndefs) * sizeof(u1_int));

	javab_out(2, "  + %u basic blocks", bb_name);

	/* Give up for many basic block
	 (memory requirements are too excessive...)
	 **************************************************** */

	if (bb_name > 6000u) {
		javab_out(0, "too many basic blocks, sorry");
		return;
	}

	/* Compute Dominators
	 ****************** */

	bb_dominators();

	/* Determine Back-Edges
	 and Natural Loops
	 ******************** */

	nl_cnt = 0u;

	bb_traverse(bb_back);

	javab_out(2, "  + %u natural loops", nl_cnt);

	/* Compute Reaching Definitions
	 **************************** */

	bb_reaching_defs();
}

void bb_second(u1_int *nm) {

	/* *******************
	 *** SECOND PASS ***
	 ******************* */

	/* Stack State
	 *********** */

	bb_stack_state();

	if (s_basic) {

		u4_int i;

		fprintf(stderr, "\n++++++++++ BASIC BLOCK INFORMATION OF %s()\n", nm);

		/* Print Uses Table
		 **************** */

		fprintf(stderr, "\nUSE|");

		for (i = 1u; i <= bb_nuses; i++)
			fprintf(stderr, "%5u|", i);

		fprintf(stderr, "\nvar|");
		for (i = 0u; i < bb_nuses; i++)
			fprintf(stderr, "%5u|", bb_use_loc[i]);

		fprintf(stderr, "\nadd|");
		for (i = 0u; i < bb_nuses; i++)
			fprintf(stderr, "%5u|", bb_use_add[i]);

		fprintf(stderr, "\ntyp|");
		for (i = 0u; i < bb_nuses; i++) {
			switch (bb_use_typ[i]) {
			case TP_INT:
				fprintf(stderr, "int  |");
				break;
			case TP_LONG:
				fprintf(stderr, "long |");
				break;
			case TP_FLOAT:
				fprintf(stderr, "float|");
				break;
			case TP_DOUBLE:
				fprintf(stderr, "doub.|");
				break;
			case TP_REF:
				fprintf(stderr, "ref  |");
				break;
			case TP_2WORD:
				fprintf(stderr, " (2) |");
				break;
			default:
				fprintf(stderr, "error|");
				break;
			}
		}

		/* Print Definitions Table
		 *********************** */

		fprintf(stderr, "\n\nDEF|");

		for (i = 1u; i <= bb_ndefs; i++)
			fprintf(stderr, "%5u|", i);

		fprintf(stderr, "\nvar|");
		for (i = 0u; i < bb_ndefs; i++)
			fprintf(stderr, "%5u|", bb_rd_loc[i]);

		fprintf(stderr, "\nadd|");
		for (i = 0u; i < bb_ndefs; i++)
			if (bb_rd_add[i] == U4MAX)
				fprintf(stderr, "  -- |");
			else
				fprintf(stderr, "%5u|", bb_rd_add[i]);

		fprintf(stderr, "\ntyp|");
		for (i = 0u; i < bb_ndefs; i++)
			if (bb_rd_add[i] == U4MAX)
				fprintf(stderr, "%5s|",
						(bb_rd_sig[i]) ? (char *) bb_rd_sig[i] : " (2) ");

		fputc('\n', stderr);
		fputc('\n', stderr);

		/* Show Flow Graph
		 *************** */

		bb_traverse(bb_show);
	}
}

/* Loop Parallelization
 ******************** */

void bb_par(attribute_ptr a, u1_int *nm, u1_int *tp) {

	loop_ptr l;

	/* Scan over Candidate Natural Loops
	 Obtain Information for Trivial Loops
	 ************************************ */

	for (l = all_loops; l; l = l->next)
		if (l->triv)
			check_triv_loop(l);

	/* Parallelize Trivial Parallel Loops
	 ********************************** */

	for (l = all_loops; l; l = l->next) {

		n_loop++;

		if (l->triv)
			n_triv++;

		if (l->array)
			javab_out(-1, "loop has already been considered");
		else if ((l->triv) && (l->par)) {

			u4_int i;
			ref_ptr scan;
			array_ptr sa, so;

			javab_out(2, "  + extended analysis in loop v%u->v%u,"
					" index %u, iinc at %u", l->b->name, l->d->name, l->ind_loc,
					l->ind_add);

			q_set = 0u;

			/* Check Local Variable Accesses
			 ***************************** */

			{
				u4_int w = 1u; /* initial word for `this' */

				for (i = 0u; i < l->load_locs; i++) {
					u1_int t = l->load_type[i];
					if (t > (u1_int) TP_2WORD) {

						if ((t == TP_LONG) || (t == TP_DOUBLE))
							w += 2;
						else
							w++;
					} else if (t == TP_ERROR) {
						l->par = 0u;
						javab_out(2, "    - PAR-FAIL: error type for local %u",
								i);
						break;
					}
				}

				if (w > U1MAX) {
					l->par = 0u;
					javab_out(2, "    - PAR-FAIL: too many parameter words");
					break;
				}
			}

			/* list of <t>astore-s and <t>aload-s
			 ********************************** */

			if (l->par)
				for (scan = l->refs; scan; scan = scan->next)
					if (!verify_sub(a, scan, l)) {
						l->par = 0u;
						break;
					}

			/* Simple Data Dependence Analysis for Arrays
			 ****************************************** */

			if (l->par) {
				for (sa = l->array; sa; sa = sa->next)
					if (sa->lhs) {

						u1_int dep = 1u;

						for (i = 0; i < sa->dim; i++) {

#ifdef DEBUG_LOOP
							if (sa->dep_s[i] == 1u)
								fprintf(stderr, "[%d,%d] ", sa->dep_l[i],
										sa->dep_u[i]);
							else
								fprintf(stderr, sa->dep_s[i] ? "* " : "- ");
#endif
							if (sa->dep_s[i] == 1u) {

								/* Check if offsets are within stride
								 ********************************** */

								s4_int r = sa->dep_u[i] - sa->dep_l[i];

								if (r < ((s4_int) l->ind_step)) {
									dep = 0u;
#ifndef DEBUG_LOOP
									break;
#endif
								}
							}
						}

#ifdef DEBUG_LOOP
						if (sa->dim_ad == U4MAX)
							fprintf(stderr,
									"<<< dep.analysis of array par. %u\n",
									sa->dim_loc);
						else
							fprintf(stderr,
									"<<< dep.analysis of array decl. at %u\n",
									sa->dim_ad);
#endif

						if (dep) {

							l->par = 0u;

							if (sa->dim_ad == U4MAX)
								javab_out(2,
										"    - PAR-FAIL: loop-carried dependences"
												" on array parameter %u",
										sa->dim_loc);
							else
								javab_out(2,
										"    - PAR-FAIL: loop-carried dependences"
												" on array declared at %u",
										sa->dim_ad);

							break;
						}
					}
			}

			/* Check if Loop is Nest in Parallel Loop
			 ************************************** */

			if ((l->par) && (!s_nest)) {
				u1_int nsi = 0u;
				loop_ptr s;

				for (s = all_loops; s; s = s->next) {
					for (i = 0u; i < bb_name; i++)
						if ((s->nl[i]) && (l->nl[i])) { /* vertex in common */
							if (s->cnt < l->cnt) { /* s is nested within l */
								nsi = 1u;
								break;
							}
						}
				}

				if (!nsi) {
					l->par = 0u;
					javab_out(2,
							"    - PAR-FAIL: loop does not contain another loop");
				}
			}

			/* Recorded Queries for Array Parameters
			 ************************************* */

			if ((l->par) && (q_set) && (s_query)) {
#if DEBUG
				fprintf(stderr, "\n*** ");
				javab_out(1, "Queries for method `%s%s'", nm, tp);
#endif
			}

			if (l->par)
				for (sa = l->array; sa && l->par; sa = sa->next) {
					if (!query_array(sa)) {
						l->par = 0u;
						break;
					}
				}

			/* Overlap Queries for Array Parameters
			 ************************************ */

			if (l->par)
				for (sa = l->array; sa && l->par; sa = sa->next) {

					if (sa->dim_ad == U4MAX) {
						for (so = sa->next; so && l->par; so = so->next) {
							if ((so->dim_ad == U4MAX)
									&& ((so->lhs) || (sa->lhs))) {

								/* Overlap Query Required */

								u2_int l1 = sa->dim_loc;
								u2_int l2 = so->dim_loc;

								if (l1 == l2)
									javab_out(-1,
											"two array nodes for same array par. %u",
											l1);

								if (s_query) {
#if DEBUG
									fprintf(stderr,
											"\nIs storage of array parameters"
													" %u `%s' and %u `%s' non-overlapping?\n",
											l1, bb_rd_sig[l1], l2,
											bb_rd_sig[l2]);
#endif
									if (!query()) {
										l->par = 0u;
										break;
									}
								} else {
									l->par = 0u;
									javab_out(2,
											"    - PAR-FAIL: potential overlap"
													" of array parameters %u%s and %u%s",
											l1, bb_rd_sig[l1], l2,
											bb_rd_sig[l2]);
									break;
								}
							}
						}
					}
				}

			if ((!error_1) && (l->par)) {

				loop_ptr s;

				/* un-parallelize all overlapping loops
				 ************************************* */

				for (s = all_loops; s; s = s->next) {
					for (i = 0u; i < bb_name; i++)
						if ((s->nl[i]) && (l->nl[i])) { /* vertex in common */
							s->par = 0u;
							if (s->cnt < l->cnt) /* s is nested within l */
								n_nest++;
							break;
						}
				}

				n_par++;

				parallelize_loop(a, l, nm);
			}
		}

	} /* all loops */
}

/* Delete Flow Graph
 ***************** */

void bb_delete(void) {
	bb_del();
	loop_del();
}

/* Output of a Stack State
 *********************** */

void dump_sta(state_ptr s) {
	u4_int i;

	if (s) {
		switch (s->kind) {

		case S_BOT:
			fprintf(stderr, "BOT ");
			break;

		case S_EXP:
			if (!s->u.exp.rd)
				fprintf(stderr, "[con %i] ", s->u.exp.con);
			else {

				if (s->u.exp.con)
					fprintf(stderr, "[exp %i ", s->u.exp.con);
				else
					fprintf(stderr, "[var ");

				for (i = 0u; i < bb_ndefs; i++)
					if (s->u.exp.rd[i])
						dump_la(i);

				fputc(']', stderr);
				fputc(' ', stderr);
			}
			break;

		case S_REF:
			if (s->u.ref.d == 0u)
				fprintf(stderr, "[ref-%u] ", s->u.ref.ad);
			else if (s->u.ref.ad == U4MAX)
				fprintf(stderr, "[apar-%u,%u d=%u] ", s->u.ref.loc,
						s->u.ref.d - s->u.ref.d2, s->u.ref.d);
			else
				fprintf(stderr, "[adecl-%u,%u d=%u] ", s->u.ref.ad,
						s->u.ref.d - s->u.ref.d2, s->u.ref.d);
			break;

		default:
			javab_out(-1, "invalid kind %u in dump_sta()", s->kind);
		}
	} else
		fprintf(stderr, "TOP ");
}
