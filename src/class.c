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

#undef DEBUG_SHADOW

/* ********************************************************
 *** EXTERNAL VARIABLES                               ***
 ******************************************************** */

/* PUBLIC  
 ****** */

u4_int magic;
u2_int minor_version, major_version;

u2_int constant_pool_count = 0u;
constant_ptr *constant_pool = NULL;

u2_int access_flags, this_class, super_class;

u2_int interfaces_count = 0u;
u2_int *interfaces = NULL;

u2_int fields_count = 0u;
fm_ptr *fields = NULL;

u2_int methods_count = 0u;
fm_ptr *methods = NULL;

u2_int attributes_count = 0u;
attribute_ptr *attributes = NULL;

/* PRIVATE
 ******* */

static unsigned char *file = NULL;

static u2_int extra_cp = 0u;
static u2_int extra_field = 0u;
static u2_int extra_method = 0u;

static u4_int SIZE = 0;
static u2_int shadow_cnt = 0u;
static constant_ptr *shadow_cp = NULL;

/* ********************************************************
 *** PRIVATE FUNCTIONS                                ***
 ******************************************************** */

/* read u1_int, u2_int, and u4_int routines
 **************************************** */

static u1_int read_u1(void) {

	if (SIZE < class_len) {
		u1_int ret = file[SIZE];
//printf(" %02x  ",ret);
		SIZE++;
		return ret;
	}else
	printf("SIZE %d class_len %d\n",SIZE,class_len);

	return 0;
}

static u2_int read_u2(void) {
	u1_int u1 = read_u1();
	u1_int u2 = read_u1();
	return B2U2(u1, u2);
}

static u4_int read_u4(void) {
	u1_int u1 = read_u1();
	u1_int u2 = read_u1();
	u1_int u3 = read_u1();
	u1_int u4 = read_u1();
	return B2U4(u1, u2, u3, u4);
}

int check_valid_CP(const unsigned char* class_bytes, int class_data_len){

	class_len = class_data_len;
	file = class_bytes;	

	magic = read_u4();

	if (magic != 0xCAFEBABE) {
		javab_out(0, "not a class file");
		return 0;
	}

	read_u2();
	read_u2();
	u4_int i, j; /* wide counters */

	constant_pool_count = 	read_u2();

	if (constant_pool_count == 0u)	
		return 0;

	u1_int tag;
	for (i = 1u; i < constant_pool_count; i++) {
		tag = read_u1();

		switch (tag) {

		case CONSTANT_Class:
		case CONSTANT_String:
			
			read_u2();
			break;

		case CONSTANT_Fieldref:
		case CONSTANT_Methodref:
		case CONSTANT_InterfaceMethodref:
		case CONSTANT_NameAndType:
			read_u2();
			read_u2();
			break;

		case CONSTANT_Integer:
		case CONSTANT_Float:

			read_u4();
			break;

		case CONSTANT_Long:
		case CONSTANT_Double:

			read_u4();
			read_u4();
			++i;
			break;

		case CONSTANT_Utf8:

			/* Read-in constant string value (represented as BYTE sequence) */
		{
			u2_int len = read_u2();
			u1_int *s = (u1_int *) make_mem((1 + len) * sizeof(u1_int));

			for (j = 0u; j < len; j++)
				s[j] = read_u1();
			s[len] = '\0';
		}
			break;

		default:
			javab_out(-1, "invalid constant pool tag (%u) -- constant pool count %d i %d \n",tag,constant_pool_count,i);
			SIZE=0;	
			return 0;
		}
	}
SIZE=0;
return 1;
}

/* Read Constant Pool
 (entry constant_pool[0] is reserved, but included in count)
 *********************************************************** */

