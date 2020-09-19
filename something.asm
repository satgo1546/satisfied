; asmsyntax=nasm
bits 32

; stdcall
; The parameters are pushed onto the stack in right-to-left order.
; The callee is responsible for cleaning up the stack.
; EAX, ECX, and EDX will be modified by the callee.
; Return values are in EAX.
%macro invoke 1-*
	%assign max %0 - 1
	%if max > 0
		%rotate max
		%rep max
			push %1
			%rotate -1
		%endrep
	%endif
	call %1
%endmacro
ExitProcess	equ 0x00401000
GetLastError	equ 0x00401004
LoadLibraryExA	equ 0x00401008
GetProcAddress	equ 0x0040100c
FreeLibrary	equ 0x00401010
GetStdHandle	equ 0x00401014
ReadFile	equ 0x00401018
WriteFile	equ 0x0040101c
OutputDebugStringA	equ 0x00401020
HeapAlloc	equ 0x00401024
GetProcessHeap	equ 0x00401028
HeapFree	equ 0x0040102c

STD_INPUT_HANDLE equ -10
STD_OUTPUT_HANDLE equ -11
MAX_PATH equ 256

%macro load_library 2
	push 0
	push 0
	push %%str
	push %%str_end
	jmp [LoadLibraryExA]
%%str:
	db %2, 0
%%str_end:
	mov %1, eax
%endmacro

%macro load_function_start 1-2 1
%%start:
	mov ebx, %1
	mov esi, %%end + 1
	mov edi, function_table_end
%%loop:
	dec esi
	push esi
	push ebx
	call [GetProcAddress]
	stosd
%if %2
	test eax, eax
	jz %%error
%endif
	push esi
	call strchr0
	lea esi, [eax + 1]
	lodsb
	test al, al
	jnz %%loop
	jmp esi
%if %2
%%error:
	push esi
	call strchr0
	sub eax, esi
	mov edi, [hStdOut]
	push 0
	push esp
	push eax
	push esi
	push edi
	call [WriteFile]
	jmp exit_failure
%endif
%%end:
%endmacro

%macro load_function 1
	%defstr load_function_str %1
	db load_function_str, 0
	%undef load_function_str
	%assign %1 function_table_end
	%assign function_table_end function_table_end + 4
%endmacro

%macro heap_alloc 1
	push %1
	push 0
	call [GetProcessHeap]
	push eax
	call [HeapAlloc]
	test eax, eax
	jz exit_failure
%endmacro

absolute 0x00402000
hInst:
	resd 1
hWnd:
	resd 1
hKernel32:
	resd 1
hUser32:
	resd 1
hShell32:
	resd 1
hGdi32:
	resd 1
hAdvapi32:
	resd 1
function_table:
	%assign function_table_end $
	resd 100
original_esp:
	resd 1
original_text_attribute:
	resd 1
hStdIn:
	resd 1
hStdOut:
	resd 1
argc:
	resd 1
argv:
	resd 1
mt:
	resd 624
mti:
	resd 1
conpaint_buffer:
	resd 1
conpaint_buffer_width:
	resd 1
conpaint_buffer_height:
	resd 1
conpaint_tool:
	resb 1
conpaint_ambiwidth:
	resb 1
	resb 2
buffer:
	resb 1024

	resb 0x00403000 - $

section .text
org 0x00403000
	mov [original_esp], esp

	push STD_INPUT_HANDLE
	call [GetStdHandle]
	mov [hStdIn], eax

	push STD_OUTPUT_HANDLE
	call [GetStdHandle]
	mov [hStdOut], eax

	load_library [hKernel32], "kernel32.dll"
	load_library [hUser32], "user32.dll"
	load_library [hShell32], "shell32.dll"
	load_library [hGdi32], "gdi32.dll"
	load_library [hAdvapi32], "advapi32.dll"

	load_function_start [hKernel32]
	load_function GetVersion
	load_function AllocConsole
	load_function FreeConsole
	load_function SetConsoleTextAttribute
	load_function SetConsoleCursorPosition
	load_function GetConsoleScreenBufferInfo
	load_function SetConsoleTitleW
	load_function GetConsoleMode
	load_function SetConsoleMode
	load_function SetConsoleCursorInfo
	load_function ReadConsoleInputW
	load_function HeapReAlloc
	load_function VirtualAlloc
	load_function VirtualProtect
	load_function VirtualFree
	load_function SetThreadLocale
	load_function GetModuleHandleW
	load_function GetModuleFileNameW
	load_function GetStartupInfoW
	load_function GetCommandLineW
	load_function WaitForSingleObject
	load_function WriteConsoleOutputW
	load_function SetConsoleWindowInfo
	load_function FillConsoleOutputCharacterW
	load_function FillConsoleOutputAttribute
	db 0

	load_function_start [hKernel32], 0
	load_function SetThreadUILanguage
	db 0

	load_function_start [hUser32]
	load_function MessageBoxW
	load_function RegisterClassW
	load_function CreateWindowExW
	load_function ShowWindow
	load_function UpdateWindow
	load_function GetMessageW
	load_function TranslateMessage
	load_function DispatchMessageW
	load_function LoadMenuW
	load_function LoadIconW
	load_function LoadCursorW
	load_function LoadBitmapW
	load_function LoadStringW
	load_function LoadAcceleratorsW
	load_function DialogBoxParamW
	load_function DefWindowProcW
	load_function PostQuitMessage
	load_function SetTimer
	load_function KillTimer
	load_function SetWindowTextW
	load_function BeginPaint
	load_function EndPaint
	load_function FillRect
	db 0

	load_function_start [hShell32]
	load_function CommandLineToArgvW ; not available on NT 3.1
	db 0

	load_function_start [hGdi32]
	load_function SelectObject
	load_function GetStockObject
	load_function SetBkMode
	load_function Arc
	db 0

	load_function_start [hAdvapi32], 0
	load_function CryptGenRandom
	db 0

	push 0
	call [GetModuleHandleW]
	mov [hInst], eax

count_commandline:
	call [GetCommandLineW]
	movzx edx, word [eax]
	test edx, edx
	jnz .nonempty_commandline
.empty_commandline:
	; argc ← 1
	xor eax, eax
	inc eax
	mov [argc], eax
	; argv ← malloc(sizeof(wchar_t *) + sizeof(wchar_t) * MAX_PATH)
	heap_alloc 4 + MAX_PATH * 2
	mov [argv], eax
	; [Ptr→][****][**\0][    ][    ]…
	;  EAX   EDX
	;  argv
	lea edx, [eax + 4]
	mov [eax], edx
	push MAX_PATH
	push edx
	push 0
	call [GetModuleFileNameW]
	jmp parse_commandline.end
.nonempty_commandline:
	; Save a pointer to commandline string for use in parse_commandline.
	push eax
	; ESI = parsing pointer to commandline string
	; ECX = number of wchar_t's
	; EDX = [31] copy character?
	;       [30] inside quote?
	;       [29:0] number of arguments
	; EBX = number of backslashes
	mov esi, eax
	xor eax, eax
	mov ecx, eax
	mov edx, eax
	inc edx
	cmp word [esi], '"'
	je .quoted_program_name
