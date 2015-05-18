#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "chip.h"

int tegra_shell_parse_field(struct tegra_shell_register_field *field,
			    xmlNode *root)
{
	xmlAttr *prop;

	for (prop = root->properties; prop; prop = prop->next) {
		char *value = (char *)xmlGetProp(root, prop->name);

		if (xmlStrcmp(prop->name, (xmlChar *)"name") == 0)
			field->name = strdup(value);

		if (xmlStrcmp(prop->name, (xmlChar *)"offset") == 0)
			field->offset = strtoul(value, NULL, 0);

		if (xmlStrcmp(prop->name, (xmlChar *)"width") == 0)
			field->width = strtoul(value, NULL, 0);
	}

	return 0;
}

int tegra_shell_parse_register(struct tegra_shell_register *reg, xmlNode *root)
{
	struct tegra_shell_register_field *field;
	unsigned int i = 0;
	xmlAttr *prop;
	xmlNode *node;

	for (prop = root->properties; prop; prop = prop->next) {
		char *value = (char *)xmlGetProp(root, prop->name);

		if (xmlStrcmp(prop->name, (xmlChar *)"name") == 0)
			reg->name = strdup(value);

		if (xmlStrcmp(prop->name, (xmlChar *)"offset") == 0)
			reg->offset = strtoul(value, NULL, 0);
	}

	for (node = root->children; node; node = node->next) {
		if (xmlStrcmp(node->name, (xmlChar *)"field") == 0)
			reg->num_fields++;
	}

	reg->fields = calloc(reg->num_fields, sizeof(*field));
	if (!reg->fields)
		return -ENOMEM;

	for (node = root->children; node; node = node->next) {
		if (xmlStrcmp(node->name, (xmlChar *)"field") == 0) {
			field = &reg->fields[i];
			tegra_shell_parse_field(field, node);
			i++;
		}
	}

	return 0;
}

int __tegra_shell_module_parse(struct tegra_shell_module *module, xmlNode *root)
{
	struct tegra_shell_register *reg;
	unsigned int i = 0;
	xmlAttr *prop;
	xmlNode *node;

	for (prop = root->properties; prop; prop = prop->next) {
		char *value = (char *)xmlGetProp(root, prop->name);

		if (xmlStrcmp(prop->name, (xmlChar *)"name") == 0)
			module->name = strdup(value);

		if (xmlStrcmp(prop->name, (xmlChar *)"base") == 0)
			module->base = strtoul(value, NULL, 0);
	}

	for (node = root->children; node; node = node->next) {
		if (xmlStrcmp(node->name, (xmlChar *)"register") == 0)
			module->num_registers++;
	}

	module->registers = calloc(module->num_registers, sizeof(*reg));
	if (!module->registers) {
		free(module);
		return -ENOMEM;
	}

	for (node = root->children; node; node = node->next) {
		if (xmlStrcmp(node->name, (xmlChar *)"register") == 0) {
			reg = &module->registers[i];
			tegra_shell_parse_register(reg, node);
			i++;
		}
	}

	return 0;
}

int tegra_shell_module_parse(struct tegra_shell_module *module, xmlNode *root)
{
	xmlNode *node;

	for (node = root; node; node = node->next)
		if (xmlStrcmp(node->name, (xmlChar *)"module") == 0)
			return __tegra_shell_module_parse(module, node);

	return -ENOENT;
}

static int tegra_shell_module_load(struct tegra_shell_module *module,
				   const char *filename)
{
	xmlNode *root;
	xmlDoc *doc;
	int err;

	doc = xmlReadFile(filename, NULL, 0);
	if (!doc) {
		fprintf(stderr, "failed to load %s\n", filename);
		return -EIO;
	}

	root = xmlDocGetRootElement(doc);

	err = tegra_shell_module_parse(module, root);
	if (err < 0)
		return err;

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return 0;
}

void tegra_shell_register_decode(struct tegra_shell_register *reg,
				 unsigned long value)
{
	struct tegra_shell_register_field *field;
	unsigned int i, width = 0;

	printf("%s (%08x) = %08lx\n", reg->name, reg->offset, value);

	for (i = 0; i < reg->num_fields; i++) {
		int len = strlen(reg->fields[i].name);
		if (len > width)
			width = len;
	}

	for (i = 0; i < reg->num_fields; i++) {
		unsigned int end, nibbles, mask;

		field = &reg->fields[i];

		end = field->offset + field->width - 1;
		nibbles = ((field->width + 7) / 8) * 2;
		mask = (1 << field->width) - 1;

		printf("\t[%02u:%02u] ", end, field->offset);
		printf("%*s = %0*lx\n", width, field->name, nibbles,
		       (value >> field->offset) & mask);
	}
}

void tegra_shell_register_describe(struct tegra_shell_register *reg)
{
	struct tegra_shell_register_field *field;
	unsigned int i, width = 0;

	printf("%s (%08x)\n", reg->name, reg->offset);

	for (i = 0; i < reg->num_fields; i++) {
		int len = strlen(reg->fields[i].name);
		if (len > width)
			width = len;
	}

	for (i = 0; i < reg->num_fields; i++) {
		unsigned int end;

		field = &reg->fields[i];

		end = field->offset + field->width - 1;

		printf("\t[%02u:%02u] ", end, field->offset);
		printf("%*s\n", width, field->name);
	}
}

struct tegra_shell_register *
tegra_shell_module_find_register(struct tegra_shell_module *module,
				 const char *name)
{
	unsigned int i;

	for (i = 0; i < module->num_registers; i++)
		if (strcmp(name, module->registers[i].name) == 0)
			return &module->registers[i];

	return NULL;
}

struct tegra_shell_module *tegra_shell_chip_find_module(struct tegra_shell_chip *chip, const char *name)
{
	unsigned int i;

	for (i = 0; i < chip->num_modules; i++)
		if (strcmp(chip->modules[i].name, name) == 0)
			return &chip->modules[i];

	return NULL;
}

struct tegra_shell_chip *tegra_shell_load(const char *name)
{
	struct tegra_shell_module *module;
	struct tegra_shell_chip *chip;
	char *directory, *filename;
	struct dirent *ent;
	unsigned int i = 0;
	DIR *dir;
	int err;

	chip = calloc(1, sizeof(*chip));
	if (!chip)
		return NULL;

	chip->name = strdup(name);

	err = asprintf(&directory, "chips/%s", name);
	if (err < 0) {
		free(chip);
		return NULL;
	}

	dir = opendir(directory);
	if (!dir) {
		fprintf(stderr, "failed to open directory %s: %m\n", directory);
		free(directory);
		free(chip);
		return NULL;
	}

	free(directory);

	while ((ent = readdir(dir)) != NULL)
		if (strstr(ent->d_name, ".xml"))
			chip->num_modules++;

	chip->modules = calloc(chip->num_modules, sizeof(*module));
	if (!chip->modules)
		return NULL;

	rewinddir(dir);

	while ((ent = readdir(dir)) != NULL) {
		if (strstr(ent->d_name, ".xml")) {
			module = &chip->modules[i];

			err = asprintf(&filename, "chips/%s/%s", name,
				       ent->d_name);
			if (err < 0) {
				continue;
			}

			tegra_shell_module_load(module, filename);

			free(filename);
			i++;
		}
	}

	closedir(dir);

	return chip;
}