static void read_constant_pool(void) {
	u4_int i, j; /* wide counters */

	constant_pool_count = read_u2();
	constant_pool = NULL;

	if (constant_pool_count == 0u)
		return;

	/* Construct the constant pool */

	constant_pool = (constant_ptr *) make_mem(
			constant_pool_count * sizeof(constant_ptr));

	constant_pool[0] = NULL;

	for (i = 1u; i < constant_pool_count; i++) {

		if(error_1 == 1){
			break;
		}

		constant_pool[i] = (constant_ptr) make_mem(
				sizeof(struct constant_node));
		constant_pool[i]->tag = read_u1();

		switch (constant_pool[i]->tag) {

		case CONSTANT_Class:
		case CONSTANT_String:

			constant_pool[i]->u.indices.index1 = read_u2();
			constant_pool[i]->u.indices.index2 = 0u;
			break;

		case CONSTANT_Fieldref:
		case CONSTANT_Methodref:
		case CONSTANT_InterfaceMethodref:
		case CONSTANT_NameAndType:

			constant_pool[i]->u.indices.index1 = read_u2();
			constant_pool[i]->u.indices.index2 = read_u2();
			break;

		case CONSTANT_Integer:
		case CONSTANT_Float:

			constant_pool[i]->u.data.val1 = read_u4();
			constant_pool[i]->u.data.val2 = 0u;
			break;

		case CONSTANT_Long:
		case CONSTANT_Double:

			constant_pool[i]->u.data.val1 = read_u4();
			constant_pool[i]->u.data.val2 = read_u4();

			/* These entries make next entry invalid!
			 ************************************** */

			constant_pool[++i] = NULL;
			break;

		case CONSTANT_Utf8:

			/* Read-in constant string value (represented as BYTE sequence) */

		{
			u2_int len = read_u2();
			u1_int *s = (u1_int *) make_mem((1 + len) * sizeof(u1_int));

			for (j = 0u; j < len; j++)
				s[j] = read_u1();
			s[len] = '\0';

			constant_pool[i]->u.utf8.l = len;
			constant_pool[i]->u.utf8.s = s;
		}
			break;

		default:
			javab_out(-1, "invalid constant pool tag (%u)",
					constant_pool[i]->tag);
		}
	}
}

/* Read Interfaces
 **************** */

static void read_interfaces(void) {
	u4_int i; /* wide counter */

	interfaces_count = read_u2();

	if (interfaces_count != 0u) {
		interfaces = (u2_int *) make_mem(interfaces_count * sizeof(u2_int));
		for (i = 0u; i < interfaces_count; i++)
			interfaces[i] = read_u2();
	} else
		interfaces = NULL;
}

/* Read Attributes
 *************** */

static attribute_ptr *read_attributes(u2_int ac) {
	attribute_ptr *a = NULL;

	if (ac != 0u) {
		u4_int i, j; /* wide counters */
		u4_int len;

		a = (attribute_ptr *) make_mem(ac * sizeof(attribute_ptr));

		for (i = 0u; i < ac; i++) {

			a[i] = (attribute_ptr) new_attribute();

			a[i]->attribute_name_index = read_u2();
			a[i]->attribute_length = len = read_u4();

			if (len == U4MAX)
				javab_out(-1,
						"Sorry, my internal u4_int counter will wrap around");

			a[i]->info = (u1_int *) make_mem(len * sizeof(u1_int));

			for (j = 0u; j < len; j++)
				a[i]->info[j] = read_u1();
		}
	}
	return a;
}

/* Read Fields
 *********** */

static void read_fields(void) {
	u4_int i; /* wide counter */

	fields_count = read_u2();

	if (fields_count != 0u) {
		fields = (fm_ptr *) make_mem(fields_count * sizeof(fm_ptr));
		for (i = 0u; i < fields_count; i++) {
			fields[i] = (fm_ptr) make_mem(sizeof(struct fm_node));
			fields[i]->access_flags = read_u2();
			fields[i]->name_index = read_u2();
			fields[i]->descr_index = read_u2();
			fields[i]->attributes_count = read_u2();
			fields[i]->attributes = read_attributes(
					fields[i]->attributes_count);
		}
	} else
		fields = NULL;
}

/* Read Methods
 ************ */