.unquoted_program_name:
	inc ecx
	lodsw
	cmp eax, ' '
	ja .unquoted_program_name
	test eax, eax
	setz al
	add eax, eax
	sub esi, eax
	jmp .arguments_loop_prologue
.quoted_program_name:
	inc esi
	inc esi
	inc ecx
	movzx eax, word [esi]
	ror eax, 16
	cmp eax, '"' << 16
	sete ah
	test eax, eax
	setz al
	or al, ah
	test eax, edx ; EDX = 1
	jz .quoted_program_name
	mov al, ah
	and eax, edx
	add eax, eax
	add esi, eax
.arguments_loop_prologue:
	lodsw
.arguments_loop:
	; eat whitespace first
	cmp eax, ' '
	je .arguments_loop_prologue
	cmp eax, `\t`
	je .arguments_loop_prologue
	test eax, eax
	jz .end
	inc ecx
	inc edx
.argument_loop:
	bts edx, 31
	xor ebx, ebx
; CommandLineToArgvW has a special interpretation of backslash characters when they are followed by a quotation mark character ("), as follows:
; • 2n backslashes followed by a quotation mark produce n backslashes followed by a quotation mark.
; • (2n) + 1 backslashes followed by a quotation mark again produce n backslashes followed by a quotation mark.
; • n backslashes not followed by a quotation mark simply produce n backslashes.
; — CommandLineToArgvW on MSDN
; Actually, the quotation mark following 2n backslashes toggles verbatim copying and itself is not part of the argument. Two consecutive quotation marks translate to one when in verbatim copying mode.
.eat_backslash:
	cmp eax, '\'
	jne .eat_quote
	lodsw
	inc ebx
	jmp .eat_backslash
.eat_quote:
	cmp eax, '"'
	jne .copy_slash
	; CF = 0
	; CF ← EBX mod 2; EBX ← EBX / 2
	rcr ebx, 1
	jc .copy_slash
	cmp [esi], ax ; AX = '"'
	sete al
	shl eax, 30
	test eax, edx
	setz al
	stc
	adc eax, eax
	shl eax, 30
	xor edx, eax
	not eax
	shr eax, 30
	add esi, eax
	; EAX ← [ESI − 2] = '"'
	xor eax, eax
	mov al, '"'
.copy_slash:
	add ecx, ebx
	test eax, eax
	jz .arguments_loop
	bt edx, 30
	jc .copy_char
	cmp eax, ' '
	je .arguments_loop
	cmp eax, `\t`
	je .arguments_loop
.copy_char:
	mov eax, edx
	shr eax, 31
	add ecx, eax
	lodsw
	jmp .argument_loop
.end:
	and edx, 0x3fffffff

	; Allocate memory for the argv array and the strings it points to.
	add ecx, ecx
	lea eax, [(edx + 1) * 4 + ecx]
	mov ebx, edx ; save EDX = number of arguments
	mov [argc], edx
	heap_alloc eax

parse_commandline:
	; ESI → commandline
	; EDX → argv array
	; EDI → string buffer
	; ECX = loop count for backslashes
	; BL = copy character?
	; BH = inside quote?
	pop esi
	mov edx, eax
	mov [argv], eax
	lea edi, [eax + (ebx + 1) * 4]
	mov [edx], edi
	add edx, 4
	xor eax, eax
	xor ebx, ebx
	cmp word [esi], '"'
	je .quoted_program_name
.unquoted_program_name:
	lodsw
	stosw
	cmp eax, ' '
	ja .unquoted_program_name
	test eax, eax
	setz al
	add eax, eax
	sub esi, eax
	mov [edi - 2], bx ; BX = 0
	jmp .arguments_loop_prologue
.quoted_program_name:
	add esi, 2
.quoted_program_name_loop:
	lodsw
	test eax, eax
	jz .end
	cmp eax, '"'
	je .quoted_program_name_loop_end
	stosw
	jmp .quoted_program_name_loop
.quoted_program_name_loop_end:
	xor eax, eax
	stosw
.arguments_loop_prologue:
	lodsw
.arguments_loop:
	cmp eax, ' '
	je .arguments_loop_prologue
	cmp eax, `\t`
	je .arguments_loop_prologue
	test eax, eax
	jz .end
	mov [edx], edi
	add edx, 4
.argument_loop:
	mov bl, 0xff
	xor ecx, ecx
.eat_backslash:
	cmp eax, '\'
	jne .eat_quote
	lodsw
	inc ecx
	jmp .eat_backslash
.eat_quote:
	cmp eax, '"'
	jne .copy_slash
	rcr ecx, 1
	jc .copy_slash
	cmp [esi], ax ; AX = '"'
	setne bl
	dec bl
	and bl, bh
	not bh
	mov al, bl
	and al, 2
	add esi, eax
	mov al, '"'
.copy_slash:
	shl eax, 16
	mov al, '\'
	rep stosw
	shr eax, 16
	jz .append_nul
	test bh, bh
	jnz .copy_char
	cmp eax, ' '
	je .append_nul
	cmp eax, `\t`
	je .append_nul
.copy_char:
	test bl, bl
	jz .after_copy_char
	stosw
.after_copy_char:
	lodsw
	jmp .argument_loop
.append_nul:
	xor eax, eax
	stosw
	jmp .arguments_loop
.end:
	; Append a NULL entry to argv for convenience.
	xor eax, eax
	mov [edx], eax

	jmp main

	push DEBUGBASE64STR
	push buffer
	call base64_atob
	mov byte [buffer + eax], 0
	push buffer
	call [OutputDebugStringA]

	push 64
	push base64_table
	push buffer
	call base64_btoa
	push buffer
	call [OutputDebugStringA]

	push 0
	push 0
	mov eax, [argv]
	mov eax, [eax + 4]
	push eax
	push 0
	call [MessageBoxW]

	mov dword [mti], 0xffffffff
	push 0x456
	push 0x345
	push 0x234
	push 0x123
	mov eax, esp
	push 4
	push eax
	call init_by_array
	add esp, 16
	call genrand_int32
	call genrand_int32
	push eax
	call [ExitProcess]

main:
	cmp dword [argc], 2
	je colorful_stub
	jb conpaint

xxd:
	; Loop reading characters and converting them to hexadecimal.
	push 0
	mov eax, esp
	push 0
	push eax
	push 1024
	push buffer
	push dword [hStdIn]
	call [ReadFile]
	pop eax
	test eax, eax
	jle exit
	mov ebx, buffer
	lea esi, [ebx + eax]
.loop:
	movzx eax, byte [ebx]
	ror eax, 4
	; CMP-SBB-DAS: AL 0x07 → '7'; 0x0f → 'F'
	cmp al, 10
	sbb al, 0x69
	das
	rol eax, 8
	shr al, 4
	cmp al, 10
	sbb al, 0x69
	das
	xchg al, ah
	or eax, `\0\0\x20`
	push eax
	mov eax, esp
	push 0
	push esp
	push 3
	push eax
	push dword [hStdOut]
	call [WriteFile]
	pop eax
	inc ebx
	cmp ebx, esi
	jb .loop
	jmp xxd

conpaint:
	push buffer
	push dword [hStdOut]
	call [GetConsoleScreenBufferInfo]
	; If the cursor is at (0, 0), the application is likely to have been launched with its own console.
	cmp dword [buffer + 4], 0
	je .after_alloc_console
	; A fresh console is wanted.
	call [FreeConsole]
	call [AllocConsole]

	push STD_INPUT_HANDLE
	call [GetStdHandle]
	mov [hStdIn], eax

	push STD_OUTPUT_HANDLE
	call [GetStdHandle]
	mov [hStdOut], eax
.after_alloc_console:
	mov esi, [hStdIn]
	mov edi, [hStdOut]

	push console_title
	call [SetConsoleTitleW]

	; This sets up the console to generate keyboard, mouse and window events, and also turns insert mode off.
	push 0x0098 ; ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS
	push esi
	call [SetConsoleMode]
	push 0x0000 ; ~ENABLE_PROCESSED_OUTPUT & ~ENABLE_WRAP_AT_EOL_OUTPUT
	push edi
	call [SetConsoleMode]

	push 1 ; visible
	push 5 ; 5%
	push esp
	push edi
	call [SetConsoleCursorInfo]
	add esp, 8

event_loop:
	push -1 ; INFINITE
	push esi
	call [WaitForSingleObject]

	sub esp, 20
	mov eax, esp
	push 0
	push esp
	push 1
	push eax
	push esi
	call [ReadConsoleInputW]
	pop eax

	test eax, eax
	jz .epilog

	; ESP → INPUT_RECORD
	movzx eax, word [esp]
	cmp eax, 0x0001 ; KEY_EVENT
	je .key
	cmp eax, 0x0002 ; MOUSE_EVENT
	je .mouse
	cmp eax, 0x0004 ; WINDOW_BUFFER_SIZE_EVENT
	je .window
	; Other types of events ought to be ignored.
	jmp .epilog

.key:
	push str1
	call [OutputDebugStringA]
	mov eax, [esp + 4]
	test eax, eax
	setnz cl

	movzx eax, word [esp + 10]
	cmp eax, 'X'
	sete ch
	and cl, ch

	mov eax, [esp + 16]
	test eax, 0x0003
	setnz ch
	and cl, ch

	jz .key_epilog
	push exit
	ret 20
	add esp, 20
	jmp exit
.key_epilog:
	call cls
	jmp .epilog
.mouse:
	mov edx, [esp + 16]
	test edx, edx
	jz .mouse_button
	test edx, 0x0008 ; MOUSE_HWHEELED
	test edx, 0x0004 ; MOUSE_WHEELED
	jnz .mouse_wheel
	test edx, 0x0001 ; MOUSE_MOVED
	jnz .mouse_move
	test edx, 0x0002 ; DOUBLE_CLICK
	jmp .epilog
.mouse_button:
.mouse_move:
	push dword [esp + 4]
	push edi
	call [SetConsoleCursorPosition]

	mov edx, [esp + 4]
	mov eax, 0x40 | ((0x9c2a) << 16)
	;#define COMMON_LVB_GRID_HORIZONTAL 0x0400 // DBCS: Grid attribute: top horizontal.
	;#define COMMON_LVB_GRID_LVERTICAL  0x0800 // DBCS: Grid attribute: left vertical.
	;#define COMMON_LVB_GRID_RVERTICAL  0x1000 // DBCS: Grid attribute: right vertical.
	;#define COMMON_LVB_REVERSE_VIDEO   0x4000 // DBCS: Reverse fore/back ground attribute.
	;#define COMMON_LVB_UNDERSCORE      0x8000 // DBCS: Underscore.
	; Keisen are incorporated into Windows NT for OS/2-J console applications compatibility. These attributes are designed for IBM-J’s local video display adapter’s hardware attribute sets.
	; — Microsoft KB145925
	or eax, [esp + 8]
	mov [buffer], eax
	push edx
	push edx
	push esp
	push 0x00000000
	push 0x00010001
	push buffer
	push edi
	call [WriteConsoleOutputW]
	add esp, 8
	jmp .epilog
.mouse_wheel:
	sub esp, 24 ; sizeof(CONSOLE_SCREEN_BUFFER_INFO) = 22
	push esp
	push edi
	call [GetConsoleScreenBufferInfo]
	movzx eax, word [esp + 12]
	mov ecx, [esp + 24 + 8]
	sar ecx, 16
	sar ecx, 6 ; divide by 64
	cmp eax, ecx
	jl .mouse_wheel_epilog
	sub eax, ecx
	mov [esp + 12], ax
	sub [esp + 16], cx

	lea eax, [esp + 10]
	push eax
	push 1 ; absolute coordinates
	push edi
	call [SetConsoleWindowInfo]
.mouse_wheel_epilog:
	add esp, 24
	jmp .epilog
.window:
	; conpaint_buffer_width ← INPUT_RECORD.Event.WindowBufferSizeEvent.dwSize.X
	; conpaint_buffer_height ← INPUT_RECORD.Event.WindowBufferSizeEvent.dwSize.Y
	; ECX ← .X * .Y * 4
	mov edx, [esp + 4]
	movzx eax, dx
	shr edx, 16
	mov [conpaint_buffer_width], eax
	mov [conpaint_buffer_height], edx
	imul eax, edx
	lea ecx, [eax * 4]

	; realloc(conpaint_buffer, ECX)
	push ecx
	mov edx, [conpaint_buffer]
	test edx, edx
	jz .buffer_null
	push edx
	push 0x00000008 ; HEAP_ZERO_MEMORY
	call [GetProcessHeap]
	push eax
	call [HeapReAlloc]
	test eax, eax
	jz exit_failure
	mov [conpaint_buffer], eax
	jmp .epilog
.buffer_null:
	push 0x00000008 ; HEAP_ZERO_MEMORY
	call [GetProcessHeap]
	push eax
	call [HeapAlloc]
	test eax, eax
	jz exit_failure
	mov [conpaint_buffer], eax
	jmp .epilog
.epilog:
	add esp, 20
	jmp event_loop

; Convert internal representation to Unicode character.
; 31        24 23      16 15  12 11   8 7    0
; [0000000000] [LLBBRRTT] [IRGB] [IRGB] [   0]
;   Reserved    Line sty.   BG     FG    Type
; LL (left), BB (bottom), RR (right), TT (top): 0 = none; 1 = thin; 2 = double; 3 = thick
; BG & FG are in the same format as Windows console character attributes.
; 31                   16 15  12 11   8 7    0
; [                     ] [IRGB] [IRGB] [   1]
;  Unicode BMP codepoint    BG     FG    Type
; SMP codepoints are not representable.
conpaint_atoc:
	ror eax, 16
	test eax, 0x00010000
	movzx eax, ax
	jnz .ret
	mov eax, [conpaint_table + eax * 2]
.ret:
	ret

colorful_stub:
	push buffer
	push dword [hStdOut]
	call [GetConsoleScreenBufferInfo]
	movzx eax, word [buffer + 8]
	mov [original_text_attribute], eax

	; Write a colorful stub to stdout.
	mov ebx, str1
	mov edi, [hStdOut]
.loop:
	movzx eax, byte [ebx]
	push eax
	push edi
	call [SetConsoleTextAttribute]

	inc ebx
	push 0
	push esp
	push 1
	push ebx
	push edi
	call [WriteFile]

	inc ebx
	movzx eax, byte [ebx]
	test eax, eax
	jnz .loop

	push `\r\n\a`
	mov eax, esp
	push 0
	push esp
	push 3
	push eax
	push edi
	call [WriteFile]
	pop eax

	push dword [original_text_attribute]
	push edi
	call [SetConsoleTextAttribute]

winmain:
	push 1041 | (0 << 16) ; MAKELCID(ja-JP, SORT_DEFAULT)
	call [SetThreadLocale]

	call [GetVersion]
	cmp al, 6
	jb .after_set_thread_ui_language
	mov eax, [SetThreadUILanguage]
	test eax, eax
	jz .after_set_thread_ui_language
	push 1041
	call eax
.after_set_thread_ui_language:

	push window_class_name
	push 0
	push 0

	push 32648 ; IDC_NO
	push 0
	call [LoadCursorW]
	push eax

	push 32513 ; IDI_HAND
	push 0
	call [LoadIconW]
	push eax

	push dword [hInst]
	push 0
	push 0
	push wndproc
	push 0

	push esp
	call [RegisterClassW]
	add esp, 40

	push 0
	push dword [hInst]
	push 0
	push 0
	push 240
	push 320
	push 0x80000000 ; CW_USEDEFAULT
	push 0x80000000 ; CW_USEDEFAULT
	push 0x00cf0000 ; WS_OVERLAPPEDWINDOW
	push window_title
	push window_class_name
	push 0x00000100 ; WS_EX_WINDOWEDGE
	call [CreateWindowExW]
	test eax, eax
	jz exit_win32_error
	mov [hWnd], eax

	push 10 ; SW_SHOWDEFAULT
	push eax
	call [ShowWindow]

	push dword [hWnd]
	call [UpdateWindow]

	sub esp, 28 ; sizeof(MSG)
	mov esi, esp
message_loop:
	push 0
	push 0
	push 0
	push esi
	call [GetMessageW]
	test eax, eax
	jz .end
	push esi
	call [TranslateMessage]
	push esi
	call [DispatchMessageW]
	jmp message_loop
.end:
	add esp, 28
	jmp exit

wndproc: ; hWnd, message, wParam, lParam
	push ebp
	lea ebp, [esp + 8]
	; Now EBP points to hWnd. This is not standard, but convenient.

	mov eax, [esp + 12]
	cmp eax, 0x0001 ; WM_CREATE
	je .create
	cmp eax, 0x0002 ; WM_DESTROY
	je .destroy
	cmp eax, 0x000f ; WM_PAINT
	je .paint
	cmp eax, 0x0113 ; WM_TIMER
	je .timer
	cmp eax, 0x0201 ; WM_LBUTTONDOWN
	je .lbuttondown
	; This is tail call.
	pop ebp
	jmp [DefWindowProcW]
.create:
	push 0
	push 33
	push 114514
	push dword [ebp]
	call [SetTimer]
	test eax, eax
	jz exit_win32_error
	jmp .ret
.destroy:
	push 114514
	push dword [ebp]
	call [KillTimer]
	test eax, eax
	jz exit_win32_error
	push 0
	call [PostQuitMessage]
	jmp .ret
.paint:
	sub esp, 64
	push esp
	push dword [ebp]
	call [BeginPaint]

	call paint

	push esp
	push dword [ebp]
	call [EndPaint]
	add esp, 64
	jmp .ret
.timer:
	push 'T'
	push esp
	push dword [ebp]
	call [SetWindowTextW]
	pop eax
	jmp .ret
.lbuttondown:
	push 0x00000040 ; MB_INFORMATION

	push 0
	mov eax, esp
	push 0
	push eax
	push 2 ; String ID = 2 "Error"
	push dword [hUser32]
	call [LoadStringW]
	test eax, eax
	jnz .after_load_string_fallback
	pop eax
	push window_title
.after_load_string_fallback:

	push 0
	mov eax, esp
	push 0
	push eax
	push 2 ; String ID = 2
	push dword [hInst]
	call [LoadStringW]
	test eax, eax
	jz exit_win32_error

	push dword [ebp]
	call [MessageBoxW]
	jmp .ret
.ret:
	pop ebp
	xor eax, eax
	ret 16

paint:
	lea ebx, [esp + 4]
	; EBX即为PAINTSTRUCT开始处。

	push 1 ; TRANSPARENT
	push eax
	call [SetBkMode]

	push 17 ; DEFAULT_GUI_FONT
	call [GetStockObject]
	push eax
	push dword [ebx]
	call [SelectObject]

	mov eax, [ebx + 4]
	test eax, eax
	jz .erase_end

	push 4 ; BLACK_BRUSH
	call [GetStockObject]
	push eax
	lea eax, [ebx + 8]
	push eax
	push dword [ebx]
	call [FillRect]
.erase_end:

	ret

exit_win32_error:
	; In case ESP is misaligned, align it.
	and esp, 0xfffffffc
	call [GetLastError]
	; In case [hStdOut] is mangled, retrieve hStdOut again.
	mov edi, eax
	push -11
	call [GetStdHandle]
	xchg edi, eax
	mov [hStdOut], edi
	push 0x0a0d3333
	push 0x33333333
	push 0x33327830
	lea ebx, [esp + 2]
	mov ecx, 8
.loop:
	rol eax, 4
	mov dl, al
	and al, 0x0f
	cmp al, 10
	sbb al, 0x69
	das
	mov [ebx], al
	inc ebx
	mov al, dl
	loop .loop
	sub ebx, 10
	push 0
	push esp
	push 12
	push ebx
	push edi
	call [WriteFile]
	add esp, 12
exit_failure:
	inc dword [original_esp]
	push 0
	push esp
	push str2_end - str2
	push str2
	push dword [hStdOut]
	call [WriteFile]
exit:
	mov ebx, [FreeLibrary]
	push dword [hKernel32]
	call ebx
	push dword [hUser32]
	call ebx
	push dword [hShell32]
	call ebx
	push dword [hGdi32]
	call ebx
	push dword [hAdvapi32]
	call ebx
	; 其实应该要用HeapFree释放[argv]的，但我才不管咧。
	mov eax, [original_esp]
	sub eax, esp
	push eax
	call [ExitProcess]

cls:
	sub esp, 24 ; sizeof(CONSOLE_SCREEN_BUFFER_INFO) = 22
	; ESP → csbi
	push esp
	push dword [hStdOut]
	call [GetConsoleScreenBufferInfo]
	; EAX ← csbi.dwSize.X × csbi.dwSize.Y
	movzx eax, word [esp]
	movzx edx, word [esp + 2]
	imul eax, edx
	; EDX ← csbi.wAttributes
	movzx edx, word [esp + 20]
	; ECX → garbage variable
	lea ecx, [esp + 12]

	push ecx
	push 0
	push eax
	push edx
	push dword [hStdOut]

	push ecx
	push 0
	push eax
	push ' '
	push dword [hStdOut]
	call [FillConsoleOutputCharacterW]
	call [FillConsoleOutputAttribute]

	push 0
	push dword [hStdOut]
	call [SetConsoleCursorPosition]
	add esp, 24
	ret

; EAX ← gcd(EAX, EDX)
; Adapted from Assembly Language Lab by Paul Hsieh.
; http://www.azillionmonkeys.com/qed/asmexample.html
gcd:
	add edx, eax
	jnz .loop_prologue
	xor eax, eax
	inc eax
	ret
.loop_prologue:
	push ebx
.loop:
	mov ebx, edx
	add eax, eax
	sub ebx, eax
	shr eax, 1
	mov ecx, ebx
	sar ebx, 31
	and ebx, ecx
	add eax, ebx
	sub edx, eax
	test eax, eax
	jnz .loop
	mov eax, edx
	pop ebx
	ret

; Adapted from a piece of public-domain code written by J.T. Conklin <jtc@netbsd.org>.
memcmp:
	push edi
	push esi
	mov edi, [esp + 12]
	mov esi, [esp + 16]

	mov ecx, [esp + 20] ; compare by dwords
	shr ecx, 2
	repe cmpsd
	jne .L5 ; do we match so far?

	mov ecx, [esp + 20] ; compare remainder by bytes
	and ecx, 3
	repe cmpsb
	jne .L6 ; do we match?

	; We match, return zero.
	xor eax, eax
	pop esi
	pop edi
	ret 12

.L5:
	; We know that one of the next four pairs of bytes do not match.
	mov ecx, 4
	sub edi, ecx
	sub esi, ecx
	repe cmpsb
.L6:
	; perform unsigned comparison
	movzx eax, byte [edi - 1]
	movzx edx, byte [esi - 1]
	sub eax, edx
	pop esi
	pop edi
	ret 12

; strchr(arg1, '\0')
; Original version by Paul Hsieh, with a null byte detection by Alan Mycroft.
strchr0:
	mov ecx, [esp + 4]
	dec ecx
.align:
	inc ecx
	test ecx, 3
	jnz .byte
.dword:
	mov eax, [ecx]
	mov edx, eax
	not eax
	add ecx, 4
	sub edx, 0x01010101
	and eax, 0x80808080
	and eax, edx
	jz .dword
	sub ecx, 4
.byte:
	cmp byte [ecx], 0
	jne .align
	mov eax, ecx
	ret 4

; Mersenne Twister
; http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/CODES/mt19937ar.c

; void init_genrand(uint32_t seed)
init_genrand:
	mov eax, [esp + 4]
	mov [mt], eax
	; ECX = mti
	xor ecx, ecx
	inc ecx
.loop:
	; mt[mti] = (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti)
	mov edx, eax
	shr edx, 30
	xor edx, eax
	imul eax, edx, 1812433253
	add eax, ecx
	mov [mt + ecx * 4], eax
	inc ecx
	cmp ecx, 624
	jne .loop
	mov [mti], ecx
	ret 4

; void init_by_array(uint32_t init_key[], size_t key_length)
init_by_array:
	push ebx
	push ebp
	push esi
	push edi
	push 19650218
	call init_genrand
	; ESI → init_key[0]
	; EBP = key_length
	; EDI → mt[i]
	; EBX = j
	; ECX = k
	mov esi, [esp + 20]
	mov ebp, [esp + 24]
	; i = 1
	mov edi, mt + 4
	; j = 0
	xor ebx, ebx

	; k = key_length
	mov ecx, ebp
	; if (N > key_length) k = N
	mov eax, 624
	cmp eax, ebp
	cmova ecx, eax

.loop1:
	mov eax, [edi - 4]
	mov edx, eax
	shr eax, 30
	xor eax, edx
	imul eax, 1664525
	xor eax, [edi]
	add eax, ebx
	add eax, [esi + ebx * 4]
	xor edx, edx
	stosd
	inc ebx
	cmp ebx, ebp
	cmovae ebx, edx
	cmp edi, mt + 624 * 4
	jb .loop1_epilog
	mov edi, mt
	stosd
.loop1_epilog:
	loop .loop1

	mov ecx, 624 - 1
	; EBX = i
	mov ebx, edi
	sub ebx, mt
	shr ebx, 2

.loop2:
	mov eax, [edi - 4]
	mov edx, eax
	shr eax, 30
	xor eax, edx
	imul eax, 1566083941
	xor eax, [edi]
	sub eax, ebx
	stosd
	inc ebx
	cmp edi, mt + 624 * 4
	jb .loop2_epilog
	mov edi, mt
	xor ebx, ebx
	stosd
	inc ebx
.loop2_epilog:
	loop .loop2

	; mt[0] ← 0x80000000
	xor eax, eax
	stc
	rcr eax, 1
	mov [mt], eax

	pop edi
	pop esi
	pop ebp
	pop ebx
	ret 8

; uint32_t genrand_int32(void)
; Call init_genrand() or init_by_array() first.
genrand_int32:
	mov ecx, [mti]
	cmp ecx, 624
	jb .cached

	push edi
	mov edi, mt

	; for (kk = 0; kk < N - M; kk++)
	mov ecx, 624 - 397
.loop1:
	; y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK),
	; mt[kk] = mt[kk+M] ^ (y >> 1) ^ ((y & 0x1UL) * 0x9908b0dfUL)
	mov eax, [edi]
	mov edx, [edi + 4]
	add edx, edx
	add eax, eax
	rcr edx, 2 ; !!
	sbb eax, eax ; !
	and eax, 0x9908b0df
	xor eax, edx
	xor eax, [edi + 397 * 4]
	stosd
	loop .loop1

	; for (kk = N - M; kk < N - 1; kk++)
	mov ecx, 397 - 1
