package main

import (
	"fmt"
	"io"
	"math"
	"os"
	"sort"
	"strings"
)

const (
	TIDENT = iota
	TKEYWORD
	TNUMBER
	TCHAR
	TSTRING
	TEOF
	TINVALID
	// Only in CPP
	MIN_CPP_TOKEN
	TNEWLINE
	TSPACE
	TMACRO_PARAM
)

const (
	AST_LITERAL = 256 + iota
	AST_LVAR
	AST_GVAR
	AST_TYPEDEF
	AST_FUNCALL
	AST_FUNCPTR_CALL
	AST_FUNCDESG
	AST_FUNC
	AST_DECL
	AST_INIT
	AST_CONV
	AST_ADDR
	AST_DEREF
	AST_IF
	AST_TERNARY
	AST_DEFAULT
	AST_RETURN
	AST_COMPOUND_STMT
	AST_STRUCT_REF
	AST_GOTO
	AST_COMPUTED_GOTO
	AST_LABEL
	OP_SIZEOF
	OP_CAST
	OP_SHR
	OP_SHL
	OP_A_SHR
	OP_A_SHL
	OP_PRE_INC
	OP_PRE_DEC
	OP_POST_INC
	OP_POST_DEC
	OP_LABEL_ADDR

	OP_ARROW
	OP_A_ADD
	OP_A_AND
	OP_A_DIV
	OP_A_MOD
	OP_A_MUL
	OP_A_OR
	OP_A_SAL
	OP_A_SAR
	OP_A_SUB
	OP_A_XOR
	OP_DEC
	OP_EQ
	OP_GE
	OP_INC
	OP_LE
	OP_LOGAND
	OP_LOGOR
	OP_NE
	OP_SAL
	OP_SAR

	KALIGNAS
	KALIGNOF
	KAUTO
	KBOOL
	KBREAK
	KCASE
	KCHAR
	KCOMPLEX
	KCONST
	KCONTINUE
	KDEFAULT
	KDO
	KDOUBLE
	KELSE
	KENUM
	KEXTERN
	KFLOAT
	KFOR
	KGENERIC
	KGOTO
	KIF
	KIMAGINARY
	KINLINE
	KINT
	KLONG
	KNORETURN
	KREGISTER
	KRESTRICT
	KRETURN
	KHASHHASH
	KSHORT
	KSIGNED
	KSIZEOF
	KSTATIC
	KSTATIC_ASSERT
	KSTRUCT
	KSWITCH
	KELLIPSIS
	KTYPEDEF
	KTYPEOF
	KUNION
	KUNSIGNED
	KVOID
	KVOLATILE
	KWHILE
)

const (
	KIND_VOID = iota
	KIND_BOOL
	KIND_CHAR
	KIND_SHORT
	KIND_INT
	KIND_FLOAT
	KIND_DOUBLE
	KIND_LDOUBLE
	KIND_ARRAY
	KIND_ENUM
	KIND_PTR
	KIND_STRUCT
	KIND_FUNC
	// used only in parser
	KIND_STUB
)

var type_void = &Type{kind: KIND_VOID, size: 0, align: 0}
var type_bool = &Type{kind: KIND_BOOL, size: 1, align: 1, usig: true}
var type_char = &Type{kind: KIND_CHAR, size: 1, align: 1, usig: false}
var type_short = &Type{kind: KIND_SHORT, size: 2, align: 2, usig: false}
var type_int = &Type{kind: KIND_INT, size: 4, align: 4, usig: false}
var type_uchar = &Type{kind: KIND_CHAR, size: 1, align: 1, usig: true}
var type_ushort = &Type{kind: KIND_SHORT, size: 2, align: 2, usig: true}
var type_uint = &Type{kind: KIND_INT, size: 4, align: 4, usig: true}
var type_float = &Type{kind: KIND_FLOAT, size: 4, align: 4, usig: false}
var type_double = &Type{kind: KIND_DOUBLE, size: 8, align: 8, usig: false}
var type_ldouble = &Type{kind: KIND_LDOUBLE, size: 8, align: 8, usig: false}
var type_enum = &Type{kind: KIND_ENUM, size: 4, align: 4, usig: false}

type Type struct {
	kind  int
	size  int
	align int
	usig  bool // true if unsigned
	// pointer or array
	ptr *Type
	// array length
	len int
	// struct
	fields    map[any]*Type
	offset    int
	is_struct bool
	// function
	rettype *Type
	params  []*Type
	hasva   bool
}

type Node struct {
	kind int
	ty   *Type

	// Char, int, or long
	ival int64

	// Float or double
	fval   float64
	flabel string

	// String
	sval   string
	slabel string

	// Local/global variable
	varname string
	// local
	loff     int
	lvarinit []*Node
	// global
	glabel string

	// Binary operator
	left  *Node
	right *Node

	// Unary operator
	operand *Node

	// Function call or function declaration
	fname string
	// Function call
	args  []*Node
	ftype *Type
	// Function pointer or function designator
	fptr *Node
	// Function declaration
	params    []*Node
	localvars []*Node
	body      *Node

	// Declaration
	declvar  *Node
	declinit []*Node

	// Initializer
	initval *Node
	initoff int
	totype  *Type

	// If statement or ternary operator
	cond *Node
	then *Node
	els  *Node

	// Goto and label
	label    string
	newlabel string

	// Return statement
	retval *Node

	// Compound statement
	stmts []*Node

	// Struct reference
	struc *Node
	field string
}

func error(format string, a ...any) {
	fmt.Fprintf(os.Stderr, format, a...)
	os.Exit(1)
}

func uop_to_string(b io.Writer, op string, node *Node) {
	fmt.Fprintf(b, "(%s ", op)
	do_node2s(b, node.operand)
	fmt.Fprintf(b, ")")
}

func binop_to_string(b io.Writer, op string, node *Node) {
	fmt.Fprintf(b, "(%s ", op)
	do_node2s(b, node.left)
	fmt.Fprintf(b, " ")
	do_node2s(b, node.right)
	fmt.Fprintf(b, ")")
}

func a2s_declinit(b io.Writer, initlist []*Node) {
	for i, init := range initlist {
		if i > 0 {
			fmt.Fprintf(b, " ")
		}
		do_node2s(b, init)
	}
}

