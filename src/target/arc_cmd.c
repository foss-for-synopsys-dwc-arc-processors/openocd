/***************************************************************************
 *   Copyright (C) 2013-2015,2019 Synopsys, Inc.                           *
 *   Frank Dols <frank.dols@synopsys.com>                                  *
 *   Mischa Jonker <mischa.jonker@synopsys.com>                            *
 *   Anton Kolesov <anton.kolesov@synopsys.com>                            *
 *   Evgeniy Didin <didin@synopsys.com>
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "arc.h"

/* --------------------------------------------------------------------------
 *
 *   ARC targets expose command interface.
 *   It can be accessed via GDB through the (gdb) monitor command.
 *
 * ------------------------------------------------------------------------- */

/* Add flags register data type */
enum add_reg_type_flags {
	CFG_ADD_REG_TYPE_FLAGS_NAME,
	CFG_ADD_REG_TYPE_FLAGS_FLAG,
};

static Jim_Nvp nvp_add_reg_type_flags_opts[] = {
	{ .name = "-name",  .value = CFG_ADD_REG_TYPE_FLAGS_NAME },
	{ .name = "-flag",  .value = CFG_ADD_REG_TYPE_FLAGS_FLAG },
	{ .name = NULL,     .value = -1 }
};

int jim_arc_add_reg_type_flags(Jim_Interp *interp, int argc,
	Jim_Obj * const *argv)
{
	Jim_GetOptInfo goi;
	Jim_GetOpt_Setup(&goi, interp, argc-1, argv+1);

	LOG_DEBUG("-");

	int e = JIM_OK;

	/* Estimate number of registers as (argc - 2)/3 as each -flag option has 2
	 * arguments while -name is required. */
	unsigned int fields_sz = (goi.argc - 2) / 3;
	unsigned int cur_field = 0;

	struct arc_reg_data_type *type = calloc(1, sizeof(struct arc_reg_data_type));
	struct reg_data_type_flags *flags =
		calloc(1, sizeof(struct reg_data_type_flags));
	struct reg_data_type_flags_field *fields = calloc(fields_sz,
			sizeof(struct reg_data_type_flags_field));
	struct reg_data_type_bitfield *bitfields = calloc(fields_sz,
			sizeof(struct reg_data_type_bitfield));

	if (!(type && flags && fields && bitfields)) {
		free(type);
		free(flags);
		free(fields);
		free(bitfields);
	Jim_SetResultFormatted(goi.interp, "Failed to allocate memory.");
		return JIM_ERR;
	}

	/* Initialize type */
	type->data_type.type = REG_TYPE_ARCH_DEFINED;
	type->data_type.type_class = REG_TYPE_CLASS_FLAGS;
	type->data_type.reg_type_flags = flags;
	flags->size = 4; /* For now ARC has only 32-bit registers */

	while (goi.argc > 0 && e == JIM_OK) {
		Jim_Nvp *n;
		e = Jim_GetOpt_Nvp(&goi, nvp_add_reg_type_flags_opts, &n);
		if (e != JIM_OK) {
			Jim_GetOpt_NvpUnknown(&goi, nvp_add_reg_type_flags_opts, 0);
			continue;
		}

		switch (n->value) {
			case CFG_ADD_REG_TYPE_FLAGS_NAME:
			{
				const char *name;
				int name_len;
				if (goi.argc == 0) {
					Jim_WrongNumArgs(interp, goi.argc, goi.argv, "-name ?name? ...");
					e = JIM_ERR;
					break;
				}

				e = Jim_GetOpt_String(&goi, &name, &name_len);
				if (e == JIM_OK) {
					type->data_type.id = strndup(name, name_len);
					if (!type->data_type.id)
						e = JIM_ERR;
				}
				break;
			}

			case CFG_ADD_REG_TYPE_FLAGS_FLAG:
			{
				const char *field_name;
				int field_name_len;
				jim_wide start_position, end_position;

				if (goi.argc < 2) {
					Jim_WrongNumArgs(interp, goi.argc, goi.argv,
						"-flag ?name? ?position? ...");
					e = JIM_ERR;
					break;
				}

				/* Field name */
				e = Jim_GetOpt_String(&goi, &field_name, &field_name_len);
				if (e != JIM_OK)
					break;

				/* Field position. start == end, because flags
				 * are one-bit fields.  */

				fields[cur_field].name = strndup(field_name, field_name_len);
				if (!fields[cur_field].name) {
					e = JIM_ERR;
					break;
				}
				/* read start position */
				e = Jim_GetOpt_Wide(&goi, &start_position);
				if (e != JIM_OK)
					break;

				bitfields[cur_field].start = start_position;
				bitfields[cur_field].end = start_position;

				/* Check if any argnuments remain,
         * set bitfields[cur_field].end if flag is multibit */
				if (goi.argc > 0)
					/* Check current argv[0], if it is equal to "-flag",
           * than bitfields[cur_field].end remains start */
					if (strcmp(Jim_String(goi.argv[0]),"-flag")){
						e = Jim_GetOpt_Wide(&goi, &end_position);
						if (e != JIM_OK)
							break;
						bitfields[cur_field].end = end_position;
						bitfields[cur_field].type = REG_TYPE_INT;
					}

				fields[cur_field].bitfield = &(bitfields[cur_field]);
				if (cur_field > 0)
					fields[cur_field - 1].next = &(fields[cur_field]);
				else
					flags->fields = fields;

				cur_field += 1;

				break;
			}
		}
	}

	if (!type->data_type.id) {
		Jim_SetResultFormatted(goi.interp, "-name is a required option");
		e = JIM_ERR;
	}

	if (e == JIM_OK) {
		struct command_context *ctx;
		struct target *target;

		ctx = current_command_context(interp);
		assert(ctx);
		target = get_current_target(ctx);
		if (!target) {
			Jim_SetResultFormatted(goi.interp, "No current target");
			e = JIM_ERR;
		} else {
			arc_add_reg_data_type(target, type);
		}
	}

	if (e != JIM_OK) {
		free((void*)type->data_type.id);
		free(type);
		free(flags);
		/* `fields` is zeroed, so for uninitialized fields "name" is NULL. */
		for (unsigned int i = 0; i < fields_sz; i++)
			free((void*)fields[i].name);
		free(fields);
		free(bitfields);
		return e;
	}

	LOG_DEBUG("added flags type {name=%s}", type->data_type.id);

	return JIM_OK;
}