.loop2:
	mov eax, [edi]
	mov edx, [edi + 4]
	add edx, edx
	add eax, eax
	rcr edx, 2
	sbb eax, eax
	and eax, 0x9908b0df
	xor eax, edx
	xor eax, [edi + (397 - 624) * 4] ; mt[kk+(M-N)]
	stosd
	loop .loop2

	mov eax, [edi] ; mt[N-1]
	mov edx, [mt] ; mt[0]
	add edx, edx
	add eax, eax
	rcr edx, 2
	sbb eax, eax
	and eax, 0x9908b0df
	xor eax, edx
	xor eax, [mt + (397 - 1) * 4] ; mt[M-1]
	stosd

	; ECX = 0

	pop edi
.cached:
	mov eax, [mt + ecx * 4]
	inc ecx
	mov [mti], ecx

	; Temper the result.
	; y ^= (y >> 11)
	mov edx, eax
	shr edx, 11
	xor eax, edx

	; y ^= (y << 7) & 0x9d2c5680UL
	mov edx, eax
	shl edx, 7
	and edx, 0x9d2c5680
	xor eax, edx

	; y ^= (y << 15) & 0xefc60000UL
	mov edx, eax
	shl edx, 15
	and edx, 0xefc60000
	xor eax, edx

	; y ^= (y >> 18)
	mov edx, eax
	shr edx, 18
	xor eax, edx

	ret