func do_ty2s(dict map[*Type]bool, ty *Type) string {
	if ty == nil {
		return "(nil)"
	}
	switch ty.kind {
	case KIND_VOID:
		return "void"
	case KIND_BOOL:
		return "_Bool"
	case KIND_CHAR:
		if ty.usig {
			return "unsigned char"
		}
		return "signed char"
	case KIND_SHORT:
		if ty.usig {
			return "unsigned short"
		}
		return "short"
	case KIND_INT:
		if ty.usig {
			return "unsigned"
		}
		return "int"
	case KIND_FLOAT:
		return "float"
	case KIND_DOUBLE:
		return "double"
	case KIND_LDOUBLE:
		return "long double"
	case KIND_PTR:
		return fmt.Sprintf("*%s", do_ty2s(dict, ty.ptr))
	case KIND_ARRAY:
		return fmt.Sprintf("[%d]%s", ty.len, do_ty2s(dict, ty.ptr))
	case KIND_STRUCT:
		kind := "union"
		if ty.is_struct {
			kind = "struct"
		}
		if dict[ty] {
			return fmt.Sprintf("(%s)", kind)
		}
		dict[ty] = true
		if len(ty.fields) != 0 {
			var b strings.Builder
			b.WriteRune('(')
			b.WriteString(kind)
			for _, fieldtype := range ty.fields {
				b.WriteString(fmt.Sprintf(" (%s)", do_ty2s(dict, fieldtype)))
			}
			b.WriteRune(')')
			return b.String()
		}
	case KIND_FUNC:
		var b strings.Builder
		b.WriteRune('(')
		for i, t := range ty.params {
			if i > 0 {
				b.WriteRune(',')
			}
			b.WriteString(do_ty2s(dict, t))
		}
		b.WriteString(")=>")
		b.WriteString(do_ty2s(dict, ty.rettype))
		return b.String()
	default:
		return fmt.Sprintf("(Unknown ty: %d)", ty.kind)
	}
	panic("ty2s")
}

func ty2s(ty *Type) string {
	return do_ty2s(nil, ty)
}

func do_node2s(b io.Writer, node *Node) {
	if node == nil {
		fmt.Fprintf(b, "(nil)")
		return
	}
	switch node.kind {
	case AST_LITERAL:
		switch node.ty.kind {
		case KIND_CHAR:
			if node.ival == '\n' {
				fmt.Fprintf(b, "'\n'")
			} else if node.ival == '\\' {
				fmt.Fprintf(b, "'\\\\'")
			} else if node.ival == 0 {
				fmt.Fprintf(b, "'\\0'")
			} else {
				fmt.Fprintf(b, "'%c'", node.ival)
			}
		case KIND_INT:
			fmt.Fprintf(b, "%d", node.ival)
		case KIND_FLOAT, KIND_DOUBLE, KIND_LDOUBLE:
			fmt.Fprintf(b, "%f", node.fval)
		case KIND_ARRAY:
			fmt.Fprintf(b, "%q", node.sval)
		default:
			error("internal error")
		}
	case AST_LABEL:
		fmt.Fprintf(b, "%s:", node.label)
	case AST_LVAR:
		fmt.Fprintf(b, "lv=%s", node.varname)
		if node.lvarinit != nil {
			fmt.Fprintf(b, "(")
			a2s_declinit(b, node.lvarinit)
			fmt.Fprintf(b, ")")
		}
	case AST_GVAR:
		fmt.Fprintf(b, "gv=%s", node.varname)
	case AST_FUNCALL, AST_FUNCPTR_CALL:
		fmt.Fprintf(b, "(%s)", ty2s(node.ty))
		if node.kind == AST_FUNCALL {
			fmt.Fprintf(b, node.fname)
		} else {
			do_node2s(b, node)
		}
		fmt.Fprintf(b, "(")
		for i, arg := range node.args {
			if i > 0 {
				fmt.Fprintf(b, ",")
			}
			do_node2s(b, arg)
		}
		fmt.Fprintf(b, ")")
	case AST_FUNCDESG:
		fmt.Fprintf(b, "(funcdesg %s)", node.fname)
	case AST_FUNC:
		fmt.Fprintf(b, "(%s)%s(", ty2s(node.ty), node.fname)
		for i, param := range node.params {
			if i > 0 {
				fmt.Fprintf(b, ",")
			}
			fmt.Fprintf(b, "%s ", ty2s(param.ty))
			do_node2s(b, param)
		}
		fmt.Fprintf(b, ")")
		do_node2s(b, node.body)
	case AST_GOTO:
		fmt.Fprintf(b, "goto(%s)", node.label)
	case AST_DECL:
		fmt.Fprintf(b, "(decl %s %s", ty2s(node.declvar.ty), node.declvar.varname)
		if len(node.declinit) != 0 {
			fmt.Fprintf(b, " ")
			a2s_declinit(b, node.declinit)
		}
		fmt.Fprintf(b, ")")
	case AST_INIT:
		do_node2s(b, node.initval)
		fmt.Fprintf(b, "@%d:%s", node.initoff, ty2s(node.totype))
	case AST_CONV:
		fmt.Fprintf(b, "(conv ")
		do_node2s(b, node.operand)
		fmt.Fprintf(b, "=>%s)", ty2s(node.ty))
	case AST_IF:
		fmt.Fprintf(b, "(if ")
		do_node2s(b, node.cond)
		fmt.Fprintf(b, " ")
		do_node2s(b, node.then)
		if node.els != nil {
			fmt.Fprintf(b, " ")
			do_node2s(b, node.els)
		}
		fmt.Fprintf(b, ")")
	case AST_TERNARY:
		fmt.Fprintf(b, "(? ")
		do_node2s(b, node.cond)
		fmt.Fprintf(b, " ")
		do_node2s(b, node.then)
		fmt.Fprintf(b, " ")
		do_node2s(b, node.els)
		fmt.Fprintf(b, ")")
	case AST_RETURN:
		fmt.Fprintf(b, "(return ")
		do_node2s(b, node.retval)
		fmt.Fprintf(b, ")")
	case AST_COMPOUND_STMT:
		fmt.Fprintf(b, "{")
		for _, stmt := range node.stmts {
			do_node2s(b, stmt)
			fmt.Fprintf(b, ";")
		}
		fmt.Fprintf(b, "}")
	case AST_STRUCT_REF:
		do_node2s(b, node.struc)
		fmt.Fprintf(b, ".")
		fmt.Fprintf(b, node.field)
	case AST_ADDR:
		uop_to_string(b, "addr", node)
	case AST_DEREF:
		uop_to_string(b, "deref", node)
	case OP_SAL:
		binop_to_string(b, "<<", node)
	case OP_SAR, OP_SHR:
		binop_to_string(b, ">>", node)
	case OP_GE:
		binop_to_string(b, ">=", node)
	case OP_LE:
		binop_to_string(b, "<=", node)
	case OP_NE:
		binop_to_string(b, "!=", node)
	case OP_PRE_INC:
		uop_to_string(b, "pre++", node)
	case OP_PRE_DEC:
		uop_to_string(b, "pre--", node)
	case OP_POST_INC:
		uop_to_string(b, "post++", node)
	case OP_POST_DEC:
		uop_to_string(b, "post--", node)
	case OP_LOGAND:
		binop_to_string(b, "and", node)
	case OP_LOGOR:
		binop_to_string(b, "or", node)
	case OP_A_ADD:
		binop_to_string(b, "+=", node)
	case OP_A_SUB:
		binop_to_string(b, "-=", node)
	case OP_A_MUL:
		binop_to_string(b, "*=", node)
	case OP_A_DIV:
		binop_to_string(b, "/=", node)
	case OP_A_MOD:
		binop_to_string(b, "%=", node)
	case OP_A_AND:
		binop_to_string(b, "&=", node)
	case OP_A_OR:
		binop_to_string(b, "|=", node)
	case OP_A_XOR:
		binop_to_string(b, "^=", node)
	case OP_A_SAL:
		binop_to_string(b, "<<=", node)
	case OP_A_SAR, OP_A_SHR:
		binop_to_string(b, ">>=", node)
	case '!':
		uop_to_string(b, "!", node)
	case '&':
		binop_to_string(b, "&", node)
	case '|':
		binop_to_string(b, "|", node)
	case OP_CAST:
		fmt.Fprintf(b, "((%s)=>(%s) ", ty2s(node.operand.ty), ty2s(node.ty))
		do_node2s(b, node.operand)
		fmt.Fprintf(b, ")")
	case OP_LABEL_ADDR:
		fmt.Fprintf(b, "&&%s", node.label)
	default:
		if node.kind == OP_EQ {
			fmt.Fprintf(b, "(== ")
		} else {
			fmt.Fprintf(b, "(%c ", node.kind)
		}
		do_node2s(b, node.left)
		fmt.Fprintf(b, " ")
		do_node2s(b, node.right)
		fmt.Fprintf(b, ")")
	}
}

