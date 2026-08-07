/* SPDX-License-Identifier: GPL-2.0 */

	/* For entry hooks, caller saved registers are already saved by caller;
	 * callee-saved registers are not used yet. The only thing we need to
	 * save is argument registers */
	.macro SAVE_REGS
	push %rdi
	push %rsi
	push %rdx
	push %rcx
	push %r8
	push %r9
	.endm

	.macro RESTORE_REGS
	pop %r9
	pop %r8
	pop %rcx
	pop %rdx
	pop %rsi
	pop %rdi
	.endm

	.text
	.align 16
	.globl tramp_start
	.hidden tramp_start
tramp_start:

.tramp_a_return:
	/* Before returning, these original instructions must be simulated:
	 *
	 * push %rbp          +------------+
	 * mov %rsp, %rbp     | RETURNADDR |
	 * push %r15          +------------+
	 * push %r14          | SAVED R12  |
	 * push %r13          +------------+
	 * push %r12          | SAVED R13  |
	 *                    +------------+
	 *                    | SAVED R14  |
	 *                    +------------+
	 *                    | SAVED R15  |
	 * +------------+     +------------+
	 * | RETURNADDR | --> | SAVED RBP  |
	 * +------------+     +------------+
	 *
	 * Luckily, RAX is a scratch register so stack shuffling is easy */
	pop %rax

	push %rbp
	mov %rsp, %rbp
	push %r15
	push %r14
	push %r13
	push %r12

	push %rax
	ret

.tramp_b_return:
	/* Similar, but these original instructions must be simulated:
	 *
	 * push %rbp
	 * mov %rsp, %rbp
	 * push %r14
	 * push %rbx
	 * mov %rdi, %rbx        +------------+
	 * mov 0x1c(%rsi), %eax  | RETURNADDR |
	 *                       +------------+
	 *                       | SAVED rbx  |
	 *                       +------------+
	 *                       | SAVED r14  |
	 * +------------+        +------------+
	 * | RETURNADDR | -->    | SAVED RBP  |
	 * +------------+        +------------+
	 *
	 * Annoyingly, RAX will be used by this set of instructions. so
	 * do it after the stack is set up
	 */
	pop %rax

	push %rbp
	mov %rsp, %rbp
	push %r14
	push %rbx

	push %rax

	mov %rdi, %rbx
	mov 0x1c(%rsi), %eax 

	ret

	.globl hook_RemoteCommandProcessor_sendToConsole_entry
	.hidden hook_RemoteCommandProcessor_sendToConsole_entry
	.align 16
hook_RemoteCommandProcessor_sendToConsole_entry:
	/* ENTRY: RDI = RemoteCommandProcessor *this, RSI = std::string *message, RDX = std::string *tag */
	SAVE_REGS
	call on_RemoteCommandProcessor_sendToConsole
	RESTORE_REGS
	jmp .tramp_a_return

	.globl hook_OutputConsoleRenderer_getRenderItems_entry
	.hidden hook_OutputConsoleRenderer_getRenderItems_entry
	.align 16
hook_OutputConsoleRenderer_getRenderItems_entry:
	/* ENTRY: RDI = std::vector<OutputConsoleRenderer::RenderItem> *result, RSI = const OutputConsole *outputConsole, ... */
	SAVE_REGS
	call on_OutputConsoleRenderer_getRenderItems
	RESTORE_REGS
	jmp .tramp_a_return

	.globl hook_agui_TextBox_textInputHandleKeyEvent_entry
	.hidden hook_agui_TextBox_textInputHandleKeyEvent_entry
	.align 16
hook_agui_TextBox_textInputHandleKeyEvent_entry:
	/* ENTRY: RDI = agui::TextField *this, RSI = agui::KeyEvent *keyEvent */
	SAVE_REGS
	call on_agui_TextBox_textInputHandleKeyEvent
	RESTORE_REGS
	jmp .tramp_a_return

	.globl hook_agui_TextField_handleKeyboard_entry
	.hidden hook_agui_TextField_handleKeyboard_entry
	.align 16
hook_agui_TextField_handleKeyboard_entry:
	/* ENTRY: RDI = agui::TextField *this, RSI = agui::KeyEvent *keyEvent */
	SAVE_REGS
	call on_agui_TextField_handleKeyboard
	RESTORE_REGS
	jmp .tramp_b_return

.section .note.GNU-stack,"",%progbits
