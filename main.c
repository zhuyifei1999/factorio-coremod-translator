/* SPDX-License-Identifier: GPL-2.0 */

#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "translator.h"

int rpc_channel_sockets[2];

static void std_str_dup(struct std_string *dst, struct std_string *src)
{
	/* NOTE: This does not set capacity */
	char *ptr = malloc(src->length + 1);

	if (!ptr)
		error(1, errno, "std_str_dup: malloc");
	memcpy(ptr, src->ptr, src->length);
	ptr[src->length] = 0;

	dst->ptr = ptr;
	dst->length = src->length;
}

static bool std_str_eq(struct std_string *x, struct std_string *y)
{
	if (x->length != y->length)
		return false;

	return !memcmp(x->ptr, y->ptr, x->length);
}

static void send_rpc(unsigned char cmd, struct std_string *message)
{
	unsigned long len = message->length;
	ssize_t written;
	char *ptr;

	if (write(rpc_channel_sockets[0], &cmd, sizeof(cmd)) != sizeof(cmd))
		error(1, errno, "on_message: write");

	if (write(rpc_channel_sockets[0], &len, sizeof(len)) != sizeof(len))
		error(1, errno, "on_message: write");

	ptr = message->ptr;
	while (len) {
		written = write(rpc_channel_sockets[0], ptr, len);
		if (written < 0)
			error(1, errno, "on_message: write");

		len -= written;
		ptr += written;
	}
}

void on_RemoteCommandProcessor_sendToConsole(void *this, struct std_string *message, struct std_string *tag)
{
	send_rpc(0, message);
}

static void *saved_outputConsole;
static time_t saved_outputConsole_time;

/* https://github.com/reswitched/newlib/blob/master/newlib/libc/string/strnstr.c */
static char *strnstr(const char *haystack, const char *needle, size_t haystack_len)
{
	size_t needle_len = strnlen(needle, haystack_len);

	if (needle_len < haystack_len || !needle[needle_len]) {
		char *x = memmem(haystack, haystack_len, needle, needle_len);
		if (x && !memchr(haystack, 0, x - haystack))
			return x;
	}
	return NULL;
}

static bool compare_replace_message(struct std_string *target, struct std_string *old, struct std_string *new)
{
	char *substring = strnstr(target->ptr, ": ", target->length);
	char *old_allocation, *new_allocation;
	bool free_old = true;
	size_t prefix_len;

	if (!substring)
		return false;

	substring += 2;
	prefix_len = substring - target->ptr;

	if (target->length - prefix_len != old->length)
		return false;
	if (memcmp(target->ptr + prefix_len, old->ptr, old->length))
		return false;

	if (target->ptr == (void *)&target->capacity) {
		free_old = false;
		goto allocate;
	}

	if (prefix_len + new->length + 1 <= target->capacity) {
		memcpy(target->ptr + prefix_len, new->ptr, new->length);
		target->ptr[prefix_len + new->length] = 0;
		target->length = prefix_len + new->length;
		return true;
	}

allocate:
	old_allocation = target->ptr;
	new_allocation = malloc(prefix_len + new->length + 1);
	if (!new_allocation)
		error(1, errno, "compare_replace_message: malloc");

	memcpy(new_allocation, target->ptr, prefix_len);
	memcpy(new_allocation + prefix_len, new->ptr, new->length);
	new_allocation[prefix_len + new->length] = 0;
	target->ptr = new_allocation;
	/* Not sure if capacity includes trailing NUL or not, just gonna be
	 * conservative and assume no */
	target->capacity = target->length = prefix_len + new->length;

	/* Since there's no pointer following, I'm not bothering with
	 * potential UAF */
	if (free_old)
		free(old_allocation);
	return true;
}

void on_OutputConsoleRenderer_getRenderItems(void *result, void *outputConsole)
{
	saved_outputConsole = outputConsole;
	saved_outputConsole_time = time(NULL);
}

void apply_translation_to_console(const char *orig_str, unsigned long orig_len, const char *new_str, unsigned long new_len)
{
	struct list_node {
		struct list_node *next, *prev;
	};

	struct std_string old = {(char *)orig_str, orig_len};
	struct std_string new = {(char *)new_str, new_len};
	struct std_string *key, *localisation_result;
	struct list_node *list, *node;
	void **wrappedtext;
	char *correct;

	/* The OutputConsole gets freed when the game exits to menu.
	 * Do not access if stale. */
	if (!saved_outputConsole || time(NULL) - saved_outputConsole_time > 1)
		return;

	list = saved_outputConsole + 8;

	/* FIXME: This async and can UAF. Maybe append to list and do it in
	 * on_OutputConsoleRenderer_getRenderItems instead */
	for (node = list->next; node != list; node = node->next) {
		key = (void *)node + 0x18;
		localisation_result = (void *)node + 0x58;

#if 0
		/* This causes desync errors */
		if (compare_replace_message(key, &old, &new)) {
			correct = (void *)node + 0x78;
			*correct = 0;
		}
#else
		(void)key;
		(void)correct;
#endif

		if (compare_replace_message(localisation_result, &old, &new)) {
			wrappedtext = (void *)node + 0x98;
			/* There's a bit of memory leak but idk if I should
			 * bother with properly freeing it */
			*wrappedtext = NULL;
		}
	}
}