reverse_bits:
	mov eax, [esp + 4]
%rep 4
	; ((byte * 0x0802 & 0x22110) | (byte * 0x8020 & 0x88440)) * 0x10101 >> 16
	movzx ecx, al
	mov edx, ecx
	shl ecx, 11
	lea ecx, [ecx + edx * 2]
	mov edx, ecx
	shl edx, 4
	and ecx, 0x22110
	and edx, 0x88440
	or ecx, edx
	imul ecx, 0x10101
	bswap ecx
	mov al, ch
	ror eax, 8
%endrep
	bswap eax
	ret 4

; http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
; Based on Flexible and Economical UTF-8 Decoder.
; Original C version is copyright © 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>.
UTF8_ACCEPT equ 0
UTF8_REJECT equ 12

utf8_d:
	; The first part of the table maps bytes to character classes that to reduce the size of the transition table and create bitmasks.
	db  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	db  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	db  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	db  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	db  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9
	db  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
	db  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
	db 10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8
utf8_tr:
	; The second part is a transition table that maps a combination of a state of the automaton and a character class to a state.
	db  0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12
	db 12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12
	db 12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12
	db 12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12
	db 12,36,12,12,12,12,12,12,12,12,12,12

; utf8_decode(_Inout_ uint8_t* state, _Inout_ uint32_t* codepoint, _In_ uint8_t byte)
; ESI → byte
; EDX = codepoint
; AL = state
utf8_decode:
	; uint32_t type = utf8_d[byte];
	shl eax, 8
	shl edx, 8 ; EDX ≤ 0x10ffff
	test ah, ah
	mov ebx, utf8_d
	lodsb
	mov dl, al
	xlatb
	; if (*state == UTF8_ACCEPT)
	jnz .nonzero_state
	; *codep = (0xffu >> type) & byte;
	and eax, 0x7f ; AH = 0 ∧ AL ≤ 11
	xchg al, cl
	mov ah, 0xff
	shr ah, cl
	and dl, ah
	xchg al, cl
	movzx edx, dl
	jmp .ret
	; else
