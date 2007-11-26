#include <config.h>

#include <errno.h>
#include <string.h>

#include "write_fluffy.h"
#include "ast_t.h"
#include "type_t.h"
#include "type.h"
#include "adt/error.h"

static const context_t *global_context;
static FILE            *out;

static void write_type(const type_t *type);

static const char *get_atomic_type_string(const atomic_type_type_t type)
{
	switch(type) {
	case ATOMIC_TYPE_VOID:       return "void";
	case ATOMIC_TYPE_CHAR:       return "byte";
	case ATOMIC_TYPE_SCHAR:      return "byte";
	case ATOMIC_TYPE_UCHAR:      return "unsigned byte";
	case ATOMIC_TYPE_SHORT:	     return "short";
	case ATOMIC_TYPE_USHORT:     return "unsigned short";
	case ATOMIC_TYPE_INT:        return "int";
	case ATOMIC_TYPE_UINT:       return "unsigned int";
	case ATOMIC_TYPE_LONG:       return "int";
	case ATOMIC_TYPE_ULONG:      return "unsigned int";
	case ATOMIC_TYPE_LONGLONG:   return "long";
	case ATOMIC_TYPE_ULONGLONG:  return "unsigned long";
	case ATOMIC_TYPE_FLOAT:      return "float";
	case ATOMIC_TYPE_DOUBLE:     return "double";
	case ATOMIC_TYPE_LONG_DOUBLE: return "double";
	case ATOMIC_TYPE_BOOL:       return "bool";
	default:                     panic("unsupported atomic type");
	}
}

static void write_atomic_type(const type_t *type)
{
	fprintf(out, "%s", get_atomic_type_string(type->v.atomic_type.atype));
}

static void write_pointer_type(const type_t *type)
{
	write_type(type->v.pointer_type.points_to);
	fputc('*', out);
}

static declaration_t *find_typedef(const type_t *type)
{
	/* first: search for a matching typedef in the global type... */
	declaration_t *declaration = global_context->declarations;
	while(declaration != NULL) {
		if(! (declaration->storage_class == STORAGE_CLASS_TYPEDEF)) {
			declaration = declaration->next;
			continue;
		}
		if(declaration->type == type)
			break;
		declaration = declaration->next;
	}

	return declaration;
}

static void write_compound_type(const type_t *type)
{
	declaration_t *declaration = find_typedef(type);
	if(declaration != NULL) {
		fprintf(out, "%s", declaration->symbol->string);
		return;
	}

	/* does the struct have a name? */
	symbol_t *symbol = type->v.compound_type.declaration->symbol;
	if(symbol != NULL) {
		/* TODO: make sure we create a struct for it... */
		fprintf(out, "%s", symbol->string);
		return;
	}
	/* TODO: create a struct and use its name here... */
	fprintf(out, "/* TODO anonymous struct */byte");
}

static void write_enum_type(const type_t *type)
{
	declaration_t *declaration = find_typedef(type);
	if(declaration != NULL) {
		fprintf(out, "%s", declaration->symbol->string);
		return;
	}

	/* does the enum have a name? */
	symbol_t *symbol = type->v.enum_type.declaration->symbol;
	if(symbol != NULL) {
		/* TODO: make sure we create an enum for it... */
		fprintf(out, "%s", symbol->string);
		return;
	}
	/* TODO: create a struct and use its name here... */
	fprintf(out, "/* TODO anonymous enum */byte");
}

static void write_function_type(const type_t *type)
{
	fprintf(out, "(func(");

	function_parameter_t *parameter = type->v.function_type.parameters;
	int                   first     = 1;
	while(parameter != NULL) {
		if(!first) {
			fprintf(out, ", ");
		} else {
			first = 0;
		}

#if 0
		if(parameter->symbol != NULL) {
			fprintf(out, "%s : ", parameter->symbol->string);
		} else {
			/* TODO make up some unused names (or allow _ in fluffy?) */
			fprintf(out, "_ : ");
		}
#endif
		fputs("_ : ", out);
		write_type(parameter->type);

		parameter = parameter->next;
	}

	fprintf(out, ") : ");
	write_type(type->v.function_type.result_type);
	fprintf(out, ")");
}

static void write_type(const type_t *type)
{
	switch(type->type) {
	case TYPE_ATOMIC:
		write_atomic_type(type);
		return;
	case TYPE_POINTER:
		write_pointer_type(type);
		return;
	case TYPE_COMPOUND_UNION:
	case TYPE_COMPOUND_STRUCT:
		write_compound_type(type);
		return;
	case TYPE_ENUM:
		write_enum_type(type);
		return;
	case TYPE_FUNCTION:
		write_function_type(type);
		return;
	case TYPE_INVALID:
		panic("invalid type found");
		break;
	default:
		fprintf(out, "/* TODO type */");
		break;
	}
}

static void write_struct_entry(const declaration_t *declaration)
{
	fprintf(out, "\t%s : ", declaration->symbol->string);
	write_type(declaration->type);
	fprintf(out, "\n");
}

static void write_struct(const symbol_t *symbol, const type_t *type)
{
	fprintf(out, "struct %s:\n", symbol->string);

	const declaration_t *declaration = type->v.compound_type.declaration->context.declarations;
	while(declaration != NULL) {
		write_struct_entry(declaration);
		declaration = declaration->next;
	}

	fprintf(out, "\n");
}

