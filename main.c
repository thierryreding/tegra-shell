#include <stdio.h>
#include <stdlib.h>

#include "chip.h"

int main(int argc, char *argv[])
{
	struct tegra_shell_module *module;
	struct tegra_shell_chip *chip;
	unsigned long value;

	chip = tegra_shell_load(argv[1]);

	module = tegra_shell_chip_find_module(chip, argv[2]);

	if (argc > 3) {
		struct tegra_shell_register *reg;

		reg = tegra_shell_module_find_register(module, argv[3]);

		if (argc > 4) {
			value = strtoul(argv[4], NULL, 0);

			tegra_shell_register_decode(reg, value);
		} else {
			tegra_shell_register_describe(reg);
		}
	} else {
		unsigned int i;

		for (i = 0; i < module->num_registers; i++) {
			tegra_shell_register_describe(&module->registers[i]);
			printf("\n");
		}
	}

	return 0;
}