/* Add struct register data type */
enum add_reg_type_struct {
	CFG_ADD_REG_TYPE_STRUCT_NAME,
	CFG_ADD_REG_TYPE_STRUCT_BITFIELD,
};

static Jim_Nvp nvp_add_reg_type_struct_opts[] = {
	{ .name = "-name",     .value = CFG_ADD_REG_TYPE_STRUCT_NAME },
	{ .name = "-bitfield", .value = CFG_ADD_REG_TYPE_STRUCT_BITFIELD },
	{ .name = NULL,     .value = -1 }
};


/* This function supports only bitfields. */
int jim_arc_add_reg_type_struct(Jim_Interp *interp, int argc,
	Jim_Obj * const *argv)
{
	Jim_GetOptInfo goi;
	Jim_GetOpt_Setup(&goi, interp, argc-1, argv+1);

	LOG_DEBUG("-");

	int e = JIM_OK;

	/* Estimate number of registers as (argc - 2)/4 as each -bitfield option has 3
	 * arguments while -name is required. */
	unsigned int fields_sz = (goi.argc - 2) / 4;
	unsigned int cur_field = 0;

	struct arc_reg_data_type *type = calloc(1, sizeof(struct arc_reg_data_type));
	struct reg_data_type_struct *struct_type =
		calloc(1, sizeof(struct reg_data_type_struct));
	struct reg_data_type_struct_field *fields =
		calloc(fields_sz, sizeof(struct reg_data_type_struct_field));
	struct reg_data_type_bitfield *bitfields =
		calloc(fields_sz, sizeof(struct reg_data_type_bitfield));

	if (!(type && struct_type && fields && bitfields)) {
		free(type);
		free(struct_type);
		free(fields);
		free(bitfields);
		Jim_SetResultFormatted(goi.interp, "Failed to allocate memory.");
		return JIM_ERR;
	}

	/* Initialize type */
	type->data_type.type = REG_TYPE_ARCH_DEFINED;
	type->data_type.type_class = REG_TYPE_CLASS_STRUCT;
	type->data_type.reg_type_struct = struct_type;
	struct_type->size = 4; /* For now ARC has only 32-bit registers */

	while (goi.argc > 0 && e == JIM_OK) {
		Jim_Nvp *n;
		e = Jim_GetOpt_Nvp(&goi, nvp_add_reg_type_struct_opts, &n);
		if (e != JIM_OK) {
			Jim_GetOpt_NvpUnknown(&goi, nvp_add_reg_type_struct_opts, 0);
			continue;
		}

		switch (n->value) {
			case CFG_ADD_REG_TYPE_STRUCT_NAME:
			{
				const char *name;
				int name_len;
				if (goi.argc == 0) {
					Jim_WrongNumArgs(interp, goi.argc, goi.argv, "-name ?name? ...");
					e = JIM_ERR;
					break;
				}

				e = Jim_GetOpt_String(&goi, &name, &name_len);
				if (e == JIM_OK) {
					type->data_type.id = strndup(name, name_len);
					if (!type->data_type.id)
						e = JIM_ERR;
				}

				break;
			}
			case CFG_ADD_REG_TYPE_STRUCT_BITFIELD:
			{
				const char *field_name;
				int field_name_len;
				jim_wide start, end;

				if (goi.argc < 3) {
					Jim_WrongNumArgs(interp, goi.argc, goi.argv,
						"-bitfield ?name? ?start? ?end? ...");
					e = JIM_ERR;
					break;
				}

				/* Field name */
				e = Jim_GetOpt_String(&goi, &field_name, &field_name_len);
				if (e != JIM_OK)
					break;

				/* Bit-field start */
				e = Jim_GetOpt_Wide(&goi, &start);
				if (e != JIM_OK)
					break;

				/* Bit-field end */
				e = Jim_GetOpt_Wide(&goi, &end);
				if (e != JIM_OK)
					break;

				fields[cur_field].name = strndup(field_name, field_name_len);
				if (!fields[cur_field].name) {
					e = JIM_ERR;
					break;
				}
				bitfields[cur_field].start = start;
				bitfields[cur_field].end = end;
				bitfields[cur_field].type = REG_TYPE_INT;
				fields[cur_field].bitfield = &(bitfields[cur_field]);
				/* Only bitfields are supported so far. */
				fields[cur_field].use_bitfields = true;
				if (cur_field > 0)
					fields[cur_field - 1].next = &(fields[cur_field]);
				else
					struct_type->fields = fields;

				cur_field += 1;

				break;
			}
		}
	}

	if (!type->data_type.id) {
		Jim_SetResultFormatted(goi.interp, "-name is a required option");
		e = JIM_ERR;
	}

	if (e == JIM_OK) {
		struct command_context *ctx;
		struct target *target;

		ctx = current_command_context(interp);
		assert(ctx);
		target = get_current_target(ctx);
		if (!target) {
			Jim_SetResultFormatted(goi.interp, "No current target");
			e = JIM_ERR;
		} else {
			arc_add_reg_data_type(target, type);
		}
	}

	if (e != JIM_OK) {
		free((void*)type->data_type.id);
		free(type);
		free(struct_type);
		/* `fields` is zeroed, so for uninitialized fields "name" is NULL. */
		for (unsigned int i = 0; i < fields_sz; i++)
			free((void*)fields[i].name);
		free(fields);
		free(bitfields);
		return e;
	}

	LOG_DEBUG("added struct type {name=%s}", type->data_type.id);

	return JIM_OK;
}



