#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

#include "chip.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct cli_options {
	const char *chip;
	bool help;
};

struct command {
	const char *name;
	int (*exec)(struct cli_options *options, int argc, char *argv[]);
};

static int exec_decode(struct cli_options *options, int argc, char *argv[])
{
	struct tegra_shell_module *module;
	struct tegra_shell_chip *chip;
	unsigned long value;

	/* requires at least the module name */
	if (argc < 2)
		return -EINVAL;

	chip = tegra_shell_load(options->chip);
	if (!chip)
		return -ENOENT;

	module = tegra_shell_chip_find_module(chip, argv[1]);
	if (!module)
		return -ENOENT;

	/*
	printf("%s (%lx)\n", module->name, module->base);
	*/

	if (argc > 2) {
		struct tegra_shell_register *reg;
		unsigned int index;

		reg = tegra_shell_module_find_register(module, argv[2], &index);
		if (!reg)
			return -ENOENT;

		if (argc > 3) {
			value = strtoul(argv[3], NULL, 0);

			tegra_shell_register_decode(reg, index, value);
		} else {
			tegra_shell_register_describe(reg, index);
		}
	} else {
		struct tegra_shell_register *reg;
		unsigned int i, j;

		for (i = 0; i < module->num_registers; i++) {
			reg = &module->registers[i];

			for (j = 0; j < reg->count; j++) {
				tegra_shell_register_describe(reg, j);
				printf("\n");
			}
		}
	}

	return 0;
}

static int exec_read(struct cli_options *options, int argc, char *argv[])
{
	struct tegra_shell_module *module;
	struct tegra_shell_chip *chip;
	int err = 0, fd;
	uint32_t value;
	off_t offset;
	void *base;

	/* requires at least the module name */
	if (argc < 2)
		return -EINVAL;

	chip = tegra_shell_load(options->chip);
	if (!chip)
		return -ENOENT;

	module = tegra_shell_chip_find_module(chip, argv[1]);
	if (!module)
		return -ENOENT;

	printf("%s (%lx)\n", module->name, module->base);
	offset = module->base;

	fd = open("/dev/mem", O_RDWR);
	if (fd < 0)
		return -errno;

	base = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, offset);
	if (base == MAP_FAILED) {
		fprintf(stderr, "mmap() failed: %m\n");
		err = -errno;
		goto close;
	}

	if (argc > 2) {
		struct tegra_shell_register *reg;
		unsigned int index;

		reg = tegra_shell_module_find_register(module, argv[2], &index);
		if (!reg) {
			err = -ENOENT;
			goto unmap;
		}

		value = *(uint32_t *)(base + reg->offset + index * 4);
		tegra_shell_register_decode(reg, index, value);
	} else {
		struct tegra_shell_register *reg;
		unsigned int i, j;

		for (i = 0; i < module->num_registers; i++) {
			reg = &module->registers[i];

			for (j = 0; j < reg->count; j++) {
				value = *(uint32_t *)(base + reg->offset + j * 4);
				tegra_shell_register_decode(reg, j, value);
				printf("\n");
			}
		}
	}

unmap:
	munmap(base, 4096);
close:
	close(fd);
	return err;
}

static const struct command commands[] = {
	{ "decode", exec_decode },
	{ "read", exec_read },
};

static void usage(FILE *fp, const char *program)
{
	fprintf(fp, "usage: %s [options] COMMAND\n", program);
}

static int process_command_line(struct cli_options *opts, int argc,
				char *argv[])
{
	static const struct option options[] = {
		{ "chip", 1, NULL, 'c' },
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, '\0' }
	};
	static const char *optstr = "h";
	int opt;

	while ((opt = getopt_long(argc, argv, optstr, options, NULL)) != -1) {
		switch (opt) {
		case 'c':
			opts->chip = optarg;
			break;

		case 'h':
			opts->help = true;
			break;

		default:
			return -EINVAL;
		}
	}

	return optind;
}

int main(int argc, char *argv[])
{
	const struct command *command = NULL;
	struct cli_options options;
	unsigned int i;
	int args, err;

	memset(&options, 0, sizeof(options));

	args = process_command_line(&options, argc, argv);
	if (args < 0) {
		fprintf(stderr, "failed to parse command-line: %d\n", args);
		return 1;
	}

	if (args >= argc) {
		usage(stderr, argv[0]);
		return 1;
	}

	if (options.help) {
		usage(stdout, argv[0]);
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[args], commands[i].name) == 0) {
			command = &commands[i];
			break;
		}
	}

	if (!command) {
		fprintf(stderr, "unknown command: %s\n", argv[1]);
		return 1;
	}

	err = command->exec(&options, argc - args, argv + args);
	if (err < 0) {
		/*
		fprintf(stderr, "command %s failed: %d\n", command->name, err);
		*/
		return 1;
	}

	return 0;
}