// parse.c
func is_inttype(ty *Type) bool {
	switch ty.kind {
	case KIND_BOOL, KIND_CHAR, KIND_SHORT, KIND_INT:
		return true
	}
	return false
}

func is_flotype(ty *Type) bool {
	switch ty.kind {
	case KIND_FLOAT, KIND_DOUBLE, KIND_LDOUBLE:
		return true
	}
	return false
}

func boole(x bool) int {
	if x {
		return 1
	}
	return 0
}

func eval_struct_ref(node *Node, offset int) int {
	if node.kind == AST_STRUCT_REF {
		return eval_struct_ref(node.struc, node.ty.offset+offset)
	}
	return eval_intexpr(node, nil) + offset
}

func conv(node *Node) *Node {
	if node == nil {
		return nil
	}
	ty := node.ty
	switch ty.kind {
	case KIND_ARRAY:
		// C11 6.3.2.1p3: An array of T is converted to a pointer to T.
		return &Node{kind: AST_CONV, ty: &Type{kind: KIND_PTR, ptr: ty.ptr, size: 8, align: 8}, operand: node}
	case KIND_FUNC:
		// C11 6.3.2.1p4: A function designator is converted to a pointer to the function.
		return &Node{kind: AST_ADDR, ty: &Type{kind: KIND_PTR, ptr: ty, size: 8, align: 8}, operand: node}
	case KIND_SHORT, KIND_CHAR, KIND_BOOL:
		// C11 6.3.1.1p2: The integer promotions
		return &Node{kind: AST_CONV, ty: type_int, operand: node}
	}
	return node
}

func eval_intexpr(node *Node, addr **Node) int {
	switch node.kind {
	case AST_LITERAL:
		if is_inttype(node.ty) {
			return int(node.ival)
		}
		goto err
	case '!':
		return boole(eval_intexpr(node.operand, addr) == 0)
	case '~':
		return ^eval_intexpr(node.operand, addr)
	case OP_CAST:
		return eval_intexpr(node.operand, addr)
	case AST_CONV:
		return eval_intexpr(node.operand, addr)
	case AST_ADDR:
		if node.operand.kind == AST_STRUCT_REF {
			return eval_struct_ref(node.operand, 0)
		}
		fallthrough
	case AST_GVAR:
		if addr != nil {
			*addr = conv(node)
			return 0
		}
		goto err
	case AST_DEREF:
		if node.operand.ty.kind == KIND_PTR {
			return eval_intexpr(node.operand, addr)
		}
		goto err
	case AST_TERNARY:
		cond := eval_intexpr(node.cond, addr)
		if cond != 0 {
			if node.then != nil {
				return eval_intexpr(node.then, addr)
			} else {
				return cond
			}
		}
		return eval_intexpr(node.els, addr)
	case '+':
		return (eval_intexpr(node.left, addr)) + (eval_intexpr(node.right, addr))
	case '-':
		return (eval_intexpr(node.left, addr)) - (eval_intexpr(node.right, addr))
	case '*':
		return (eval_intexpr(node.left, addr)) * (eval_intexpr(node.right, addr))
	case '/':
		return (eval_intexpr(node.left, addr)) / (eval_intexpr(node.right, addr))
	case '<':
		return boole((eval_intexpr(node.left, addr)) < (eval_intexpr(node.right, addr)))
	case '^':
		return (eval_intexpr(node.left, addr)) ^ (eval_intexpr(node.right, addr))
	case '&':
		return (eval_intexpr(node.left, addr)) & (eval_intexpr(node.right, addr))
	case '|':
		return (eval_intexpr(node.left, addr)) | (eval_intexpr(node.right, addr))
	case '%':
		return (eval_intexpr(node.left, addr)) % (eval_intexpr(node.right, addr))
	case OP_EQ:
		return boole((eval_intexpr(node.left, addr)) == (eval_intexpr(node.right, addr)))
	case OP_LE:
		return boole((eval_intexpr(node.left, addr)) <= (eval_intexpr(node.right, addr)))
	case OP_NE:
		return boole((eval_intexpr(node.left, addr)) != (eval_intexpr(node.right, addr)))
	case OP_SAL:
		return (eval_intexpr(node.left, addr)) << (eval_intexpr(node.right, addr))
	case OP_SAR:
		return (eval_intexpr(node.left, addr)) >> (eval_intexpr(node.right, addr))
	case OP_SHR:
		return int(uint(eval_intexpr(node.left, addr)) >> (eval_intexpr(node.right, addr)))
	case OP_LOGAND:
		return boole((eval_intexpr(node.left, addr) != 0) && (eval_intexpr(node.right, addr) != 0))
	case OP_LOGOR:
		return boole((eval_intexpr(node.left, addr) != 0) || (eval_intexpr(node.right, addr) != 0))
	}
err:
	error("Integer expression expected, but got %#v", node)
	panic("")
}

var make_label_count int

func make_label() string {
	make_label_count++
	return fmt.Sprintf(".L%d", make_label_count-1)
}

var REGS = []string{"rdi", "rsi", "rdx", "rcx", "r8", "r9"}
var SREGS = []string{"dil", "sil", "dl", "cl", "r8b", "r9b"}
var MREGS = []string{"edi", "esi", "edx", "ecx", "r8d", "r9d"}
var stackpos int
var numgp int
var numfp int
var outputfp io.Writer

const REGAREA_SIZE = 176

func emit(format string, a ...any) {
	emit_noindent("\t"+format, a...)
}
func emit_noindent(format string, a ...any) {
	fmt.Fprintf(outputfp, format, a...)
	outputfp.Write([]byte{'\n'})
}

func get_int_reg(ty *Type, r rune) string {
	assert(r == 'a' || r == 'c')
	switch ty.size {
	case 1:
		if r == 'a' {
			return "al"
		} else {
			return "cl"
		}
	case 2:
		if r == 'a' {
			return "ax"
		} else {
			return "cx"
		}
	case 4:
		if r == 'a' {
			return "eax"
		} else {
			return "ecx"
		}
	case 8:
		if r == 'a' {
			return "rax"
		} else {
			return "rcx"
		}
	}
	error("Unknown data size: %#v: %d", ty, ty.size)
	panic("")
}