/* ----- Exported target commands ------------------------------------------ */

static const struct command_registration arc_core_command_handlers[] = {
 {
		.name = "add-reg-type-flags",
		.jim_handler = jim_arc_add_reg_type_flags,
		.mode = COMMAND_CONFIG,
		.usage = "-name ?string? (-flag ?name? ?position?)+",
		.help = "Add new 'flags' register data type. Only single bit flags "
			"are supported. Type name is global. Bitsize of register is fixed "
			"at 32 bits.",
	},
 {
		.name = "add-reg-type-struct",
		.jim_handler = jim_arc_add_reg_type_struct,
		.mode = COMMAND_CONFIG,
		.usage = "-name ?string? (-bitfield ?name? ?start? ?end?)+",
		.help = "Add new 'struct' register data type. Only bit-fields are "
			"supported so far, which means that for each bitfield start and end "
			"position bits must be specified. GDB also support type-fields, "
			"where common type can be used instead. Type name is global. Bitsize of "
			"register is fixed at 32 bits.",
	},
	COMMAND_REGISTRATION_DONE
};

const struct command_registration arc_monitor_command_handlers[] = {
	{
		.name = "arc",
		.mode = COMMAND_ANY,
		.help = "ARC monitor command group",
		.usage = "Help info ...",
		.chain = arc_core_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};
