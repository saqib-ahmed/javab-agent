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
#include "class.h"

/* ********************************************************
 *** EXTERNAL VARIABLES                               ***
 ******************************************************** */

/* PRIVATE
 ******* */

static FILE *dumpfile;
static int new_len = 0u;
static int new_pos = 0u;
int global_pos = 0u;
/* ********************************************************
 *** PRIVATE FUNCTIONS                                ***
 ******************************************************** */

/* write u1_int, u2_int, and u4_int routines
 **************************************** */

static void write_u1(u1_int u) {

	if (!worker_flag) {
		if (new_pos == new_len) {
			new_len += 500u;

			if (new_class_ptr)
				new_class_ptr = (u1_int *) more_mem(new_class_ptr,
						new_len * sizeof(u1_int));
			else
				new_class_ptr = (u1_int *) make_mem(new_len * sizeof(u1_int));
		}

		new_class_ptr[new_pos++] = u;
#if DEBUG>1
		fputc(u, dumpfile);
#endif
	}
	else {
		fputc(u, dumpfile);
	}

}

static void write_u2(u2_int u) {
	u1_int u1 = HIGB_U2(u);
	u1_int u2 = LOWB_U2(u);

	if (!worker_flag) {
		if (new_len - new_pos < 2) {
			new_len += 100u;

			if (new_class_ptr)
				new_class_ptr = (u1_int *) more_mem(new_class_ptr,
						new_len * sizeof(u1_int));
			else
				new_class_ptr = (u1_int *) make_mem(new_len * sizeof(u1_int));
		}

		new_class_ptr[new_pos++] = u1;
		new_class_ptr[new_pos++] = u2;
#if DEBUG>1
		fputc(u1, dumpfile);
		fputc(u2, dumpfile);
#endif
	} else {
		fputc(u1, dumpfile);
		fputc(u2, dumpfile);
	}

}

static void write_u4(u4_int u) {
	u1_int u1 = HIGB_U4(u);
	u1_int u2 = LOWB_U4(u);
	u1_int u3 = HIGB_U2(u);
	u1_int u4 = LOWB_U2(u);

	if (!worker_flag) {
		if (new_len - new_pos < 4) {
			new_len += 100u;

			if (new_class_ptr)
				new_class_ptr = (u1_int *) more_mem(new_class_ptr,
						new_len * sizeof(u1_int));
			else
				new_class_ptr = (u1_int *) make_mem(new_len * sizeof(u1_int));
		}

		new_class_ptr[new_pos++] = u1;
		new_class_ptr[new_pos++] = u2;
		new_class_ptr[new_pos++] = u3;
		new_class_ptr[new_pos++] = u4;
#if DEBUG>1
		fputc(u1, dumpfile);
		fputc(u2, dumpfile);
		fputc(u3, dumpfile);
		fputc(u4, dumpfile);
#endif
	} else {
		fputc(u1, dumpfile);
		fputc(u2, dumpfile);
		fputc(u3, dumpfile);
		fputc(u4, dumpfile);
	}

}

/* **********************************************************
 *** Output of the different components of a class file ***
 ********************************************************** */

/* output of attribute information
 ******************************* */

static void dump_attributes(u2_int cnt, attribute_ptr *a) {

	u4_int i, j; /* wide counters */

	write_u2(cnt);

	if (cnt != 0u) {

		if (!a)
			javab_out(-1, "lost attributes in dump_attributes()");

		for (i = 0u; i < cnt; i++) {

			if ((!a[i]) || (!a[i]->info))
				javab_out(-1, "lost attribute entry in dump_attributes()");
			else {

				u2_int ind = a[i]->attribute_name_index;
				u4_int len = a[i]->attribute_length;
				u1_int *info = a[i]->info;

				write_u2(ind);
				write_u4(len);

				for (j = 0u; j < len; j++)
					write_u1(info[j]);
			}
		}
	}
}

/* output of constant pool information 
 *********************************** */