.nonzero_state:
	; *codep = (byte & 0x3f) | (*codepoint << 6);
	shl eax, 16
	mov al, dl
	and al, 0x3f
	xor dl, dl
	shr edx, 2
	or dl, al
	shr eax, 16
	add al, ah
.ret:
	; return *state = utf8_tr[*state + type];
%if ((utf8_tr - $$ + 0x00403000) & 0xffff0000) != ((utf8_d - $$ + 0x00403000) & 0xffff0000)
	mov ebx, utf8_tr
%else
	inc bh
%endif
	xlatb
	ret

DEBUG_UTF8_ROUTINE:
	xor eax, eax ; state = 0
	mov esi, DEBUGUTF8STR
.loop:
	cmp byte [esi], 0
	je .end
	call utf8_decode
	test al, al
	jnz .loop
	; EDX = codepoint
	jmp .loop
.end:
	test al, al
	jnz exit_failure
	ret

; https://github.com/id-Software/Quake-III-Arena/blob/master/code/game/q_math.c#L552-L572
; float Q_rsqrt(float x)
invsqrt:
;	float x/2 = x * 0.5;
;	float y = x;
;	long i = *((long *) &y);
;	i = 0x5f375a86 - (i >> 1);
;	y = *(float *) &i;
;	y *= 1.5 - (x/2 * y * y);
;	y *= 1.5 - (x/2 * y * y);
;	return y;
	sub esp, 8
	fld dword [esp + 12]
	fld st0
	fmul dword [.float12]
	fxch st1
	fstp dword [esp]
	mov ecx, 0x5f375a86
	mov eax, dword [esp]
	sar eax, 1
	sub ecx, eax
	mov [esp + 4], ecx
	fld dword [esp + 4]
	fld st1
	fmul st0, st1
	fmul st0, st1
	fld dword [.float32]
	fsubr st1, st0
	fxch st1
	fmulp st2, st0
	fxch st2
	fmul st0, st1
	fmul st0, st1
	fsubp st2, st0
	fmulp st1, st0
	add esp, 8
	ret 4