static void read_methods(void) {
	u4_int i; /* wide counter */

	methods_count = read_u2();

	if (methods_count != 0u) {
		methods = (fm_ptr *) make_mem(methods_count * sizeof(fm_ptr));
		for (i = 0u; i < methods_count; i++) {
			methods[i] = (fm_ptr) make_mem(sizeof(struct fm_node));
			methods[i]->access_flags = read_u2();
			methods[i]->name_index = read_u2();
			methods[i]->descr_index = read_u2();
			methods[i]->attributes_count = read_u2();
			methods[i]->attributes = read_attributes(
					methods[i]->attributes_count);
		}
	} else
		methods = NULL;
}

/* Read Class-File
 *************** */

static  void read_classfile(void) {

	javab_out(2, " -- reading class file");

	/* Read magic 0xCAFEBABE string and version
	 **************************************** */

	magic = read_u4();

	if (magic != 0xCAFEBABE) {
		javab_out(0, "not a class file");
		return;
	}

	minor_version = read_u2();
//	major_version = read_u2();
	major_version = (u2_int)(0x31);
	SIZE+=2;

	/* Read constant pool
	 ****************** */

	read_constant_pool();

	/* Read flags and class info
	 ************************* */

	access_flags = read_u2();
	this_class = read_u2();
	super_class = read_u2();

	/* Read interfaces, fields, and methods
	 ************************************ */

	read_interfaces();
	read_fields();
	read_methods();

	/* Read attributes
	 *************** */

	attributes_count = read_u2();
	attributes = read_attributes(attributes_count);
	SIZE=0;
}

/* Check Attribute 
 *************** */

static void check_attr(u4_int ac, attribute_ptr *a) {
	u4_int i; /* wide counter */

	for (i = 0; i < ac; i++)
		valid_cp_entry(CONSTANT_Utf8, a[i]->attribute_name_index, "attribute");
}

/* Check Field/Method
 ****************** */

static void check_fm(fm_ptr f) {
	valid_cp_entry(CONSTANT_Utf8, f->name_index, "fm name index");
	valid_cp_entry(CONSTANT_Utf8, f->descr_index, "fm descriptor index");

	check_attr(f->attributes_count, f->attributes);
}

/* Check Class File
 **************** */

static void check_classfile(void) {
	u4_int i; /* wide counter */

	javab_out(2, " -- verifying class file");

	/* Check class references */

	valid_cp_entry(CONSTANT_Class, this_class, "this");

	if (super_class)
		valid_cp_entry(CONSTANT_Class, super_class, "super");

	/* Check constant pool */

	for (i = 1u; i < constant_pool_count; i++)
		if (constant_pool[i])
			switch (constant_pool[i]->tag) {

			case CONSTANT_Class:

				valid_cp_entry(CONSTANT_Utf8,
						constant_pool[i]->u.indices.index1, "Class");
				break;

			case CONSTANT_Fieldref:
			case CONSTANT_Methodref:
			case CONSTANT_InterfaceMethodref:

				valid_cp_entry(CONSTANT_Class,
						constant_pool[i]->u.indices.index1, "ref");
				valid_cp_entry(CONSTANT_NameAndType,
						constant_pool[i]->u.indices.index2, "ref");
				break;

			case CONSTANT_String:

				valid_cp_entry(CONSTANT_Utf8,
						constant_pool[i]->u.indices.index1, "String");
				break;

			case CONSTANT_NameAndType:

				valid_cp_entry(CONSTANT_Utf8,
						constant_pool[i]->u.indices.index1, "N_and_T");
				valid_cp_entry(CONSTANT_Utf8,
						constant_pool[i]->u.indices.index2, "N_and_T");

				{
					constant_ptr c =
							constant_pool[constant_pool[i]->u.indices.index2];

					if (c->u.utf8.l == 0)
						javab_out(0, "invalid field/method descriptor");
				}
				break;
			}

	/* Check interfaces */

	for (i = 0u; i < interfaces_count; i++)
		valid_cp_entry(CONSTANT_Class, interfaces[i], "interface");

	/* Check Fields */

	for (i = 0u; i < fields_count; i++)
		check_fm(fields[i]);

	/* Check Methods */

	for (i = 0u; i < methods_count; i++)
		check_fm(methods[i]);

	/* Check Attributes */

	check_attr(attributes_count, attributes);
}