static void dump_constant_pool(void) {

	u4_int i, j; /* wide counters */

	write_u2(constant_pool_count);

	if ((constant_pool_count == 0u) || (!constant_pool))
		javab_out(-1, "lost constant pool in dump_cpool()");

	for (i = 1u; i < constant_pool_count; i++) {

		constant_ptr ce = constant_pool[i];

		if (!ce)
			javab_out(-1, "lost pool entry in dump_cpool()");

		write_u1(ce->tag);

		switch (ce->tag) {

		case CONSTANT_Class:
		case CONSTANT_String:

			write_u2(ce->u.indices.index1);
			break;

		case CONSTANT_Fieldref:
		case CONSTANT_Methodref:
		case CONSTANT_InterfaceMethodref:
		case CONSTANT_NameAndType:

			write_u2(ce->u.indices.index1);
			write_u2(ce->u.indices.index2);
			break;

		case CONSTANT_Integer:
		case CONSTANT_Float:

			write_u4(ce->u.data.val1);
			break;

		case CONSTANT_Long:
		case CONSTANT_Double:

			write_u4(ce->u.data.val1);
			write_u4(ce->u.data.val2);

			i++; /* invalid next entry */

			break;

		case CONSTANT_Utf8:

		{
			u2_int l = ce->u.utf8.l;
			u1_int *s = ce->u.utf8.s;

			if (!s)
				javab_out(-1, "lost UTF8 string in dump_cpool()");

			write_u2(l);

			for (j = 0u; j < l; j++)
				write_u1(s[j]);
		}
			break;

		default:
			javab_out(-1, "invalid constant pool entry in dump_cpool()");
		}
	}
}

/* output of interface information
 ******************************* */

static void dump_interfaces(void) {

	u4_int i; /* wide counter */

	write_u2(interfaces_count);

	if (interfaces_count != 0u) {

		if (!interfaces)
			javab_out(-1, "lost interfaces in dump_interfaces()");

		for (i = 0u; i < interfaces_count; i++)
			write_u2(interfaces[i]);
	}
}

/* output of field information
 *************************** */

static void dump_fields(void) {

	u4_int i; /* wide counter */

	write_u2(fields_count);

	if (fields_count != 0u) {

		if (!fields)
			javab_out(-1, "lost fields in dump_fields()");

		for (i = 0u; i < fields_count; i++) {

			if (!fields[i])
				javab_out(-1, "lost field entry in dump_fields()");

			write_u2(fields[i]->access_flags);
			write_u2(fields[i]->name_index);
			write_u2(fields[i]->descr_index);

			dump_attributes(fields[i]->attributes_count, fields[i]->attributes);
		}
	}
}

/* output of method information
 **************************** */

static void dump_methods(void) {

	u4_int i; /* wide counter */

	write_u2(methods_count);

	if (methods_count != 0u) {

		if (!methods)
			javab_out(-1, "lost methods in dump_methods()");

		for (i = 0u; i < methods_count; i++) {

			if (!methods[i])
				javab_out(-1, "lost method entry in dump_methods()");

			write_u2(methods[i]->access_flags);
			write_u2(methods[i]->name_index);
			write_u2(methods[i]->descr_index);

			dump_attributes(methods[i]->attributes_count,
					methods[i]->attributes);
		}
	}
}

/* output of complete class file structure
 *************************************** */

static void dump_class(void) {

	write_u4(magic); /* magic    */

	write_u2(minor_version); /* versions */
	write_u2(major_version);

	dump_constant_pool();

	write_u2(access_flags); /* class info */
	write_u2(this_class);
	write_u2(super_class);

	dump_interfaces();
	dump_fields();
	dump_methods();
	dump_attributes(attributes_count, attributes);

	if (!worker_flag) {
		global_pos = new_pos;
		new_pos=0;
		new_len=0;
#if DEBUG>1
		printf("Size of the class is: %d\n", global_pos);
		for (int i = 0; i < global_pos; i += 4) {
			if (i % 16 == 0)
				printf("\n");

			printf("%02x%02x  %02x%02x  ", new_class_ptr[i],
					new_class_ptr[i + 1], new_class_ptr[i + 2],
					new_class_ptr[i + 3]);
		}
		printf("\n");
#endif
	}

}

/* ********************************************************
 *** PUBLIC FUNCTIONS                                 ***
 ******************************************************** */

void dump_classfile(FILE *f) {
	dumpfile = (f) ? f : stdout;
	dump_class();
}