static void write_union(const symbol_t *symbol, const type_t *type)
{
	fprintf(out, "union %s:\n", symbol->string);

	const declaration_t *declaration = type->v.compound_type.declaration->context.declarations;
	while(declaration != NULL) {
		write_struct_entry(declaration);
		declaration = declaration->next;
	}

	fprintf(out, "\n");
}

static void write_expression(const expression_t *expression);

static void write_unary_expression(const unary_expression_t *expression)
{
	switch(expression->type) {
	case UNEXPR_NEGATE:
		fputc('-', out);
		break;
	case UNEXPR_NOT:
		fputc('!', out);
		break;
	default:
		panic("unimplemented unary expression found");
	}
	write_expression(expression->value);
}

static void write_expression(const expression_t *expression)
{
	const const_t *constant;
	/* TODO */
	switch(expression->type) {
	case EXPR_CONST:
		constant = (const const_t*) expression;
		if(is_type_integer(expression->datatype)) {
			fprintf(out, "%lld", constant->v.int_value);
		} else {
			fprintf(out, "%Lf", constant->v.float_value);
		}
		break;
	case EXPR_UNARY:
		write_unary_expression((const unary_expression_t*) expression);
		break;
	default:
		panic("not implemented expression");
	}
}

static void write_enum(const symbol_t *symbol, const type_t *type)
{
	fprintf(out, "enum %s:\n", symbol->string);

	declaration_t *entry = type->v.enum_type.declaration->next;
	for ( ; entry != NULL && entry->storage_class == STORAGE_CLASS_ENUM_ENTRY;
			entry = entry->next) {
		fprintf(out, "\t%s", entry->symbol->string);
		if(entry->init.initializer != NULL) {
			fprintf(out, " <- ");
			write_expression(entry->init.enum_value);
		}
		fputc('\n', out);
	}
	fprintf(out, "typealias %s <- int\n", symbol->string);
	fprintf(out, "\n");
}

static void write_variable(const declaration_t *declaration)
{
	fprintf(out, "var %s : ", declaration->symbol->string);
	write_type(declaration->type);
	/* TODO: initializers */
	fprintf(out, "\n");
}

static void write_function(const declaration_t *declaration)
{
	if(declaration->init.statement != NULL) {
		fprintf(stderr, "Warning: can't convert function bodies (at %s)\n",
		        declaration->symbol->string);
	}

	fprintf(out, "func extern %s(",
	        declaration->symbol->string);

	const type_t *function_type = declaration->type;

	declaration_t *parameter = declaration->context.declarations;
	int            first     = 1;
	for( ; parameter != NULL; parameter = parameter->next) {
		if(!first) {
			fprintf(out, ", ");
		} else {
			first = 0;
		}
		if(parameter->symbol != NULL) {
			fprintf(out, "%s : ", parameter->symbol->string);
		} else {
			fputs("_ : ", out);
		}
		write_type(parameter->type);
	}
	if(function_type->v.function_type.variadic) {
		if(!first) {
			fprintf(out, ", ");
		} else {
			first = 0;
		}
		fputs("...", out);
	}
	fprintf(out, ")");

	const type_t *result_type = function_type->v.function_type.result_type;
	if(result_type->type != TYPE_ATOMIC ||
	   result_type->v.atomic_type.atype != ATOMIC_TYPE_VOID) {
		fprintf(out, " : ");
		write_type(result_type);
	}
	fputc('\n', out);
}

void write_fluffy_decls(const translation_unit_t *unit)
{
#if 0
	out = fopen("out.fluffy", "w");
	if(out == NULL) {
		fprintf(stderr, "Couldn't open out.fluffy: %s\n", strerror(errno));
		exit(1);
	}
#endif
	out            = stdout;
	global_context = &unit->context;

	fprintf(out, "/* WARNING: Automatically generated file */\n");

	/* write structs, unions + enums */
	declaration_t *declaration = unit->context.declarations;
	for( ; declaration != NULL; declaration = declaration->next) {
		//fprintf(out, "// Decl: %s\n", declaration->symbol->string);
		if(! (declaration->storage_class == STORAGE_CLASS_TYPEDEF)) {
			continue;
		}
		type_t *type = declaration->type;
		if(type->type == TYPE_COMPOUND_STRUCT) {
			write_struct(declaration->symbol, type);
		} else if(type->type == TYPE_COMPOUND_UNION) {
			write_union(declaration->symbol, type);
		} else if(type->type == TYPE_ENUM) {
			write_enum(declaration->symbol, type);
		}
	}

	/* write global variables */
	declaration = unit->context.declarations;
	for( ; declaration != NULL; declaration = declaration->next) {
		if(declaration->namespc != NAMESPACE_NORMAL)
			continue;
		if(declaration->storage_class == STORAGE_CLASS_TYPEDEF
				|| declaration->storage_class == STORAGE_CLASS_ENUM_ENTRY)
			continue;

		type_t *type = declaration->type;
		if(type->type == TYPE_FUNCTION)
			continue;

		write_variable(declaration);
	}

	/* write functions */
	declaration = unit->context.declarations;
	for( ; declaration != NULL; declaration = declaration->next) {
		if(declaration->namespc != NAMESPACE_NORMAL)
			continue;
		if(declaration->storage_class == STORAGE_CLASS_TYPEDEF
				|| declaration->storage_class == STORAGE_CLASS_ENUM_ENTRY)
			continue;

		type_t *type = declaration->type;
		if(type->type != TYPE_FUNCTION)
			continue;

		write_function(declaration);
	}

	//fclose(out);
}