func get_load_inst(ty *Type) string {
	switch ty.size {
	case 1:
		return "movsbq"
	case 2:
		return "movswq"
	case 4:
		return "movslq"
	case 8:
		return "mov"
	}
	error("Unknown data size: %#v: %d", ty, ty.size)
	panic("")
}

func align(n int, m int) int {
	rem := n % m
	if rem == 0 {
		return n
	}
	return n - rem + m
}

func push_xmm(reg int) {
	emit("sub rsp, 8")
	emit("movsd #xmm%d, (#rsp)", reg)
	stackpos += 8
}

func pop_xmm(reg int) {
	emit("movsd (#rsp), #xmm%d", reg)
	emit("add rsp, 8")
	stackpos -= 8
	assert(stackpos >= 0)
}

func push(reg string) {
	emit("push %s", reg)
	stackpos += 8
}

func pop(reg string) {
	emit("pop %s", reg)
	stackpos -= 8
	assert(stackpos >= 0)
}

func push_struct(size int) int {
	aligned := align(size, 8)
	emit("sub $%d, #rsp", aligned)
	emit("mov #rcx, -8(#rsp)")
	emit("mov #r11, -16(#rsp)")
	emit("mov #rax, #rcx")
	var i int
	for ; i < size; i += 8 {
		emit("movq %d(#rcx), #r11", i)
		emit("mov #r11, %d(#rsp)", i)
	}
	for ; i < size; i += 4 {
		emit("movl %d(#rcx), #r11", i)
		emit("movl #r11d, %d(#rsp)", i)
	}
	for ; i < size; i++ {
		emit("movb %d(#rcx), #r11", i)
		emit("movb #r11b, %d(#rsp)", i)
	}
	emit("mov -8(#rsp), #rcx")
	emit("mov -16(#rsp), #r11")
	stackpos += aligned
	return aligned
}

func emit_gload(ty *Type, label string, off int) {
	if ty.kind == KIND_ARRAY {
		if off != 0 {
			emit("lea %s+%d(#rip), #rax", label, off)
		} else {
			emit("lea %s(#rip), #rax", label)
		}
		return
	}
	inst := get_load_inst(ty)
	emit("%s %s+%d(#rip), #rax", inst, label, off)
}

func emit_intcast(ty *Type) {
	switch ty.kind {
	case KIND_BOOL, KIND_CHAR:
		if ty.usig {
			emit("movzx eax, al")
		} else {
			emit("movsx eax, al")
		}
		return
	case KIND_SHORT:
		if ty.usig {
			emit("movzx eax, ax")
		} else {
			emit("movsx eax, ax")
		}
		return
	case KIND_INT:
		return
	}
}

func emit_toint(ty *Type) {
	if ty.kind == KIND_FLOAT {
		emit("cvttss2si #xmm0, #eax")
	} else if ty.kind == KIND_DOUBLE {
		emit("cvttsd2si #xmm0, #eax")
	}
}

func emit_lload(ty *Type, base string, off int) {
	if ty.kind == KIND_ARRAY {
		emit("lea rax, [%s+%d]", base, off)
	} else if ty.kind == KIND_FLOAT {
		emit("movss %d(#%s), #xmm0", off, base)
	} else if ty.kind == KIND_DOUBLE || ty.kind == KIND_LDOUBLE {
		emit("movsd %d(#%s), #xmm0", off, base)
	} else {
		inst := get_load_inst(ty)
		emit("%s rax, [%s+%d]", inst, base, off)
	}
}

func maybe_convert_bool(ty *Type) {
	if ty.kind == KIND_BOOL {
		emit("test eax, eax")
		emit("setnz al")
	}
}

func emit_gsave(varname string, ty *Type, off int) {
	assert(ty.kind != KIND_ARRAY)
	maybe_convert_bool(ty)
	reg := get_int_reg(ty, 'a')
	addr := fmt.Sprintf("%s+%d(%%rip)", varname, off)
	emit("mov #%s, %s", reg, addr)
}

func emit_lsave(ty *Type, off int) {
	if ty.kind == KIND_FLOAT {
		emit("movss #xmm0, %d(#rbp)", off)
	} else if ty.kind == KIND_DOUBLE {
		emit("movsd #xmm0, %d(#rbp)", off)
	} else {
		maybe_convert_bool(ty)
		reg := get_int_reg(ty, 'a')
		addr := fmt.Sprintf("%d(%%rbp)", off)
		emit("mov %s, %s", addr, reg)
	}
}

func do_emit_assign_deref(ty *Type, off int) {
	emit("mov rcx, [rsp]")
	reg := get_int_reg(ty, 'c')
	if off != 0 {
		emit("mov [rax+%d], %s", off, reg)
	} else {
		emit("mov [rax], %s", reg)
	}
	pop("rax")
}

func emit_assign_deref(variable *Node) {
	push("rax")
	emit_expr(variable.operand)
	do_emit_assign_deref(variable.operand.ty.ptr, 0)
}

func emit_pointer_arith(kind int, left *Node, right *Node) {
	emit_expr(left)
	push("rcx")
	push("rax")
	emit_expr(right)
	size := left.ty.ptr.size
	if size > 1 {
		emit("imul rax, %d", size)
	}
	emit("mov rcx, rax")
	pop("rax")
	switch kind {
	case '+':
		emit("add rax, rcx")
	case '-':
		emit("sub rax, rcx")
	default:
		error("invalid operator '%d'", kind)
	}
	pop("rcx")
}

func emit_zero_filler(start int, end int) {
	for ; start <= end-4; start += 4 {
		emit("movl $0, %d(#rbp)", start)
	}
	for ; start < end; start++ {
		emit("movb $0, %d(#rbp)", start)
	}
}

func ensure_lvar_init(node *Node) {
	assert(node.kind == AST_LVAR)
	if len(node.lvarinit) != 0 {
		emit_decl_init(node.lvarinit, node.loff, node.ty.size)
	}
	node.lvarinit = nil
}

func emit_assign_struct_ref(struc *Node, field *Type, off int) {
	switch struc.kind {
	case AST_LVAR:
		ensure_lvar_init(struc)
		emit_lsave(field, struc.loff+field.offset+off)
	case AST_GVAR:
		emit_gsave(struc.glabel, field, field.offset+off)
	case AST_STRUCT_REF:
		emit_assign_struct_ref(struc.struc, field, off+struc.ty.offset)
	case AST_DEREF:
		push("rax")
		emit_expr(struc.operand)
		do_emit_assign_deref(field, field.offset+off)
	default:
		error("internal error: %#v", struc)
	}
}

func emit_load_struct_ref(struc *Node, field *Type, off int) {
	switch struc.kind {
	case AST_LVAR:
		ensure_lvar_init(struc)
		emit_lload(field, "rbp", struc.loff+field.offset+off)
	case AST_GVAR:
		emit_gload(field, struc.glabel, field.offset+off)
	case AST_STRUCT_REF:
		emit_load_struct_ref(struc.struc, field, struc.ty.offset+off)
	case AST_DEREF:
		emit_expr(struc.operand)
		emit_lload(field, "rax", field.offset+off)
	default:
		error("internal error: %#v", struc)
	}
}