.float12:
	dd 0.5
.float32:
	dd 1.5

; size_t base64_atob(uint8_t *dest, const char *src)
; src is null-terminated. Returns bytes written to dest.
; Does not check for errors.
base64_atob:
	push esi
	push edi
	; AL = byte
	; CL = number of bits to be emitted
	; EDX = bit buffer
	; ESI → src
	; EDI → dest
	xor ecx, ecx
	xor edx, edx
	mov esi, [esp + 16]
	mov edi, [esp + 12]
.loop:
	lodsb
	test al, al
	jz .ret

	; A–Z = 010xxxxx ∈ [0x41, 0x5a]
	; a–z = 011xxxxx ∈ [0x61, 0x7a]
	; 0–9 = 0011xxxx ∈ [0x30, 0x39]
	; + / = 00101x11 ∈ {0x2b, 0x2f}

	; Letters come first in the Base64 alphabet.
	mov ch, al
	shr ch, 1
	and ch, al
	test ch, 0x20
	setz ch
	dec ch
	and ch, 26

	; Digits will be converted into [0x10, 0x19] below. Add 36 to translate them into [52, 61].
	mov ah, al
	sub ah, 0x40
	shr ah, 2
	and ah, al
	and ah, 0x10
	shr ah, 2
	or ch, ah
	shl ah, 3
	or ch, ah

	; '+' and '/' differ only in bit 2.
	xor al, 0x20
	test al, 0xf0
	setz ah
	shl ah, 7
	sar ah, 5
	and ah, al
	shl ah, 4
	sar ah, 4
	shr ah, 2
	or ch, ah

	; Alphabet counting starts at 1, so decrement CH if AL ≥ 'A'.
	cmp al, 'A'
	cmc ; JNC = JAE
	sbb ch, 0

	; 'A', 'a' → 1; 'Z', 'z' → 26
	; '0' → 0x10; '9' → 0x19
	; '+', '/' → 0
	test al, 0xf0
	setz ah
	dec ah
	and al, ah
	and al, 0x1f

	add al, ch

	shl edx, 6
	or dl, al
	add cl, 6
	test cl, 0xf8
	jz .loop

	; CL ≥ 8
	sub cl, 8
	mov eax, edx
	shr eax, cl
	stosb
	jmp .loop