/* Release Memory Fields of an Attribute
 ************************************* */

static void cleanup_attributes(u4_int cnt, attribute_ptr *a) {

	u4_int i, j; /* wide counters */

	for (i = 0u; i < cnt; i++)
		if (a[i]) {

			if (a[i]->info)
				free(a[i]->info);

			if (a[i]->reachable)
				free(a[i]->reachable);

			if (a[i]->is_leader)
				free(a[i]->is_leader);

			if (a[i]->sp_before)
				free(a[i]->sp_before);

			if (a[i]->my_bb)
				free(a[i]->my_bb);

			if (a[i]->st_state) {
				for (j = 0u; j < a[i]->code_length; j++)
					if (a[i]->st_state[j])
						free(a[i]->st_state[j]);
				free(a[i]->st_state);
			}

			free(a[i]);
		}

	if (a)
		free(a);
}

/* Release Memory of a Constant Pool Entry
 *************************************** */

static void del_cp(constant_ptr c) {
	if (c) {
		if ((c->tag == CONSTANT_Utf8) && (c->u.utf8.s))
			free(c->u.utf8.s);
		free(c);
	}
}

/* Release Memory of Class File
 **************************** */

static void delete_classfile(void) {

	u4_int i; /* wide counters */

	javab_out(2, " -- deleting class file");

	/* Delete Constant Pool
	 ******************** */

	for (i = 1u; i < constant_pool_count; i++)
		if (constant_pool[i])
			del_cp(constant_pool[i]);

	if (constant_pool)
		free(constant_pool);

	constant_pool_count = 0u;
	constant_pool = NULL;

	/* Delete Interfaces
	 ***************** */

	if (interfaces)
		free(interfaces);

	interfaces = NULL;

	/* Delete Fields
	 ************* */

	if (fields) {
		for (i = 0u; i < fields_count; i++)
			if (fields[i]) {
				cleanup_attributes(fields[i]->attributes_count,
						fields[i]->attributes);
				free(fields[i]);
			}
		free(fields);
	}

	fields_count = 0u;
	fields = NULL;

	/* Delete Methods
	 ************** */

	if (methods) {
		for (i = 0u; i < methods_count; i++)
			if (methods[i]) {
				cleanup_attributes(methods[i]->attributes_count,
						methods[i]->attributes);
				free(methods[i]);
			}
		free(methods);
	}

	methods_count = 0u;
	methods = NULL;

	/* Delete Attributes
	 **************** */

	cleanup_attributes(attributes_count, attributes);

	attributes_count = 0u;
	attributes = NULL;

	/* Delete Additional Space
	 *********************** */

	extra_cp = 0u;
	extra_field = 0u;
	extra_method = 0u;
}

/* Show a Field/Method
 ******************* */

static void show_fm(u4_int i, fm_ptr f, char *s) {

	u1_int *nm = constant_pool[f->name_index]->u.utf8.s;
	u1_int *tp = constant_pool[f->descr_index]->u.utf8.s;

	fprintf(stderr, "    %s[%5u]: 0x%02x %s %s (attr=%u)\n", s, i,
			f->access_flags, nm, tp, f->attributes_count);
}

/* Output of Class File Summary
 **************************** */