func emit_store(variable *Node) {
	switch variable.kind {
	case AST_DEREF:
		emit_assign_deref(variable)
	case AST_STRUCT_REF:
		emit_assign_struct_ref(variable.struc, variable.ty, 0)
	case AST_LVAR:
		ensure_lvar_init(variable)
		emit_lsave(variable.ty, variable.loff)
	case AST_GVAR:
		emit_gsave(variable.glabel, variable.ty, 0)
	default:
		error("internal error")
	}
}

func emit_to_bool(ty *Type) {
	if is_flotype(ty) {
		push_xmm(1)
		emit("xorpd #xmm1, #xmm1")
		if ty.kind == KIND_FLOAT {
			emit("ucomiss #xmm1, #xmm0")
		} else {
			emit("ucomisd #xmm1, #xmm0")
		}
		emit("setne #al")
		pop_xmm(1)
	} else {
		emit("cmp rax, 0")
		emit("setne al")
	}
	emit("movzb #al, #eax")
}

func emit_comp(inst string, usiginst string, node *Node) {
	if is_flotype(node.left.ty) {
		emit_expr(node.left)
		push_xmm(0)
		emit_expr(node.right)
		pop_xmm(1)
		if node.left.ty.kind == KIND_FLOAT {
			emit("ucomiss #xmm0, #xmm1")
		} else {
			emit("ucomisd #xmm0, #xmm1")
		}
	} else {
		emit_expr(node.left)
		push("rax")
		emit_expr(node.right)
		pop("rcx")
		emit("cmp ecx, eax")
	}
	if is_flotype(node.left.ty) || node.left.ty.usig {
		emit("%s al", usiginst)
	} else {
		emit("%s al", inst)
	}
	emit("movzx eax, al")
}

func emit_binop_int_arith(node *Node) {
	var op string
	switch node.kind {
	case '+':
		op = "add"
	case '-':
		op = "sub"
	case '*':
		op = "imul"
	case '^':
		op = "xor"
	case OP_SAL:
		op = "sal"
	case OP_SAR:
		op = "sar"
	case OP_SHR:
		op = "shr"
	case '/', '%':
	default:
		error("invalid operator '%d'", node.kind)
	}
	emit_expr(node.left)
	push("rax")
	emit_expr(node.right)
	emit("mov #rax, #rcx")
	pop("rax")
	if node.kind == '/' || node.kind == '%' {
		if node.ty.usig {
			emit("xor #edx, #edx")
			emit("div #rcx")
		} else {
			emit("cqto")
			emit("idiv #rcx")
		}
		if node.kind == '%' {
			emit("mov #edx, #eax")
		}
	} else if node.kind == OP_SAL || node.kind == OP_SAR || node.kind == OP_SHR {
		emit("%s #cl, #%s", op, get_int_reg(node.left.ty, 'a'))
	} else {
		emit("%s #rcx, #rax", op)
	}
}

func emit_binop_float_arith(node *Node) {
	suffix := 's'
	if node.ty.kind == KIND_DOUBLE {
		suffix = 'd'
	}
	var op string
	switch node.kind {
	case '+':
		op = "adds"
	case '-':
		op = "subs"
	case '*':
		op = "muls"
	case '/':
		op = "divs"
	default:
		error("invalid operator '%d'", node.kind)
	}
	emit_expr(node.left)
	push_xmm(0)
	emit_expr(node.right)
	emit("movs%c #xmm0, #xmm1", suffix)
	pop_xmm(0)
	emit("%s%c #xmm1, #xmm0", op, suffix)
}

func emit_load_convert(to *Type, from *Type) {
	if is_inttype(from) && to.kind == KIND_FLOAT {
		emit("cvtsi2ss #eax, #xmm0")
	} else if is_inttype(from) && to.kind == KIND_DOUBLE {
		emit("cvtsi2sd #eax, #xmm0")
	} else if from.kind == KIND_FLOAT && to.kind == KIND_DOUBLE {
		emit("cvtps2pd #xmm0, #xmm0")
	} else if (from.kind == KIND_DOUBLE || from.kind == KIND_LDOUBLE) && to.kind == KIND_FLOAT {
		emit("cvtpd2ps #xmm0, #xmm0")
	} else if to.kind == KIND_BOOL {
		emit_to_bool(from)
	} else if is_inttype(from) && is_inttype(to) {
		emit_intcast(from)
	} else if is_inttype(to) {
		emit_toint(from)
	}
}

func emit_ret() {
	emit("leave")
	emit("ret")
}

func emit_binop(node *Node) {
	if node.ty.kind == KIND_PTR {
		emit_pointer_arith(node.kind, node.left, node.right)
		return
	}
	switch node.kind {
	case int('<'):
		emit_comp("setl", "setb", node)
		return
	case OP_EQ:
		emit_comp("sete", "sete", node)
		return
	case OP_LE:
		emit_comp("setle", "setna", node)
		return
	case OP_NE:
		emit_comp("setne", "setne", node)
		return
	}
	if is_inttype(node.ty) {
		emit_binop_int_arith(node)
	} else if is_flotype(node.ty) {
		emit_binop_float_arith(node)
	} else {
		error("internal error: %#v", node)
	}
}

func emit_save_literal(node *Node, totype *Type, off int) {
	switch totype.kind {
	case KIND_BOOL:
		emit("movb $%d, %d(#rbp)", boole(node.ival != 0), off)
	case KIND_CHAR:
		emit("movb $%d, %d(#rbp)", node.ival, off)
	case KIND_SHORT:
		emit("movw $%d, %d(#rbp)", node.ival, off)
	case KIND_INT, KIND_PTR:
		emit("movl $%d, %d(#rbp)", node.ival, off)
	case KIND_FLOAT:
		emit("movl $%d, %d(#rbp)", math.Float32bits(float32(node.fval)), off)
	case KIND_DOUBLE, KIND_LDOUBLE:
		emit("movl $%d, %d(#rbp)", math.Float64bits(node.fval)&((1<<32)-1), off)
		emit("movl $%d, %d(#rbp)", math.Float64bits(node.fval)>>32, off+4)
	default:
		error("internal error: <%#v> <%#v> <%d>", node, totype, off)
	}
}

func emit_addr(node *Node) {
	switch node.kind {
	case AST_LVAR:
		ensure_lvar_init(node)
		emit("lea %d(#rbp), #rax", node.loff)
	case AST_GVAR:
		emit("lea %s(#rip), #rax", node.glabel)
	case AST_DEREF:
		emit_expr(node.operand)
	case AST_STRUCT_REF:
		emit_addr(node.struc)
		emit("add $%d, #rax", node.ty.offset)
	case AST_FUNCDESG:
		emit("lea %s(#rip), #rax", node.fname)
	default:
		error("internal error: %#v", node)
	}
}

