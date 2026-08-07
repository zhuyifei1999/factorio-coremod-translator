/* SPDX-License-Identifier: GPL-2.0 */

#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <sys/auxv.h>
#include <unistd.h>

#include <gelf.h>
#include <libelf.h>
#include <libiberty/demangle.h>

#include "translator.h"

uintptr_t get_symbol_addr(char *symbol)
{
	uintptr_t entrypoint;
	Elf_Scn *scn = NULL;
	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	Elf_Data *data;
	GElf_Sym sym;
	char *name;
	int fd, i;
	Elf *elf;

	elf_version(EV_CURRENT);

	fd = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		error(1, errno, "get_symbol_addr: openat");

	elf = elf_begin(fd, ELF_C_READ, NULL);
	if (!elf)
		error(1, 0, "get_symbol_addr: elf_begin: %s", elf_errmsg(0));

	if (!gelf_getehdr(elf, &ehdr))
		error(1, 0, "get_symbol_addr: gelf_getehdr: %s", elf_errmsg(0));
	entrypoint = ehdr.e_entry;

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);
		if (shdr.sh_type == SHT_SYMTAB)
			break;
	}

	if (!scn)
		error(1, 0, "get_symbol_addr: No SHT_SYMTAB found");

	data = elf_getdata(scn, NULL);
	for (i = 0; i < shdr.sh_size / shdr.sh_entsize; ++i) {
		gelf_getsym(data, i, &sym);

		name = elf_strptr(elf, shdr.sh_link, sym.st_name);
		if (!name)
			continue;
		name = cplus_demangle(name, DMGL_NO_OPTS) ?: name;

		if (strcmp(name, symbol))
			continue;

		elf_end(elf);
		close(fd);
		return sym.st_value + (getauxval(AT_ENTRY) - entrypoint);
	}

	error(1, 0, "get_symbol_addr: Symbol not found: %s", symbol);
	__builtin_unreachable();
}

static const char expected_a_head[] = {
	0x55,                                /* push %rbp */
	0x48, 0x89, 0xe5,                    /* mov %rsp, %rbp */
	0x41, 0x57,                          /* push %r15 */
	0x41, 0x56,                          /* push %r14 */
	0x41, 0x55,                          /* push %r13 */
	0x41, 0x54,                          /* push %r12 */
};
static const char target_a_head[] = {
	0x48, 0xb8, 0x00, 0x00, 0x00, 0x00,  /* mov $imm64, %rax */
		    0x00, 0x00, 0x00, 0x00,
	0xff, 0xd0,                          /* call *%rax */
};

static const char expected_b_head[] = {
	0x55,                                /* push %rbp */
	0x48, 0x89, 0xe5,                    /* mov %rsp, %rbp */
	0x41, 0x56,                          /* push %r14 */
	0x53,                                /* push %rbx */
	0x48, 0x89, 0xfb,                    /* mov %rdi, %rbx */
	0x8b, 0x46, 0x1c,                    /* mov 0x1c(%rsi), %eax */
};
static const char target_b_head[] = {
	0x48, 0xb8, 0x00, 0x00, 0x00, 0x00,  /* mov $imm64, %rax */
		    0x00, 0x00, 0x00, 0x00,
	0xff, 0xd0,                          /* call *%rax */
	0x90,                                /* nop */
};


static void patch_tramp_a(int proc_self_mem, char *symbol, void *hook_entry)
{
	char actual_head[sizeof(expected_a_head)];
	char patch_head[sizeof(expected_a_head)];
	uintptr_t symaddr;

	static_assert(sizeof(expected_a_head) == sizeof(target_a_head));
	static_assert(sizeof(hook_entry) == 8);

	symaddr = get_symbol_addr(symbol);

	memcpy(actual_head, (void *)symaddr, sizeof(expected_a_head));
	if (memcmp(actual_head, expected_a_head, sizeof(expected_a_head)))
		error(1, 0, "patch_tramp_a: function text of %s differ from expected", symbol);

	memcpy(patch_head, target_a_head, sizeof(expected_a_head));
	memcpy(patch_head + 2, &hook_entry, sizeof(hook_entry));
	if (pwrite(proc_self_mem, patch_head, sizeof(expected_a_head), symaddr)
			!= sizeof(expected_a_head))
		error(1, errno, "patch_tramp_a: pwrite");
}

static void patch_tramp_b(int proc_self_mem, char *symbol, void *hook_entry)
{
	char actual_head[sizeof(expected_b_head)];
	char patch_head[sizeof(expected_b_head)];
	uintptr_t symaddr;

	static_assert(sizeof(expected_b_head) == sizeof(target_b_head));
	static_assert(sizeof(hook_entry) == 8);

	symaddr = get_symbol_addr(symbol);

	memcpy(actual_head, (void *)symaddr, sizeof(expected_b_head));
	if (memcmp(actual_head, expected_b_head, sizeof(expected_b_head)))
		error(1, 0, "patch_tramp_b: function text of %s differ from expected", symbol);

	memcpy(patch_head, target_b_head, sizeof(expected_b_head));
	memcpy(patch_head + 2, &hook_entry, sizeof(hook_entry));
	if (pwrite(proc_self_mem, patch_head, sizeof(expected_b_head), symaddr)
			!= sizeof(expected_b_head))
		error(1, errno, "patch_tramp_b: pwrite");
}

void patch_tramps(void)
{
	extern char hook_RemoteCommandProcessor_sendToConsole_entry;
	extern char hook_OutputConsoleRenderer_getRenderItems_entry;
	extern char hook_agui_TextBox_textInputHandleKeyEvent_entry;
	extern char hook_agui_TextField_handleKeyboard_entry;

	int proc_self_mem = open("/proc/self/mem", O_RDWR | O_CLOEXEC);
	if (proc_self_mem < 0)
		error(1, errno, "open /proc/pid/mem");

	patch_tramp_a(proc_self_mem, "RemoteCommandProcessor::sendToConsole",
		    &hook_RemoteCommandProcessor_sendToConsole_entry);
	patch_tramp_a(proc_self_mem, "OutputConsoleRenderer::getRenderItems",
		    &hook_OutputConsoleRenderer_getRenderItems_entry);
	patch_tramp_a(proc_self_mem, "agui::TextBox::textInputHandleKeyEvent",
		    &hook_agui_TextBox_textInputHandleKeyEvent_entry);
	patch_tramp_b(proc_self_mem, "agui::TextField::handleKeyboard",
		    &hook_agui_TextField_handleKeyboard_entry);

	close(proc_self_mem);
}