static void show_classfile(void) {
	u4_int i; /* wide counter */

	fprintf(stderr, "\n*** class file version  : %u.%u\n", major_version,
			minor_version);

	fprintf(stderr, "*** constant_pool_count : %u\n", constant_pool_count);

	for (i = 1u; i < constant_pool_count; i++)
		if (constant_pool[i]) {
			fprintf(stderr, "    constant_pool[%5u]: ", i);
			show_cp_entry(constant_pool[i]);
			fputc('\n', stderr);
		}

	fprintf(stderr, "*** access flags        : 0x%04x\n", access_flags);
	fprintf(stderr, "*** this_class          : %u\n", this_class);
	fprintf(stderr, "*** super_class         : %u\n", super_class);
	fprintf(stderr, "*** interfaces_count    : %u\n", interfaces_count);

	for (i = 0u; i < interfaces_count; i++) {
		u2_int i2 = interfaces[i];
		u1_int *s = constant_pool[constant_pool[i2]->u.indices.index1]->u.utf8.s;
		fprintf(stderr, "    interfaces   [%5u]: %u  \"%s\"\n", i, i2, s);
	}

	fprintf(stderr, "*** fields_count        : %u\n", fields_count);

	for (i = 0u; i < fields_count; i++)
		show_fm(i, fields[i], "fields       ");

	fprintf(stderr, "*** methods_count       : %u\n", methods_count);

	for (i = 0u; i < methods_count; i++)
		show_fm(i, methods[i], "methods      ");

	fprintf(stderr, "*** attributes_count    : %u\n", attributes_count);
}

/* ***********************************
 *** Restore Parts of the Old CP ***
 *********************************** */

static void add_shadow_cp(void) {
	u4_int i; /* wide counter */

	constant_pool_count = shadow_cnt;

	constant_pool = (constant_ptr *) make_mem(
			constant_pool_count * sizeof(constant_ptr));

	/* First Pass
	 ********** */

	for (i = 0u; i < shadow_cnt; i++) {

		constant_ptr old = shadow_cp[i];
		constant_ptr new = (constant_ptr) make_mem(
				sizeof(struct constant_node));

		if (old) {

#ifdef DEBUG_SHADOW
			fprintf(stderr, "copy old entry %u\n", i);
#endif

			new->tag = old->tag;

			switch (old->tag) {

			case CONSTANT_Class:
			case CONSTANT_Fieldref:
			case CONSTANT_Methodref:
			case CONSTANT_InterfaceMethodref:
			case CONSTANT_NameAndType:
			case CONSTANT_String:
				new->u.indices.index1 = old->u.indices.index1;
				new->u.indices.index2 = old->u.indices.index2;
				break;

			case CONSTANT_Integer:
			case CONSTANT_Float:
			case CONSTANT_Long:
			case CONSTANT_Double:
				new->u.data.val1 = old->u.data.val1;
				new->u.data.val2 = old->u.data.val2;
				break;

			case CONSTANT_Utf8:
				new->u.utf8.l = old->u.utf8.l;
				new->u.utf8.s = (u1_int *) strdup((char *) old->u.utf8.s);
				break;

			default:
				javab_out(-1, "invalid new shadow cp entry %u\n", old->tag);
			}

			constant_pool[i] = new;

			if ((old->tag == CONSTANT_Double) || (old->tag == CONSTANT_Long))
				i++; /* Account for NULL CP Entry */
		} else { /* Obsoleted Entry */

			new->tag = CONSTANT_Utf8;
			new->u.utf8.l = 1u;
			new->u.utf8.s = (u1_int *) strdup("-");

			constant_pool[i] = new;
		}
	}
}

/* ********************************************************
 *** PUBLIC FUNCTIONS                                 ***
 ******************************************************** */

/* Shadow Constant Pool Operations
 ******************************* */

void make_shadow_cp(void) {
	u4_int i; /* wide counter */

	if ((shadow_cnt) || (shadow_cp))
		javab_out(-1, "re-shadowing not allowed");

	shadow_cnt = constant_pool_count;
	shadow_cp = (constant_ptr *) make_mem(shadow_cnt * sizeof(constant_ptr));

	for (i = 0u; i < shadow_cnt; i++)
		shadow_cp[i] = NULL;
}

void mark_shadow_cp(u2_int index) {

	if (index < shadow_cnt) {

		if (!shadow_cp[index]) {

			constant_ptr c = constant_pool[index];
			shadow_cp[index] = c;

#ifdef DEBUG_SHADOW
			fprintf(stderr, "mark cp entry %u\n", index);
#endif

			switch (c->tag) {

			case CONSTANT_Class:
			case CONSTANT_String:
				mark_shadow_cp(c->u.indices.index1);
				break;

			case CONSTANT_Fieldref:
			case CONSTANT_Methodref:
			case CONSTANT_InterfaceMethodref:
			case CONSTANT_NameAndType:
				mark_shadow_cp(c->u.indices.index1);
				mark_shadow_cp(c->u.indices.index2);
				break;

			case CONSTANT_Integer:
			case CONSTANT_Float:
			case CONSTANT_Long:
			case CONSTANT_Double:
			case CONSTANT_Utf8:
				break;

			default:
				javab_out(-1, "invalid new shadow cp entry %u\n", c->tag);
			}
		}
	} else
		javab_out(-1, "invalid index into shadow cp %u", index);
}