func emit_copy_struct(left *Node, right *Node) {
	push("rcx")
	push("r11")
	emit_addr(right)
	emit("mov #rax, #rcx")
	emit_addr(left)
	var i int
	for ; i < left.ty.size; i += 8 {
		emit("movq %d(#rcx), #r11", i)
		emit("movq #r11, %d(#rax)", i)
	}
	for ; i < left.ty.size; i += 4 {
		emit("movl %d(#rcx), #r11", i)
		emit("movl #r11, %d(#rax)", i)
	}
	for ; i < left.ty.size; i++ {
		emit("movb %d(#rcx), #r11", i)
		emit("movb #r11, %d(#rax)", i)
	}
	pop("r11")
	pop("rcx")
}

func emit_fill_holes(inits []*Node, off int, totalsize int) {
	// If at least one of the fields in a variable are initialized,
	// unspecified fields has to be initialized with 0.
	var buf []*Node
	copy(buf, inits)
	sort.Slice(buf, func(i, j int) bool {
		return buf[i].initoff < buf[j].initoff
	})

	lastend := 0
	for _, node := range buf {
		if lastend < node.initoff {
			emit_zero_filler(lastend+off, node.initoff+off)
		}
		lastend = node.initoff + node.totype.size
	}
	emit_zero_filler(lastend+off, totalsize+off)
}

func emit_decl_init(inits []*Node, off int, totalsize int) {
	emit_fill_holes(inits, off, totalsize)
	for _, node := range inits {
		assert(node.kind == AST_INIT)
		if node.initval.kind == AST_LITERAL {
			emit_save_literal(node.initval, node.totype, node.initoff+off)
		} else {
			emit_expr(node.initval)
			emit_lsave(node.totype, node.initoff+off)
		}
	}
}

func emit_pre_inc_dec(node *Node, op string) {
	emit_expr(node.operand)
	size := 1
	if node.ty.ptr != nil {
		size = node.ty.ptr.size
	}
	emit("%s $%d, #rax", op, size)
	emit_store(node.operand)
}

func emit_post_inc_dec(node *Node, op string) {
	emit_expr(node.operand)
	push("rax")
	size := 1
	if node.ty.ptr != nil {
		size = node.ty.ptr.size
	}
	emit("%s $%d, #rax", op, size)
	emit_store(node.operand)
	pop("rax")
}

func set_reg_nums(args []*Node) {
	numgp = 0
	numfp = 0
	for _, arg := range args {
		if is_flotype(arg.ty) {
			numfp++
		} else {
			numgp++
		}
	}
}

func emit_je(label string) {
	emit("test #rax, #rax")
	emit("je %s", label)
}

func emit_label(label string) {
	emit("%s:", label)
}

func emit_jmp(label string) {
	emit("jmp %s", label)
}

func emit_literal(node *Node) {
	switch node.ty.kind {
	case KIND_BOOL, KIND_CHAR, KIND_SHORT, KIND_INT:
		emit("mov eax, %d", node.ival)
	case KIND_FLOAT:
		if node.flabel == "" {
			node.flabel = make_label()
			emit_noindent(".data")
			emit_label(node.flabel)
			emit(".long %d", math.Float32bits(float32(node.fval)))
			emit_noindent(".text")
		}
		emit("movss %s(#rip), #xmm0", node.flabel)
	case KIND_DOUBLE, KIND_LDOUBLE:
		if node.flabel == "" {
			node.flabel = make_label()
			emit_noindent(".data")
			emit_label(node.flabel)
			emit(".quad %lu", math.Float64bits(node.fval))
			emit_noindent(".text")
		}
		emit("movsd %s(#rip), #xmm0", node.flabel)
	case KIND_ARRAY:
		if node.slabel == "" {
			node.slabel = make_label()
			emit_noindent(".data")
			emit_label(node.slabel)
			emit(".string %q", node.sval)
			emit_noindent(".text")
		}
		emit("lea %s(#rip), #rax", node.slabel)
	default:
		error("internal error")
	}
}

func emit_lvar(node *Node) {
	ensure_lvar_init(node)
	emit_lload(node.ty, "rbp", node.loff)
}

func emit_gvar(node *Node) {
	emit_gload(node.ty, node.glabel, 0)
}

func emit_builtin_return_address(node *Node) {
	push("r11")
	assert(len(node.args) == 1)
	emit_expr(node.args[0])
	loop := make_label()
	end := make_label()
	emit("mov #rbp, #r11")
	emit_label(loop)
	emit("test #rax, #rax")
	emit("jz %s", end)
	emit("mov (#r11), #r11")
	emit("sub $1, #rax")
	emit_jmp(loop)
	emit_label(end)
	emit("mov 8(#r11), #rax")
	pop("r11")
}

// Set the register class for parameter passing to RAX.
// 0 is INTEGER, 1 is SSE, 2 is MEMORY.
func emit_builtin_reg_class(node *Node) {
	arg := node.args[0]
	assert(arg.ty.kind == KIND_PTR)
	ty := arg.ty.ptr
	if ty.kind == KIND_STRUCT {
		emit("mov $2, #eax")
	} else if is_flotype(ty) {
		emit("mov $1, #eax")
	} else {
		emit("mov $0, #eax")
	}
}

func emit_builtin_va_start(node *Node) {
	assert(len(node.args) == 1)
	emit_expr(node.args[0])
	push("rcx")
	emit("movl $%d, (#rax)", numgp*8)
	emit("movl $%d, 4(#rax)", 48+numfp*16)
	emit("lea %d(#rbp), #rcx", -REGAREA_SIZE)
	emit("mov #rcx, 16(#rax)")
	pop("rcx")
}

func maybe_emit_builtin(node *Node) bool {
	if node.fname == "__builtin_return_address" {
		emit_builtin_return_address(node)
		return true
	}
	if node.fname == "__builtin_reg_class" {
		emit_builtin_reg_class(node)
		return true
	}
	if node.fname == "__builtin_va_start" {
		emit_builtin_va_start(node)
		return true
	}
	return false
}

func save_arg_regs(nints int, nfloats int) {
	assert(nints <= 6)
	assert(nfloats <= 8)
	for i := 0; i < nints; i++ {
		push(REGS[i])
	}
	for i := 1; i < nfloats; i++ {
		push_xmm(i)
	}
}

func restore_arg_regs(nints int, nfloats int) {
	for i := nfloats - 1; i > 0; i-- {
		pop_xmm(i)
	}
	for i := nints - 1; i >= 0; i-- {
		pop(REGS[i])
	}
}

func emit_args(vals []*Node) int {
	r := 0
	for _, v := range vals {
		if v.ty.kind == KIND_STRUCT {
			emit_addr(v)
			r += push_struct(v.ty.size)
		} else if is_flotype(v.ty) {
			emit_expr(v)
			push_xmm(0)
			r += 8
		} else {
			emit_expr(v)
			push("rax")
			r += 8
		}
	}
	return r
}

func pop_int_args(nints int) {
	for i := nints - 1; i >= 0; i-- {
		pop(REGS[i])
	}
}

func pop_float_args(nfloats int) {
	for i := nfloats - 1; i >= 0; i-- {
		pop_xmm(i)
	}
}

func maybe_booleanize_retval(ty *Type) {
	if ty.kind == KIND_BOOL {
		emit("movzx #al, #rax")
	}
}

