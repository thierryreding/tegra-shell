#ifndef TEGRA_SHELL_CHIP_H
#define TEGRA_SHELL_CHIP_H

struct tegra_shell_register_field {
	const char *name;
	unsigned int offset;
	unsigned int width;
};

struct tegra_shell_register {
	const char *name;
	unsigned int offset;
	unsigned int count;
	struct tegra_shell_register_field *fields;
	unsigned int num_fields;
};

struct tegra_shell_module {
	const char *name;
	unsigned long base;
	struct tegra_shell_register *registers;
	unsigned int num_registers;
};

struct tegra_shell_chip {
	const char *name;
	struct tegra_shell_module *modules;
	unsigned int num_modules;
};

struct tegra_shell_chip *tegra_shell_load(const char *name);
struct tegra_shell_module *tegra_shell_chip_find_module(struct tegra_shell_chip *chip, const char *name);
struct tegra_shell_register *tegra_shell_module_find_register(struct tegra_shell_module *module, const char *name, unsigned int *index);
void tegra_shell_register_decode(struct tegra_shell_register *reg, unsigned int index, unsigned long value);
void tegra_shell_register_describe(struct tegra_shell_register *reg, unsigned int index);

#endif