static struct std_string *(*ptr_agui_TextBox_getText)(void *this);
static void *(*ptr_agui_TextBox_setText)(void *this, struct std_string *text);

static void set_textbox(void *this, struct std_string *src)
{
	struct std_string string;

	std_str_dup(&string, src);
	ptr_agui_TextBox_setText(this, &string);

	if (ptr_agui_TextBox_getText(this)->ptr != string.ptr)
		free(string.ptr);
}

/* TODO: These are concurrently accessed by multiple threads, lock them */
static bool translation_in_progress;
static struct std_string last_src, last_dest, last_explain;

static void textbox_translate(void *textbox)
{
	struct std_string *src;

	if (translation_in_progress)
		return;
	src = ptr_agui_TextBox_getText(textbox);
	if (!src->length)
		return;
	if (std_str_eq(src, &last_explain))
		return;
	if (std_str_eq(src, &last_src) || std_str_eq(src, &last_dest)) {
		set_textbox(textbox, &last_explain);
		return;
	}

	translation_in_progress = true;
	send_rpc(1, src);
}

static void textbox_translate_revert(void *textbox)
{
	struct std_string *src;

	if (translation_in_progress)
		return;

	src = ptr_agui_TextBox_getText(textbox);
	if (!src->length)
		return;
	if (std_str_eq(src, &last_src))
		return;
	if (std_str_eq(src, &last_explain) || std_str_eq(src, &last_dest))
		set_textbox(textbox, &last_src);
}

static void textbox_translate_confirm(void *textbox)
{
	struct std_string *src;

	if (translation_in_progress)
		return;

	src = ptr_agui_TextBox_getText(textbox);
	if (!src->length)
		return;
	if (std_str_eq(src, &last_dest))
		return;
	if (std_str_eq(src, &last_src) || std_str_eq(src, &last_explain))
		set_textbox(textbox, &last_dest);
}


void on_agui_TextBox_textInputHandleKeyEvent(void *this, void *keyEvent)
{
	unsigned int key = *(unsigned int *)(keyEvent + 0x1c);
	bool isControl = *(bool *)(keyEvent + 0x21);

	if (!isControl)
		return;

	switch (key) {
	case 0x2E:  /* agui::KeyEnum::KEY_PERIOD */
		textbox_translate(this);
		break;
	case 0x2C:  /* agui::KeyEnum::KEY_COMMA */
		textbox_translate_revert(this);
		break;
	case 0x2F:  /* agui::KeyEnum::KEY_FORWARDSLASH */
		textbox_translate_confirm(this);
		break;
	}
}

void on_agui_TextField_handleKeyboard(void *this, void *keyEvent)
{
	unsigned int key = *(unsigned int *)(keyEvent + 0x1c);

	if (key == 0xD) /* agui::KeyEnum::KEY_ENTER */
		textbox_translate_confirm(this);
}

void apply_translation_to_textbox(const char *src_str, unsigned long src_len, const char *dest_str, unsigned long dest_len, const char *explain_str, unsigned long explain_len)
{
	struct std_string src = {(char *)src_str, src_len};
	struct std_string dest = {(char *)dest_str, dest_len};
	struct std_string explain = {(char *)explain_str, explain_len};
	struct std_string last_src_copy = last_src;
	struct std_string last_dest_copy = last_dest;
	struct std_string last_explain_copy = last_explain;

	if (!translation_in_progress)
		return;

	std_str_dup(&last_src, &src);
	std_str_dup(&last_dest, &dest);
	std_str_dup(&last_explain, &explain);

	free(last_src_copy.ptr);
	free(last_dest_copy.ptr);
	free(last_explain_copy.ptr);

	translation_in_progress = false;
}

void textbox_translate_error(void)
{
	translation_in_progress = false;
}

__attribute__((constructor))
static void lib_entry(void)
{
	if (strcmp(program_invocation_short_name, "factorio"))
		return;

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, rpc_channel_sockets))
		error(1, errno, "socketpair");

	patch_tramps();

	ptr_agui_TextBox_getText = (void *)get_symbol_addr("agui::TextBox::getText[abi:cxx11]");
	ptr_agui_TextBox_setText = (void *)get_symbol_addr("agui::TextBox::setText");

	start_python();

	printf("factorio translator init done!\n");
}