func emit_func_call(node *Node) {
	var opos int = stackpos
	isptr := (node.kind == AST_FUNCPTR_CALL)
	var ftype *Type
	if isptr {
		ftype = node.fptr.ty.ptr
	} else {
		ftype = node.ftype
	}

	var ints []*Node
	var floats []*Node
	var rest []*Node
	// classify args
	for _, v := range node.args {
		if v.ty.kind == KIND_STRUCT {
			rest = append([]*Node{v}, rest...)
		} else if is_flotype(v.ty) {
			if len(floats) < 8 {
				floats = append(floats, v)
			} else {
				rest = append([]*Node{v}, rest...)
			}
		} else {
			if len(ints) < 6 {
				ints = append(ints, v)
			} else {
				rest = append([]*Node{v}, rest...)
			}
		}
	}
	save_arg_regs(len(ints), len(floats))

	padding := stackpos%16 != 0
	if padding {
		emit("sub rsp, 8")
		stackpos += 8
	}

	restsize := emit_args(rest)
	if isptr {
		emit_expr(node.fptr)
		push("rax")
	}
	emit_args(ints)
	emit_args(floats)
	pop_float_args(len(floats))
	pop_int_args(len(ints))

	if isptr {
		pop("r11")
	}
	if ftype.hasva {
		emit("mov eax, %d", len(floats))
	}

	if isptr {
		emit("call [r11]")
	} else {
		emit("call %s", node.fname)
	}
	maybe_booleanize_retval(node.ty)
	if restsize > 0 {
		emit("add rsp, %d", restsize)
		stackpos -= restsize
	}
	if padding {
		emit("add rsp, 8")
		stackpos -= 8
	}
	restore_arg_regs(len(ints), len(floats))
	assert(opos == stackpos)
}

func emit_decl(node *Node) {
	if len(node.declinit) == 0 {
		return
	}
	emit_decl_init(node.declinit, node.declvar.loff, node.declvar.ty.size)
}

func emit_conv(node *Node) {
	emit_expr(node.operand)
	emit_load_convert(node.ty, node.operand.ty)
}

func emit_deref(node *Node) {
	emit_expr(node.operand)
	emit_lload(node.operand.ty.ptr, "rax", 0)
	emit_load_convert(node.ty, node.operand.ty.ptr)
}

func emit_ternary(node *Node) {
	emit_expr(node.cond)
	ne := make_label()
	emit_je(ne)
	if node.then != nil {
		emit_expr(node.then)
	}
	if node.els != nil {
		end := make_label()
		emit_jmp(end)
		emit_label(ne)
		emit_expr(node.els)
		emit_label(end)
	} else {
		emit_label(ne)
	}
}

func emit_goto(node *Node) {
	assert(node.newlabel != "")
	emit_jmp(node.newlabel)
}

func emit_return(node *Node) {
	if node.retval != nil {
		emit_expr(node.retval)
		maybe_booleanize_retval(node.retval.ty)
	}
	emit_ret()
}

func emit_compound_stmt(node *Node) {
	for _, stmt := range node.stmts {
		emit_expr(stmt)
	}
}

func emit_logand(node *Node) {
	end := make_label()
	emit_expr(node.left)
	emit("test #rax, #rax")
	emit("mov $0, #rax")
	emit("je %s", end)
	emit_expr(node.right)
	emit("test #rax, #rax")
	emit("mov $0, #rax")
	emit("je %s", end)
	emit("mov $1, #rax")
	emit_label(end)
}

func emit_logor(node *Node) {
	end := make_label()
	emit_expr(node.left)
	emit("test #rax, #rax")
	emit("mov $1, #rax")
	emit("jne %s", end)
	emit_expr(node.right)
	emit("test #rax, #rax")
	emit("mov $1, #rax")
	emit("jne %s", end)
	emit("mov $0, #rax")
	emit_label(end)
}

func emit_lognot(node *Node) {
	emit_expr(node.operand)
	emit("cmp $0, #rax")
	emit("sete #al")
	emit("movzb #al, #eax")
}

func emit_bitand(node *Node) {
	emit_expr(node.left)
	push("rax")
	emit_expr(node.right)
	pop("rcx")
	emit("and #rcx, #rax")
}

func emit_bitor(node *Node) {
	emit_expr(node.left)
	push("rax")
	emit_expr(node.right)
	pop("rcx")
	emit("or #rcx, #rax")
}

func emit_bitnot(node *Node) {
	emit_expr(node.left)
	emit("not #rax")
}

func emit_cast(node *Node) {
	emit_expr(node.operand)
	emit_load_convert(node.ty, node.operand.ty)
}

func emit_comma(node *Node) {
	emit_expr(node.left)
	emit_expr(node.right)
}

func emit_assign(node *Node) {
	if node.left.ty.kind == KIND_STRUCT &&
		node.left.ty.size > 8 {
		emit_copy_struct(node.left, node.right)
	} else {
		emit_expr(node.right)
		emit_load_convert(node.ty, node.right.ty)
		emit_store(node.left)
	}
}

func emit_label_addr(node *Node) {
	emit("mov $%s, #rax", node.newlabel)
}

func emit_computed_goto(node *Node) {
	emit_expr(node.operand)
	emit("jmp *#rax")
}

func emit_expr(node *Node) {
	switch node.kind {
	case AST_LITERAL:
		emit_literal(node)
		return
	case AST_LVAR:
		emit_lvar(node)
		return
	case AST_GVAR:
		emit_gvar(node)
		return
	case AST_FUNCDESG:
		emit_addr(node)
		return
	case AST_FUNCALL:
		if maybe_emit_builtin(node) {
			return
		}
		fallthrough
	case AST_FUNCPTR_CALL:
		emit_func_call(node)
		return
	case AST_DECL:
		emit_decl(node)
		return
	case AST_CONV:
		emit_conv(node)
		return
	case AST_ADDR:
		emit_addr(node.operand)
		return
	case AST_DEREF:
		emit_deref(node)
		return
	case AST_IF, AST_TERNARY:
		emit_ternary(node)
		return
	case AST_GOTO:
		emit_goto(node)
		return
	case AST_LABEL:
		if node.newlabel != "" {
			emit_label(node.newlabel)
		}
		return
	case AST_RETURN:
		emit_return(node)
		return
	case AST_COMPOUND_STMT:
		emit_compound_stmt(node)
		return
	case AST_STRUCT_REF:
		emit_load_struct_ref(node.struc, node.ty, 0)
		return
	case OP_PRE_INC:
		emit_pre_inc_dec(node, "add")
		return
	case OP_PRE_DEC:
		emit_pre_inc_dec(node, "sub")
		return
	case OP_POST_INC:
		emit_post_inc_dec(node, "add")
		return
	case OP_POST_DEC:
		emit_post_inc_dec(node, "sub")
		return
	case '!':
		emit_lognot(node)
		return
	case '&':
		emit_bitand(node)
		return
	case '|':
		emit_bitor(node)
		return
	case '~':
		emit_bitnot(node)
		return
	case OP_LOGAND:
		emit_logand(node)
		return
	case OP_LOGOR:
		emit_logor(node)
		return
	case OP_CAST:
		emit_cast(node)
		return
	case ',':
		emit_comma(node)
		return
	case '=':
		emit_assign(node)
		return
	case OP_LABEL_ADDR:
		emit_label_addr(node)
		return
	case AST_COMPUTED_GOTO:
		emit_computed_goto(node)
		return
	default:
		emit_binop(node)
	}
}

