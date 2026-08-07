/* SPDX-License-Identifier: GPL-2.0 */

#include <stdint.h>

struct std_string {
	char *ptr;
	unsigned long length;
	unsigned long capacity;
};

extern int rpc_channel_sockets[2];

extern uintptr_t get_symbol_addr(char *symbol);
extern void patch_tramps(void);
extern void start_python(void);

extern void on_RemoteCommandProcessor_sendToConsole(void *this, struct std_string *message, struct std_string *tag);
extern void on_OutputConsoleRenderer_getRenderItems(void *result, void *outputConsole);
extern void on_agui_TextBox_textInputHandleKeyEvent(void *this, void *keyEvent);
extern void on_agui_TextField_handleKeyboard(void *this, void *keyEvent);

extern void apply_translation_to_console(const char *orig_str, unsigned long orig_len, const char *new_str, unsigned long new_len);
extern void apply_translation_to_textbox(const char *src_str, unsigned long src_len, const char *dest_str, unsigned long dest_len, const char *explain_str, unsigned long explain_len);
extern void textbox_translate_error(void);