void take_shadow_cp(void) {
	u4_int i; /* wide counter */

	for (i = 0u; i < shadow_cnt; i++)
		if (shadow_cp[i]) {
			constant_pool[i] = NULL;
#ifdef DEBUG_SHADOW
			fprintf(stderr, "take cp entry %u\n", i);
#endif
		}
}

void dump_shadow_cp(void) {
	u1_int set = 0u;
	u4_int i; /* wide counter */

	for (i = 0u; i < shadow_cnt; i++)
		if (shadow_cp[i]) {
			set = 1u;
			break;
		}

	if (set)
		add_shadow_cp();
	else
		constant_pool_count = 1u; /* reserved entry */
}

void elim_shadow_cp(void) {
	u4_int i; /* wide counter */

	if (shadow_cp) {
		for (i = 0u; i < shadow_cnt; i++)
			if (shadow_cp[i]) {
#ifdef DEBUG_SHADOW
				fprintf(stderr, "postponed deletion of cp entry %u\n", i);
#endif
				del_cp(shadow_cp[i]);
			}
		free(shadow_cp);
	}

	shadow_cnt = 0u;
	shadow_cp = NULL;
}

/* Class File Processing
 ********************* */

void process_classfile(const unsigned char *f, u1_int com) {

	file = f;

	switch (com) {

	case 0u: /* Process a Class File */

		if (f)
			read_classfile();
		break;

	case 1u: /* Check a Class File */

		check_classfile();
		break;

	case 2u: /* Show a Class File */

		show_classfile();
		break;

	case 3u: /* Delete a Class File */

		delete_classfile();
		break;
	}
}

/* Check constant pool entry
 ************************* */

u1_int valid_cp_entry(u1_int tag, u2_int entry, char *mess) {
	if ((entry != 0u) && (entry < constant_pool_count)) {
		constant_ptr p = constant_pool[entry];
		if ((!p) || (p->tag != tag)) {
			javab_out(0, "invalid reference of %s to constant pool (%u)", mess,
					entry);
			return 0;
		}
	} else {
		javab_out(0, "invalid index of %s into constant pool (%u)", mess,
				entry);
		return 0;
	}
	return 1u;
}

/* Output information in class file
 ******************************** */

void show_cp_entry(constant_ptr c) {
	switch (c->tag) {

	case CONSTANT_Class:

		fprintf(stderr, "<Class> ");
		show_cp_entry(constant_pool[c->u.indices.index1]);
		break;

	case CONSTANT_Fieldref:

		show_cp_entry(constant_pool[c->u.indices.index1]);
		fputc('.', stderr);
		show_cp_entry(constant_pool[c->u.indices.index2]);
		break;

	case CONSTANT_Methodref:

		show_cp_entry(constant_pool[c->u.indices.index1]);
		fputc('.', stderr);
		show_cp_entry(constant_pool[c->u.indices.index2]);
		break;

	case CONSTANT_InterfaceMethodref:

		show_cp_entry(constant_pool[c->u.indices.index1]);
		fputc('.', stderr);
		show_cp_entry(constant_pool[c->u.indices.index2]);
		break;

	case CONSTANT_String:

		fputc('\"', stderr);
		show_cp_entry(constant_pool[c->u.indices.index1]);
		fputc('\"', stderr);
		break;

	case CONSTANT_Integer:

		fprintf(stderr, "<Integer> %ix", (s4_int) c->u.data.val1);
		break;

	case CONSTANT_Float:

		fprintf(stderr, "<Float> 0x%04x", c->u.data.val1);
		break;

	case CONSTANT_Long:

		fprintf(stderr, "<Long> 0x%04x%04x", c->u.data.val1, c->u.data.val2);
		break;

	case CONSTANT_Double:

		fprintf(stderr, "<Double> 0x%04x%04x", c->u.data.val1, c->u.data.val2);
		break;

	case CONSTANT_NameAndType:

		show_cp_entry(constant_pool[c->u.indices.index1]);
		fputc(' ', stderr);
		show_cp_entry(constant_pool[c->u.indices.index2]);
		break;

	case CONSTANT_Utf8:

		if (c->u.utf8.s)
			fprintf(stderr, (char *) c->u.utf8.s);
		break;
	}
}