func emit_zero(size int) {
	for ; size >= 8; size -= 8 {
		emit(".quad 0")
	}
	for ; size >= 4; size -= 4 {
		emit(".long 0")
	}
	for ; size > 0; size-- {
		emit(".byte 0")
	}
}

func emit_padding(node *Node, off int) {
	diff := node.initoff - off
	assert(diff >= 0)
	emit_zero(diff)
}

func emit_data_addr(operand *Node, depth int) {
	switch operand.kind {
	case AST_LVAR:
		label := make_label()
		emit(".data %d", depth+1)
		emit_label(label)
		do_emit_data(operand.lvarinit, operand.ty.size, 0, depth+1)
		emit(".data %d", depth)
		emit(".quad %s", label)
		return
	case AST_GVAR:
		emit(".quad %s", operand.glabel)
		return
	default:
		error("internal error")
	}
}

func emit_data_charptr(s string, depth int) {
	label := make_label()
	emit(".data %d", depth+1)
	emit_label(label)
	emit(".string %q", s)
	emit(".data %d", depth)
	emit(".quad %s", label)
}

func emit_data_primtype(ty *Type, val *Node, depth int) {
	switch ty.kind {
	case KIND_FLOAT:
		emit(".long %d", math.Float32bits(float32(val.fval)))
	case KIND_DOUBLE:
		emit(".quad %d", math.Float64bits(val.fval))
	case KIND_BOOL:
		emit(".byte %d", boole(eval_intexpr(val, nil) != 0))
	case KIND_CHAR:
		emit(".byte %d", eval_intexpr(val, nil))
	case KIND_SHORT:
		emit(".short %d", eval_intexpr(val, nil))
	case KIND_INT:
		emit(".long %d", eval_intexpr(val, nil))
	case KIND_PTR:
		if val.kind == OP_LABEL_ADDR {
			emit(".quad %s", val.newlabel)
			break
		}
		//is_char_ptr := (val.operand.ty.kind == KIND_ARRAY && val.operand.ty.ptr.kind == KIND_CHAR)
		is_char_ptr := false
		if is_char_ptr {
			emit_data_charptr(val.operand.sval, depth)
		} else if val.kind == AST_GVAR {
			emit(".quad %s", val.glabel)
		} else {
			var base *Node
			v := eval_intexpr(val, &base)
			if base == nil {
				emit(".quad %d", v)
				break
			}
			ty := base.ty
			if base.kind == AST_CONV || base.kind == AST_ADDR {
				base = base.operand
			}
			if base.kind != AST_GVAR {
				error("global variable expected, but got %#v", base)
			}
			assert(ty.ptr != nil)
			emit(".quad %s+%d", base.glabel, v*ty.ptr.size)
		}
	default:
		error("don't know how to handle\n  <%#v>\n  <%#v>", ty, val)
	}
}

func do_emit_data(inits []*Node, size int, off int, depth int) {
	for i := 0; i < len(inits) && size > 0; i++ {
		node := inits[i]
		v := node.initval
		emit_padding(node, off)
		off += node.totype.size
		size -= node.totype.size
		if v.kind == AST_ADDR {
			emit_data_addr(v.operand, depth)
			continue
		}
		if v.kind == AST_LVAR && len(v.lvarinit) != 0 {
			do_emit_data(v.lvarinit, v.ty.size, 0, depth)
			continue
		}
		emit_data_primtype(node.totype, node.initval, depth)
	}
	emit_zero(size)
}

func emit_data(v *Node, off int, depth int) {
	emit(".data %d", depth)
	emit_noindent("%s:", v.declvar.glabel)
	do_emit_data(v.declinit, v.declvar.ty.size, off, depth)
}

func emit_bss(v *Node) {
	emit(".data")
	emit(".lcomm %s, %d", v.declvar.glabel, v.declvar.ty.size)
}

func emit_global_var(v *Node) {
	if v.declinit != nil {
		emit_data(v, 0, 0)
	} else {
		emit_bss(v)
	}
}

func emit_regsave_area() int {
	emit("sub $%d, #rsp", REGAREA_SIZE)
	emit("mov #rdi, (#rsp)")
	emit("mov #rsi, 8(#rsp)")
	emit("mov #rdx, 16(#rsp)")
	emit("mov #rcx, 24(#rsp)")
	emit("mov #r8, 32(#rsp)")
	emit("mov #r9, 40(#rsp)")
	emit("movaps #xmm0, 48(#rsp)")
	emit("movaps #xmm1, 64(#rsp)")
	emit("movaps #xmm2, 80(#rsp)")
	emit("movaps #xmm3, 96(#rsp)")
	emit("movaps #xmm4, 112(#rsp)")
	emit("movaps #xmm5, 128(#rsp)")
	emit("movaps #xmm6, 144(#rsp)")
	emit("movaps #xmm7, 160(#rsp)")
	return REGAREA_SIZE
}

func push_func_params(params []*Node, off int) {
	ireg := 0
	xreg := 0
	arg := 2
	for _, v := range params {
		if v.ty.kind == KIND_STRUCT {
			emit("lea rax, [rbp+%d]", arg*8)
			size := push_struct(v.ty.size)
			off -= size
			arg += size / 8
		} else if is_flotype(v.ty) {
			if xreg >= 8 {
				emit("mov rax, [rbp+%d]", arg*8)
				arg++
				push("rax")
			} else {
				push_xmm(xreg)
				xreg++
			}
			off -= 8
		} else {
			if ireg >= 6 {
				if v.ty.kind == KIND_BOOL {
					emit("mov %d(#rbp), #al", arg*8)
					arg++
					emit("movzb #al, #eax")
				} else {
					emit("mov %d(#rbp), #rax", arg*8)
					arg++
				}
				push("rax")
			} else {
				if v.ty.kind == KIND_BOOL {
					emit("movzb #%s, #%s", SREGS[ireg], MREGS[ireg])
				}
				push(REGS[ireg])
				ireg++
			}
			off -= 8
		}
		v.loff = off
	}
}

func emit_func_prologue(function *Node) {
	emit_noindent("%s:", function.fname)
	push("ebp")
	emit("mov ebp, esp")
	off := 0
	if function.ty.hasva {
		set_reg_nums(function.params)
		off -= emit_regsave_area()
	}
	push_func_params(function.params, off)
	off -= len(function.params) * 8

	localarea := 0
	for _, v := range function.localvars {
		size := align(v.ty.size, 8)
		assert(size%8 == 0)
		off -= size
		v.loff = off
		localarea += size
	}
	if localarea != 0 {
		emit("sub esp, %d", localarea)
		stackpos += localarea
	}
}

func emit_toplevel(v *Node) {
	stackpos = 8
	if v.kind == AST_FUNC {
		emit_func_prologue(v)
		emit_expr(v.body)
		emit_ret()
	} else if v.kind == AST_DECL {
		emit_global_var(v)
	} else {
		error("internal error")
	}
}