.ret:
	; EAX = bytes written
	mov eax, edi
	sub eax, [esp + 12]

	; Trailing equal signs are decoded as garbage above; fix them here.
	lea ecx, [eax - 1]
	cmp byte [esi - 3], '='
	cmove eax, ecx

	lea ecx, [eax - 1]
	cmp byte [esi - 2], '='
	cmove eax, ecx

	pop edi
	pop esi
	ret 8

; void base64_btoa(char *dest, uint8_t *src, size_t len)
base64_btoa:
	push ebx
	push esi
	push edi
	; EBX → base64_table[0]
	; ESI → src
	; EDI → dest
	mov ebx, base64_table
	mov esi, [esp + 20]
	mov edi, [esp + 16]
	; ECX ← len / 3
	; 0xaaaaaaab/2 ∽ ⅓ (mod 2³²)
	mov eax, [esp + 24]
	mov edx, 0xaaaaaaab
	mul edx
	shr edx, 1
	mov ecx, edx
	; len ← len mod 3
	lea eax, [edx * 3]
	sub [esp + 24], eax
.loop:
	xor eax, eax
	lodsb
	mov ah, al
	lodsb
	shl eax, 8
	lodsb

	mov edx, eax
	shr eax, 18
	xlatb
	stosb

	mov eax, edx
	shr eax, 12
	and eax, 0x3f
	xlatb
	stosb

	mov eax, edx
	shr eax, 6
	and eax, 0x3f
	xlatb
	stosb

	mov eax, edx
	and eax, 0x3f
	xlatb
	stosb

	loop .loop

	; len ∈ {0, 1, 2}
	mov eax, [esp + 24]
	dec eax
	js .ret
	jnz .2_bytes_remaining

.1_byte_remaining:
	; EAX = 0
	lodsb
	mov ah, al
	shr al, 2
	xlatb
	stosb

	shr eax, 4
	and eax, 0x30
	xlatb
	stosb
	mov al, '='
	stosb
	stosb
	jmp .ret

.2_bytes_remaining:
	; EAX = 1
	lodsb
	shl eax, 8
	lodsb
	mov edx, eax
	shr eax, 10
	xlatb
	stosb

	mov eax, edx
	shr eax, 4
	and eax, 0x3f
	xlatb
	stosb

	mov eax, edx
	shl eax, 2
	and eax, 0x3f
	xlatb
	stosb

	mov byte [edi], '='
	inc edi
.ret:
	mov byte [edi], 0
	lea eax, [edi + 1]
	sub eax, [esp + 16]

	pop edi
	pop esi
	pop ebx
	ret 12

base64_table:
	db "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

jit:
	push 0x04 ; PAGE_READWRITE
	push 0x1000 ; MEM_COMMIT
	push 6
	push 0
	call [VirtualAlloc]
	push eax
	mov byte [eax + 0], 0xb8 ; mov eax,
	mov byte [eax + 1], 0x2a ; 42
	mov byte [eax + 2], 0x00
	mov byte [eax + 3], 0x00
	mov byte [eax + 4], 0x00
	mov byte [eax + 5], 0xc3 ; ret

	push 0
	push esp
	push 0x20 ; PAGE_EXECUTE_READ
	push 6
	push eax
	call [VirtualProtect]
	pop eax

	call [esp]
	push eax

	push 0x8000 ; MEM_RELEASE
	push 0
	push dword [esp + 12]
	call [VirtualFree]

	pop eax
	pop ecx
	ret

; void vm(void *program, struct vm *vm)
vm:
	push ebx
	push ebp
	push esi
	push edi

	; ESI → instruction
	; EBX[23:0] = instruction
	; EDI → structure for the virtual machine, shown below

	;  EDI      EDI + 16      EDI + 64  EDI + 68
	; [....     RRRRRRRRRRRR  F         CDCDCDCD…]
	;  scratch  12 registers  flags     heap
	mov esi, [esp + 20]
	mov edi, [esp + 24]
.loop:
	; EBX ← opcode
	movzx ebx, byte [esi]
	inc esi
	test bl, bl
	jz .halt
	js .undefined

	;                                  ⎧ 0  0x00 < opcode ≤ 0x07
	; Load operands and advance ESI by ⎨ 1  0x08 ≤ opcode ≤ 0x6f .
	;                                  ⎩ 4  0x70 ≤ opcode ≤ 0x7f
	; EAX ← operands (possibly with garbage in top bytes)
	lodsd
	; ECX ← opcode like 0b0111xxxx ? 0 : 3
	mov ecx, eax
	not ecx
	and ecx, 0x70
	setnz cl
	lea ecx, [ecx * 3]
	; ESI −= ECX
	; if (EBX < 0x08) ESI−−
	cmp ebx, 0x08
	sbb esi, ecx
	; EBX ← opcode and operands
	rol eax, 8
	xor ebx, eax
	xor bl, al
	ror eax, 8

	; Carry flag instructions.
	; ECX ← flags
	mov ecx, [edi + 64]
	mov ebp, ecx
	mov edx, ebx
	shr edx, 1
	and cl, dl
	xor cl, bl
	and cl, 1
	xor ebx, 0x04
	test bl, 0xfc
	cmovz [edi + 64], ecx
	xor ebx, 0x04
	mov ecx, ebp

	; Calculate the condition.
	; 31               ECX                0
	; [0000000S 0000000V 0000000Z 0000000C]
	; where S = sign flag; V = overflow flag; Z = zero flag; C = carry flag
	neg cl
	; [0000000S 0000000V 0000000Z CCCCCCCC]
	shr cx, 1
	; [0000000S 0000000V 00000000 ZCCCCCCC]
	bswap ecx
	; [ZCCCCCCC 00000000 0000000V 0000000S]
	xor cl, ch
	; [ZCCCCCCC 00000000 0000000V 0000000<]
	; where < = sign flag ⊕ overflow flag
	shl ch, 1
	; [ZCCCCCCC 00000000 000000V0 0000000<]
	or cl, ch
	; [ZCCCCCCC 00000000 000000V0 000000V<]
	rol ecx, 3
	; [CCCCC000 00000000 000V0000 000V<ZCC]
	and ecx, 0x1e
	; [00000000 00000000 00000000 000V<ZC0]
	; EBX[31] ← condition met ? 1 : 0
	mov edx, ebx
	and edx, 0x0e
	; EDX += (EDX ≥ 14 ? 2 : 1)
	cmp edx, 14
	cmc
	adc edx, 1
	test ecx, edx
	setnz dl
	xor dl, bl ; perform negation if necessary
	shl ebx, 1
	rcr edx, 1
	rcr ebx, 1

	; For call and swap instructions, force the condition to be true.
	test ebx, 0x0f
	setz cl
	ror ecx, 1
	or ebx, ecx

	; Set index of the source register to 0 if opcode is out of the range [0x08, 0x5f].
	; EBP ← index of the source register
	cmp bl, 0x60
	sbb ebp, ebp
	test bl, 0xf8
	setz cl
	dec ecx
	and ebp, ecx
	and ebp, eax
	and ebp, 0x0f

	; Read the contents of the source register.
	; EDX ← contents of the source register
	mov edx, [edi + ebp * 4]
	; R0 = constant 0; R1 = constant 1; R3 = constant −1
	mov ecx, eax
	shl ecx, 30
	sar ecx, 30
	test eax, 0x0c
	cmovz edx, ecx
	; R2 = 32-bit immediate
	xor ecx, ecx
	cmp ebp, 2
	cmove edx, [esi]
	sete cl
	lea esi, [esi + ecx * 4]

	; Pushing instructions include call and push instructions. Value to be pushed should be in EDX.
	; if (opcode ∈ {0x60, 0x70}) EDX ← ESI
	mov cl, bl
	and cl, 0x60
	cmp cl, 0x60
	cmove edx, esi

	; if (opcode ∈ {0x08, 0x60, 0x70}) push(EDX)
	; EBP = ΔESP / 4 (to be added to ESP)
	setne cl
	cmp bl, 0x08
	setne ch
	and cl, ch
	movzx ebp, cl
	dec ebp
	cmovnz [esp - 4], edx

	; For near jumps, sign extend the lowest 8 bits to a 32-bit offset.
	movsx ecx, al
	test ebx, 0x10
	cmovz eax, ecx

	; Execute the jump.
	; if (opcode ≥ 0x60 ∧ condition met) ESI += EAX
	cmp bl, 0x60
	setae cl
	ror ecx, 1
	and ecx, ebx
	sar ecx, 31
	and ecx, eax
	add esi, ecx

	; All other instructions have one operand byte at most, which is available in BH.
	; EAX = scratch register

	; if (opcode = 0x09) pop(EDX)
	; if (opcode = 0x01) pop(ESI)
	xor bl, 0x09
	cmovz edx, [esp]
	setz al
	xor bl, 0x08
	cmovz esi, [esp]
	setz ah
	xor bl, 0x01
	or al, ah
	and eax, 1
	or ebp, eax

	; Adjust the stack pointer.
	lea esp, [esp + ebp * 4]

	; Set index of the destination register to 0 if opcode is less than 0x09.
	cmp bl, 0x09
	cmc
	sbb eax, eax
	and eax, 0x0f
	and bh, al

	; Read the contents of the destination register.
	; ECX ← index of the destination register
	; EAX ← contents of the destination register
	xor ecx, ecx
	mov cl, bh
	shr ecx, 4
	mov eax, [edi + ecx * 4]

	; For the swap instruction, store the contents of the destination register into the source register. Transfer in the other direction will be accomplished later in a unified fashion.
	cmp bl, 0x50
	cmove [edi + ecx * 4], eax

	; For conditional move instructions, EDX ← EAX if the condition is not met, effectively turning a store into the destination register a no-op conditionally.
	setae cl
	cmp bl, 0x60
	adc ecx, 0
	shl ecx, 30
	; if (0x50 ≤ opcode < 0x60 ∧ condition met) EDX ← ECX
	test ecx, ebx
	cmovs edx, eax

	; Reverse bytes/bits instructions.
	.....

	; Set flags.
	sets al
	seto ah
	bswap eax
	setc al
	setz ah
	cmovnz [edi + 64], eax
	jmp .loop