/* Obtain a new attribute
 ********************** */

attribute_ptr new_attribute(void) {

	attribute_ptr a = (attribute_ptr) make_mem(sizeof(struct attribute_node));

	a->attribute_name_index = 0u;
	a->attribute_length = 0u;
	a->info = NULL;

	/* JAVAB Specific Information */

	a->reachable = NULL;
	a->is_leader = NULL;
	a->sp_before = NULL;
	a->my_bb = NULL;
	a->st_state = NULL;

	return a;
}

/* *************************************
 *** Class File Insertion Routines ***
 ************************************* */

static void more_aux_cp(void) {
	if (extra_cp == 0u) {
		extra_cp = 50u;
		constant_pool = (constant_ptr *) more_mem(constant_pool,
				(constant_pool_count + extra_cp) * sizeof(constant_ptr));
	}
	extra_cp--;
}

void add_cp_entry(u1_int tag, char *str, u2_int i1, u2_int i2) {

	if (constant_pool_count < U2MAX) {

		/* Construct new Entry
		 ******************* */

		constant_ptr newcp = (constant_ptr) make_mem(
				sizeof(struct constant_node));
		newcp->tag = tag;

		switch (tag) {

		case CONSTANT_Utf8:

			newcp->u.utf8.l = (u2_int) strlen(str);
			newcp->u.utf8.s = (u1_int *) strdup(str);
			break;

		case CONSTANT_Class:
		case CONSTANT_String:
		case CONSTANT_Fieldref:
		case CONSTANT_Methodref:
		case CONSTANT_InterfaceMethodref:
		case CONSTANT_NameAndType:

			newcp->u.indices.index1 = i1;
			newcp->u.indices.index2 = i2;
			break;

		case CONSTANT_Integer:
		case CONSTANT_Float:
		case CONSTANT_Long:
		case CONSTANT_Double:
			newcp->u.data.val1 = 0u;
			newcp->u.data.val2 = 0u;
			break;

		default:
			javab_out(-1, "unsupported cp tag %u", tag);
		}

		/* Allocate More Entries and Copy
		 ****************************** */

		more_aux_cp();
		constant_pool[constant_pool_count++] = newcp;

		/* Account for NULL CP Entry
		 ************************* */

		if ((tag == CONSTANT_Double) || (tag == CONSTANT_Long)) {
			more_aux_cp();
			constant_pool[constant_pool_count++] = NULL;
		}
	} else
		javab_out(-1, "too many cp entries");
}

void add_field(fm_ptr f) {

	if (fields_count < U2MAX) {

		/* Allocate More Fields and Copy
		 ***************************** */

		if (extra_field == 0u) {
			extra_field = 50u;
			fields = (fm_ptr *) more_mem(fields,
					(fields_count + extra_field) * sizeof(fm_ptr));
		}
		extra_field--;

		fields[fields_count++] = f;
	} else
		javab_out(-1, "too many fields");
}

void add_method(fm_ptr f) {

	if (methods_count < U2MAX) {

		/* Allocate More Methods and Copy
		 ****************************** */

		if (extra_method == 0u) {
			extra_method = 50u;
			methods = (fm_ptr *) more_mem(methods,
					(methods_count + extra_method) * sizeof(fm_ptr));
		}
		extra_method--;

		methods[methods_count++] = f;
	} else
		javab_out(-1, "too many methods");
}