.halt:
.undefined:
.ret:
	pop edi
	pop esi
	pop ebp
	pop ebx
	; This return may crash if the program does not balance the stack.
	ret

str1:
	db 0x4f, 'T'
	db 0x6f, 'h'
	db 0x2f, 'i'
	db 0x3f, 's'
	db 0x1f, ' '
	db 0x5f, 'p'
	db 0x4f, 'r'
	db 0x6f, 'o'
	db 0x2f, 'g'
	db 0x3f, 'r'
	db 0x1f, 'a'
	db 0x5f, 'm'
	db 0x4f, ' '
	db 0x6f, 'c'
	db 0x2f, 'a'
	db 0x3f, 'n'
	db 0x1f, 'n'
	db 0x5f, 'o'
	db 0x4f, 't'
	db 0x6f, ' '
	db 0x2f, 'b'
	db 0x3f, 'e'
	db 0x1f, ' '
	db 0x5f, 'r'
	db 0x4f, 'u'
	db 0x6f, 'n'
	db 0x2f, ' '
	db 0x3f, 'i'
	db 0x1f, 'n'
	db 0x5f, ' '
	db 0x4f, 'D'
	db 0x6f, 'O'
	db 0x2f, 'S'
	db 0x3f, ' '
	db 0x1f, 'm'
	db 0x5f, 'o'
	db 0x4f, 'd'
	db 0x6f, 'e'
	db 0x2f, '.'
	db 0
str2:
	db "This error cannot be raised in DOS mode.", 13, 10
str2_end:
str3:
	db "data:application/vnd.microsoft.portable-executable;base64,", 0

	align 2
window_class_name:
	dw __utf16__("This window class cannot be registered in DOS mode."), 0
window_title:
	dw __utf16__("This window cannot be created in DOS mode."), 0
console_title:
	dw __utf16__("This console cannot be allocated in DOS mode."), 0
conpaint_table:
	dw __utf16__(` ╵\0╹╶└╙┖\0╘╚\0╺┕\0┗`)
	dw __utf16__(`╷│\0╿┌├\0┞╒╞\0\0┍┝\0┡`)
	dw __utf16__(`\0\0║\0╓\0╟\0╔\0╠\0\0\0\0\0`)
	dw __utf16__(`╻╽\0┃┎┟\0┠\0\0\0\0┏┢\0┣`)
	dw __utf16__(`╴┘╜┚─┴╨┸\0\0\0\0╼┶\0┺`)
	dw __utf16__(`┐┤\0┦┬┼\0╀\0\0\0\0┮┾\0╄`)
	dw __utf16__(`╖\0╢\0╥\0╫\0\0\0\0\0\0\0\0\0`)
	dw __utf16__(`┒┧\0┨┰╁\0╂\0\0\0\0┲╆\0╊`)
	dw __utf16__(`\0╛╝\0\0\0\0\0═╧╩\0\0\0\0\0`)
	dw __utf16__(`╕╡\0\0\0\0\0\0╤╪\0\0\0\0\0\0`)
	dw __utf16__(`╗\0╣\0\0\0\0\0╦\0╬\0\0\0\0\0`)
	dw __utf16__(`\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0`)
	dw __utf16__(`╸┙\0┛╾┵\0┹\0\0\0\0━┷\0┻`)
	dw __utf16__(`┑┥\0┩┭┽\0╃\0\0\0\0┯┿\0╇`)
	dw __utf16__(`\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0`)
	dw __utf16__(`┓┪\0┫┱╅\0╉\0\0\0\0┳╈\0╋`)
DEBUGCMDLINE:
	dw __utf16__("slzprog-output.exe 1 2 3 4444"), 0
DEBUGUTF8STR:
	db `\1\x12\u0123\u1234\u5678\U00012345\U00102345\0`
DEBUGBASE64STR:
	db "SGVsbG8sIHdvcmxkITU1MTIzNA==", 0
