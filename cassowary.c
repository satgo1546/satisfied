// https://github.com/starwing/amoeba/blob/3de7bc50b3a38d0b5c6baf9ed9edf3b5c2c6f1f0/amoeba.h

#define CL_OK 0
#define CL_FAILED (-1)
#define CL_UNSATISFIED (-2)
#define CL_UNBOUND (-3)

#define CL_REQUIRED 1001001000.0
#define CL_STRONG 1e6
#define CL_MEDIUM 1e3
#define CL_WEAK 1.0

#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

// ClDummyVariable
// ClVariable
// ClSlackVariable
// ClObjectiveVariable
enum {
	CL_EXTERNAL = 0,
	CL_SLACK,
	CL_ERROR,
	CL_DUMMY,
};

#define cl_ispivotable(key) ((key).type == CL_SLACK || (key).type == CL_ERROR)

#define CL_POOLSIZE 4096
#define CL_MIN_HASHSIZE 4

typedef struct {
	unsigned id : 30;
	unsigned type : 2;
} cl_Symbol;

typedef struct cl_Entry {
	int next;
	cl_Symbol key;
} cl_Entry;

typedef struct cl_Table {
	size_t size;
	size_t count;
	size_t entry_size;
	size_t lastfree;
	cl_Entry *hash;
} cl_Table;

typedef struct cl_VarEntry {
	int next;
	cl_Symbol key;
	struct cl_Variable {
		cl_Symbol sym;
		cl_Symbol dirty_next;
		unsigned refcount;
		struct cl_Constraint *constraint;
		double edit_value;
		double value;
	} *var;
} cl_VarEntry;

typedef struct cl_ConsEntry {
	int next;
	cl_Symbol key;
	struct cl_Constraint {
		struct cl_Row {
			cl_Entry entry;
			cl_Symbol infeasible_next;
			cl_Table terms;
			double constant;
		} expression;
		cl_Symbol marker;
		cl_Symbol other;
		bool equal;
		double strength;
	} *constraint;
} cl_ConsEntry;

typedef struct cl_Term {
	int next;
	cl_Symbol key;
	double multiplier;
} cl_Term;

struct cl_Solver {
	struct cl_Row objective;
	cl_Table vars; /* symbol -> VarEntry */
	cl_Table constraints; /* symbol -> ConsEntry */
	cl_Table rows; /* symbol -> Row */
	unsigned symbol_count;
	unsigned constraint_count;
	bool auto_update;
	cl_Symbol infeasible_rows;
	cl_Symbol dirty_vars;
};

void cl_updatevars(struct cl_Solver *solver);
void cl_remove(struct cl_Solver *solver, struct cl_Constraint *cons);
void cl_delconstraint(struct cl_Solver *solver, struct cl_Constraint *cons);

static cl_Symbol cl_newsymbol(struct cl_Solver *solver, int type) {
	unsigned id = ++solver->symbol_count;
	if (id > 0x3fffffff) id = solver->symbol_count = 1;
	assert(type >= 0 && type <= 3);
	return (cl_Symbol) {.id = id, .type = type};
}

// Find the exception.
// CSS4  clamp(MIN, VAL, MAX)
// GLSL  genType clamp(genType x, genType minVal, genType maxVal);
// HLSL  ret clamp(x, min, max)
// C++17  template <class T> constexpr const T& clamp(const T& v, const T& lo, const T& hi);
// VB.NET  Public Shared Function Clamp#(value#, min#, max#)
// Arduino  constrain(x, a, b)
#define clamp(x, a, b) fmin(fmax(x, a), b)

static int cl_nearzero(double a) {
	return fabs(a) < 1e-6;
}

static cl_Symbol cl_null() {
	return (cl_Symbol) {.id = 0, .type = CL_EXTERNAL};
}

static void cl_initsymbol(struct cl_Solver *solver, cl_Symbol *sym, int type) {
	if (!sym->id) *sym = cl_newsymbol(solver, type);
}

/* hash table */


/*void printtable(const cl_Table *t) {
	static int ii;
	printf("#%i, %p, size = %d, count = %d, entry_size = %d, hash = %p\n", ii++, t, (int) t->size, (int) t->count, (int) t->entry_size, t->hash);
	for (size_t i = 0; i < t->size; i++) {
		cl_Entry *e = (cl_Entry *) ((unsigned char *) t->hash + t->entry_size * i);
		printf("[%p] %d %d\n", e, e->key.type, e->key.id);
		if (i >= t->count && e->key.id) printf("!!!!!!!!!!!!!!!!");
	}
}

static void cl_inittable(cl_Table *t, size_t entry_size) {
	t->size = 4;
	t->count = 0;
	t->entry_size = entry_size;
	t->hash = calloc(entry_size, 4);
}

static void cl_freetable(cl_Table *t) {
	free(t->hash);
}

static void cl_resizetable(cl_Table *t) {
	t->hash = realloc(t->hash, t->size * t->entry_size * 2);
	memset((unsigned char *) t->hash + t->entry_size * t->size, 0, t->entry_size * t->size);
	t->size *= 2;
}

static cl_Entry *cl_bsearch(const cl_Table *t, cl_Symbol key) {
	cl_Entry *base = t->hash;
	size_t nel = t->count;
	while (nel > 0) {
		cl_Entry *try = (cl_Entry *) ((unsigned char *) base + t->entry_size * (nel / 2));
		if (key.id < try->key.id) {
			nel /= 2;
		} else if (key.id > try->key.id) {
			base = (unsigned char *) try + t->entry_size;
			nel -= nel / 2 + 1;
		} else {
			return try;
		}
	}
	return base;
}

static const cl_Entry *cl_gettable(const cl_Table *t, cl_Symbol key) {
	const cl_Entry *e = cl_bsearch(t, key);
	//printf("bsearch %d = %p\n", key.id, e);printtable(t);
	return e->key.id == key.id ? e : NULL;
}

static cl_Entry *cl_settable(cl_Table *t, cl_Symbol key) {
	assert(key.id);
	if (t->size == t->count) cl_resizetable(t);
	cl_Entry *e = cl_bsearch(t, key);
	if (e->key.id == key.id) return e;
	memmove((unsigned char *) e + t->entry_size, e, t->entry_size * t->count - (size_t) ((unsigned char *) e - (unsigned char *) t->hash));
	t->count++;
	memset(e, 0, t->entry_size);
	e->key = key;
	return e;
}

static void cl_delkey(cl_Table *t, cl_Entry *e) {
	//printf("before delete %p:\n", e);printtable(t);
	memmove(e, (unsigned char *) e + t->entry_size, t->entry_size * t->count - (size_t) ((unsigned char *) e + t->entry_size - (unsigned char *) t->hash));
	t->count--;
	memset((unsigned char *) t->hash + t->entry_size * t->count, 0, t->entry_size);
	//printf("after delete %p:\n", e);printtable(t);
}

static bool cl_nextentry(const cl_Table *t, cl_Entry **pentry) {
	cl_Entry *end = (cl_Entry *) ((unsigned char *) t->hash + t->count * t->entry_size);
	*pentry = *pentry ? (cl_Entry *) ((unsigned char *) *pentry + t->entry_size) : t->hash;
	return *pentry < end;
}
*/

/* hash table */

#define cl_key(entry) (((cl_Entry*)(entry))->key)
#define cl_offset(lhs,rhs) ((int)((char*)(lhs) - (char*)(rhs)))
#define cl_index(h, i) ((cl_Entry*)((char*)(h) + (i)))

static cl_Entry *cl_newkey(cl_Table *t, cl_Symbol key);

static void cl_delkey(cl_Table *t, cl_Entry *entry) {
	entry->key = cl_null();
	t->count--;
}

static void cl_inittable(cl_Table *t, size_t entry_size) {
	memset(t, 0, sizeof(*t));
	t->entry_size = entry_size;
}

static cl_Entry *cl_mainposition(const cl_Table *t, cl_Symbol key) {
	return cl_index(t->hash, (key.id & (t->size - 1))*t->entry_size);
}

static size_t cl_hashsize(cl_Table *t, size_t len) {
	size_t newsize = CL_MIN_HASHSIZE;
	const size_t max_size = ((SIZE_MAX - 100) / 2) / t->entry_size;
	while (newsize < max_size && newsize < len) newsize <<= 1;
	assert(!(newsize & (newsize - 1)));
	return newsize < len ? 0 : newsize;
}

static void cl_freetable(cl_Table *t) {
	if (t->size * t->entry_size) free(t->hash);
	cl_inittable(t, t->entry_size);
}

static size_t cl_resizetable(cl_Table *t, size_t len) {
	size_t oldsize = t->size * t->entry_size;
	cl_Table nt = *t;
	nt.size = cl_hashsize(t, len);
	nt.lastfree = nt.size*nt.entry_size;
	nt.hash = (cl_Entry*) malloc(nt.lastfree);
	memset(nt.hash, 0, nt.size*nt.entry_size);
	for (size_t i = 0; i < oldsize; i += nt.entry_size) {
		cl_Entry *e = cl_index(t->hash, i);
		if (e->key.id) {
			cl_Entry *ne = cl_newkey(&nt, e->key);
			if (t->entry_size > sizeof(cl_Entry)) {
				memcpy(ne + 1, e + 1, t->entry_size - sizeof(cl_Entry));
			}
		}
	}
	if (oldsize) free(t->hash);
	*t = nt;
	return t->size;
}

static cl_Entry *cl_newkey(cl_Table *t, cl_Symbol key) {
	if (t->size == 0) cl_resizetable(t, CL_MIN_HASHSIZE);
	for (;;) {
		cl_Entry *mp = cl_mainposition(t, key);
		if (mp->key.id) {
			cl_Entry *f = NULL, *othern;
			while (t->lastfree > 0) {
				cl_Entry *e = cl_index(t->hash, t->lastfree -= t->entry_size);
				if (e->key.id == 0 && e->next == 0) { f = e; break; }
			}
			if (!f) { cl_resizetable(t, t->count*2); continue; }
			assert(!f->key.id);
			othern = cl_mainposition(t, mp->key);
			if (othern != mp) {
				cl_Entry *next;
				while ((next = cl_index(othern, othern->next)) != mp) {
					othern = next;
				}
				othern->next = cl_offset(f, othern);
				memcpy(f, mp, t->entry_size);
				if (mp->next) f->next += cl_offset(mp, f), mp->next = 0;
			} else {
				if (mp->next) {
					f->next = cl_offset(mp, f) + mp->next;
				} else {
					assert(!f->next);
				}
				mp->next = cl_offset(f, mp), mp = f;
			}
		}
		mp->key = key;
		return mp;
	}
}

static const cl_Entry *cl_gettable(const cl_Table *t, cl_Symbol key) {
	//printf("gettable %d\n", key.id);
	const cl_Entry *e;
	if (t->size == 0 || key.id == 0) return NULL;
	for (e = cl_mainposition(t, key); e->key.id != key.id; e = cl_index(e, e->next)) {
		if (!e->next) return NULL;
	}
	return e;
}

static cl_Entry *cl_settable(cl_Table *t, cl_Symbol key) {
	assert(key.id);
	//printf("settable %d\n", key.id);
	cl_Entry *e = (cl_Entry*)cl_gettable(t, key);
	if (e) return e;
	e = cl_newkey(t, key);
	if (t->entry_size > sizeof(cl_Entry)) {
		memset(e + 1, 0, t->entry_size-sizeof(cl_Entry));
	}
	++t->count;
	return e;
}

static int cl_nextentry(const cl_Table *t, cl_Entry **pentry) {
	cl_Entry *end = cl_index(t->hash, t->size*t->entry_size);
	cl_Entry *e = *pentry;
	e = e ? cl_index(e, t->entry_size) : t->hash;
	for (; e < end; e = cl_index(e, t->entry_size))
		if (e->key.id != 0) return *pentry = e, 1;
	*pentry = NULL;
	return 0;
}

/* expression (row) */
static int cl_isconstant(struct cl_Row *row) {
	return !row->terms.count;
}

static void cl_freerow(struct cl_Row *row) {
	cl_freetable(&row->terms);
}

static void cl_initrow(struct cl_Row *row) {
	cl_key(row) = cl_null();
	row->infeasible_next = cl_null();
	row->constant = 0;
	cl_inittable(&row->terms, sizeof(cl_Term));
}

static void cl_multiply(struct cl_Row *row, double multiplier) {
	cl_Term *term = NULL;
	row->constant *= multiplier;
	while (cl_nextentry(&row->terms, (cl_Entry**)&term)) term->multiplier *= multiplier;
}

static void cl_addvar(struct cl_Solver *solver, struct cl_Row *row, cl_Symbol sym, double value) {
	if (!sym.id) return;
	cl_Term *term = (cl_Term*)cl_settable(&row->terms, sym);
	if (cl_nearzero(term->multiplier += value)) {
		cl_delkey(&row->terms, term);
	}
}

static void cl_addrow(struct cl_Solver *solver, struct cl_Row *row, const struct cl_Row *other, double multiplier) {
	cl_Term *term = NULL;
	row->constant += other->constant*multiplier;
	while (cl_nextentry(&other->terms, (cl_Entry**)&term))
		cl_addvar(solver, row, cl_key(term), term->multiplier*multiplier);
}

static void cl_solvefor(struct cl_Solver *solver, struct cl_Row *row, cl_Symbol entry, cl_Symbol exit) {
	cl_Term *term = (cl_Term*) cl_gettable(&row->terms, entry);
	double reciprocal = 1 / term->multiplier;
	assert(entry.id != exit.id && !cl_nearzero(term->multiplier));
	cl_delkey(&row->terms, term);
	cl_multiply(row, -reciprocal);
	if (exit.id) cl_addvar(solver, row, exit, reciprocal);
}

static void cl_substitute(struct cl_Solver *solver, struct cl_Row *row, cl_Symbol entry, const struct cl_Row *other) {
	cl_Term *term = (cl_Term*)cl_gettable(&row->terms, entry);
	if (!term) return;
	cl_delkey(&row->terms, term);
	cl_addrow(solver, row, other, term->multiplier);
}


/* variables & constraints */

static struct cl_Variable *cl_sym2var(struct cl_Solver *solver, cl_Symbol sym) {
	cl_VarEntry *ve = (cl_VarEntry*)cl_gettable(&solver->vars, sym);
	assert(ve);
	return ve->var;
}

void cl_newvariable(struct cl_Solver *solver, struct cl_Variable *var) {
	cl_Symbol sym = cl_newsymbol(solver, CL_EXTERNAL);
	cl_VarEntry *ve = (cl_VarEntry *) cl_settable(&solver->vars, sym);
	assert(!ve->var);
	memset(var, 0, sizeof(*var));
	var->sym = sym;
	var->refcount = 1;
	ve->var = var;
}

void cl_delvariable(struct cl_Solver *solver, struct cl_Variable *var) {
	if (var && --var->refcount <= 0) {
		cl_VarEntry *e = (cl_VarEntry*)cl_gettable(&solver->vars, var->sym);
		assert(e);
		cl_delkey(&solver->vars, e);
		cl_remove(solver, var->constraint);
	}
}

void cl_newconstraint(struct cl_Solver *solver, struct cl_Constraint *cons, double strength, bool equal) {
	memset(cons, 0, sizeof(*cons));
	cons->strength = cl_nearzero(strength) ? CL_REQUIRED : strength;
	cons->equal = equal;
	cl_initrow(&cons->expression);
	cl_key(cons).id = ++solver->constraint_count;
	cl_key(cons).type = CL_EXTERNAL;
	((cl_ConsEntry*)cl_settable(&solver->constraints, cl_key(cons)))->constraint = cons;
}

void cl_delconstraint(struct cl_Solver *solver, struct cl_Constraint *cons) {
	if (!cons) return;
	cl_remove(solver, cons);
	cl_ConsEntry *ce = (cl_ConsEntry*)cl_gettable(&solver->constraints, cl_key(cons));
	assert(ce);
	cl_delkey(&solver->constraints, ce);
	cl_Term *term = NULL;
	while (cl_nextentry(&cons->expression.terms, (cl_Entry**)&term)) {
		cl_delvariable(solver, cl_sym2var(solver, cl_key(term)));
	}
	cl_freerow(&cons->expression);
}

void cl_addterm(struct cl_Solver *solver, struct cl_Constraint *cons, struct cl_Variable *var, double multiplier) {
	assert(var && var->sym.id);
	assert(cons && !cons->marker.id);
	cl_addvar(solver, &cons->expression, var->sym, multiplier);
	var->refcount++;
}

void cl_addconstant(struct cl_Constraint *cons, double constant) {
	assert(cons && !cons->marker.id);
	cons->expression.constant += constant;
}

/* Cassowary algorithm */

static void cl_infeasible(struct cl_Solver *solver, struct cl_Row *row) {
	if (row->constant < 0 && row->infeasible_next.type != CL_DUMMY) {
		row->infeasible_next.id = solver->infeasible_rows.id;
		row->infeasible_next.type = CL_DUMMY;
		solver->infeasible_rows = cl_key(row);
	}
}

static void cl_markdirty(struct cl_Solver *solver, struct cl_Variable *var) {
	if (var->dirty_next.type == CL_DUMMY) return;
	var->dirty_next.id = solver->dirty_vars.id;
	var->dirty_next.type = CL_DUMMY;
	solver->dirty_vars = var->sym;
}

static void cl_substitute_rows(struct cl_Solver *solver, cl_Symbol var, struct cl_Row *expr) {
	struct cl_Row *row = NULL;
	while (cl_nextentry(&solver->rows, (cl_Entry**)&row)) {
		cl_substitute(solver, row, var, expr);
		if (cl_key(row).type == CL_EXTERNAL) {
			cl_markdirty(solver, cl_sym2var(solver, cl_key(row)));
		} else {
			cl_infeasible(solver, row);
		}
	}
	cl_substitute(solver, &solver->objective, var, expr);
}

static bool cl_takerow(struct cl_Solver *solver, cl_Symbol sym, struct cl_Row *dst) {
	struct cl_Row *row = (struct cl_Row*)cl_gettable(&solver->rows, sym);
	cl_key(dst) = cl_null();
	if (!row) return false;
	cl_delkey(&solver->rows, row);
	dst->constant = row->constant;
	dst->terms = row->terms;
	return true;
}

static void cl_putrow(struct cl_Solver *solver, cl_Symbol sym, const struct cl_Row *src) {
	struct cl_Row *row = (struct cl_Row*)cl_settable(&solver->rows, sym);
	row->constant = src->constant;
	row->terms = src->terms;
}

static void cl_mergerow(struct cl_Solver *solver, struct cl_Row *row, cl_Symbol var, double multiplier) {
	struct cl_Row *oldrow = (struct cl_Row *) cl_gettable(&solver->rows, var);
	if (oldrow) {
		cl_addrow(solver, row, oldrow, multiplier);
	} else {
		cl_addvar(solver, row, var, multiplier);
	}
}

static void cl_optimize(struct cl_Solver *solver, struct cl_Row *objective) {
	for (;;) {
		cl_Symbol enter = cl_null(), exit = cl_null();
		double r, min_ratio = INFINITY;
		struct cl_Row tmp, *row = NULL;
		cl_Term *term = NULL;

		assert(!solver->infeasible_rows.id);
		while (cl_nextentry(&objective->terms, (cl_Entry**)&term)) {
			if (cl_key(term).type != CL_DUMMY && term->multiplier < 0) {
				enter = cl_key(term);
				break;
			}
		}
		if (!enter.id) return;

		while (cl_nextentry(&solver->rows, (cl_Entry**)&row)) {
			term = (cl_Term*)cl_gettable(&row->terms, enter);
			if (!term || !cl_ispivotable(cl_key(row)) || term->multiplier > 0) continue;
			r = -row->constant / term->multiplier;
			if (r < min_ratio || (cl_nearzero(r - min_ratio) && cl_key(row).id < exit.id)) {
				min_ratio = r;
				exit = cl_key(row);
			}
		}
		assert(exit.id);

		cl_takerow(solver, exit, &tmp);
		cl_solvefor(solver, &tmp, enter, exit);
		cl_substitute_rows(solver, enter, &tmp);
		if (objective != &solver->objective)
			cl_substitute(solver, objective, enter, &tmp);
		cl_putrow(solver, enter, &tmp);
	}
}

static struct cl_Row cl_makerow(struct cl_Solver *solver, struct cl_Constraint *cons) {
	cl_Term *term = NULL;
	struct cl_Row row;
	cl_initrow(&row);
	row.constant = cons->expression.constant;
	while (cl_nextentry(&cons->expression.terms, (cl_Entry**)&term)) {
		cl_markdirty(solver, cl_sym2var(solver, cl_key(term)));
		cl_mergerow(solver, &row, cl_key(term), term->multiplier);
	}
	if (!cons->equal) {
		cl_initsymbol(solver, &cons->marker, CL_SLACK);
		cl_addvar(solver, &row, cons->marker, -1);
		if (cons->strength < CL_REQUIRED) {
			cl_initsymbol(solver, &cons->other, CL_ERROR);
			cl_addvar(solver, &row, cons->other, 1);
			cl_addvar(solver, &solver->objective, cons->other, cons->strength);
		}
	} else if (cons->strength >= CL_REQUIRED) {
		cl_initsymbol(solver, &cons->marker, CL_DUMMY);
		cl_addvar(solver, &row, cons->marker, 1);
	} else {
		cl_initsymbol(solver, &cons->marker, CL_ERROR);
		cl_initsymbol(solver, &cons->other, CL_ERROR);
		cl_addvar(solver, &row, cons->marker, -1);
		cl_addvar(solver, &row, cons->other, 1);
		cl_addvar(solver, &solver->objective, cons->marker, cons->strength);
		cl_addvar(solver, &solver->objective, cons->other, cons->strength);
	}
	if (row.constant < 0) cl_multiply(&row, -1);
	return row;
}

static void cl_remove_errors(struct cl_Solver *solver, struct cl_Constraint *cons) {
	if (cons->marker.type == CL_ERROR) {
		cl_mergerow(solver, &solver->objective, cons->marker, -cons->strength);
	}
	if (cons->other.type == CL_ERROR) {
		cl_mergerow(solver, &solver->objective, cons->other, -cons->strength);
	}
	if (cl_isconstant(&solver->objective)) {
		solver->objective.constant = 0;
	}
	cons->marker = cons->other = cl_null();
}

static int cl_add_with_artificial(struct cl_Solver *solver, struct cl_Row *row, struct cl_Constraint *cons) {
	cl_Symbol a = cl_newsymbol(solver, CL_SLACK);
	cl_Term *term = NULL;
	--solver->symbol_count; // artificial variable will be removed
	struct cl_Row tmp;
	cl_initrow(&tmp);
	cl_addrow(solver, &tmp, row, 1);
	cl_putrow(solver, a, row);
	cl_initrow(row);
	row = NULL; // row is useless
	cl_optimize(solver, &tmp);
	int ret = cl_nearzero(tmp.constant) ? CL_OK : CL_UNBOUND;
	cl_freerow(&tmp);
	if (cl_takerow(solver, a, &tmp)) {
		cl_Symbol entry = cl_null();
		if (cl_isconstant(&tmp)) {
			cl_freerow(&tmp);
			return ret;
		}
		while (cl_nextentry(&tmp.terms, (cl_Entry**)&term)) {
			if (cl_ispivotable(cl_key(term))) {
				entry = cl_key(term);
				break;
			}
		}
		if (!entry.id) {
			cl_freerow(&tmp);
			return CL_UNBOUND;
		}
		cl_solvefor(solver, &tmp, entry, a);
		cl_substitute_rows(solver, entry, &tmp);
		cl_putrow(solver, entry, &tmp);
	}
	while (cl_nextentry(&solver->rows, (cl_Entry**)&row)) {
		term = (cl_Term*)cl_gettable(&row->terms, a);
		if (term) cl_delkey(&row->terms, term);
	}
	term = (cl_Term*)cl_gettable(&solver->objective.terms, a);
	if (term) cl_delkey(&solver->objective.terms, term);
	if (ret != CL_OK) cl_remove(solver, cons);
	return ret;
}

static int cl_try_addrow(struct cl_Solver *solver, struct cl_Row *row, struct cl_Constraint *cons) {
	cl_Symbol subject = cl_null();
	cl_Term *term = NULL;
	while (cl_nextentry(&row->terms, (cl_Entry**)&term)) {
		if (cl_key(term).type == CL_EXTERNAL) {
			subject = cl_key(term);
			break;
		}
	}
	if (!subject.id && cl_ispivotable(cons->marker)) {
		cl_Term *mterm = (cl_Term*)cl_gettable(&row->terms, cons->marker);
		if (mterm->multiplier < 0) subject = cons->marker;
	}
	if (!subject.id && cl_ispivotable(cons->other)) {
		cl_Term *mterm = (cl_Term*)cl_gettable(&row->terms, cons->other);
		if (mterm->multiplier < 0) subject = cons->other;
	}
	if (!subject.id) {
		while (cl_nextentry(&row->terms, (cl_Entry**)&term))
			if (cl_key(term).type != CL_DUMMY) break;
		if (!term) {
			if (cl_nearzero(row->constant)) {
				subject = cons->marker;
			} else {
				cl_freerow(row);
				return CL_UNSATISFIED;
			}
		}
	}
	if (!subject.id) return cl_add_with_artificial(solver, row, cons);
	cl_solvefor(solver, row, subject, cl_null());
	cl_substitute_rows(solver, subject, row);
	cl_putrow(solver, subject, row);
	return CL_OK;
}

static cl_Symbol cl_get_leaving_row(struct cl_Solver *solver, cl_Symbol marker) {
	cl_Symbol first = cl_null(), second = cl_null(), third = cl_null();
	double r1 = INFINITY, r2 = INFINITY;
	struct cl_Row *row = NULL;
	while (cl_nextentry(&solver->rows, (cl_Entry**)&row)) {
		cl_Term *term = (cl_Term*)cl_gettable(&row->terms, marker);
		if (!term) continue;
		if (cl_key(row).type == CL_EXTERNAL) {
			third = cl_key(row);
		} else if (term->multiplier < 0) {
			double r = -row->constant / term->multiplier;
			if (r < r1) {
				r1 = r;
				first = cl_key(row);
			}
		} else {
			double r = row->constant / term->multiplier;
			if (r < r2) {
				r2 = r;
				second = cl_key(row);
			}
		}
	}
	return first.id ? first : second.id ? second : third;
}

static void cl_delta_edit_constant(struct cl_Solver *solver, double delta, struct cl_Constraint *cons) {
	struct cl_Row *row;
	if ((row = (struct cl_Row*)cl_gettable(&solver->rows, cons->marker))) {
		row->constant -= delta;
		cl_infeasible(solver, row);
		return;
	}
	if ((row = (struct cl_Row*)cl_gettable(&solver->rows, cons->other))) {
		row->constant += delta;
		cl_infeasible(solver, row);
		return;
	}
	while (cl_nextentry(&solver->rows, (cl_Entry**)&row)) {
		cl_Term *term = (cl_Term*)cl_gettable(&row->terms, cons->marker);
		if (!term) continue;
		row->constant += term->multiplier*delta;
		if (cl_key(row).type == CL_EXTERNAL) {
			cl_markdirty(solver, cl_sym2var(solver, cl_key(row)));
		} else {
			cl_infeasible(solver, row);
		}
	}
}

static void cl_dual_optimize(struct cl_Solver *solver) {
	while (solver->infeasible_rows.id) {
		cl_Symbol enter = cl_null();
		cl_Term *objterm, *term = NULL;
		double r, min_ratio = INFINITY;
		struct cl_Row *row = (struct cl_Row*)cl_gettable(&solver->rows, solver->infeasible_rows);
		assert(row);
		cl_Symbol leave = cl_key(row);
		solver->infeasible_rows = row->infeasible_next;
		row->infeasible_next = cl_null();
		if (cl_nearzero(row->constant) || row->constant >= 0) continue;
		while (cl_nextentry(&row->terms, (cl_Entry**)&term)) {
			cl_Symbol cur = cl_key(term);
			if (cur.type == CL_DUMMY || term->multiplier <= 0) continue;
			objterm = (cl_Term*)cl_gettable(&solver->objective.terms, cur);
			r = objterm ? objterm->multiplier / term->multiplier : 0;
			if (min_ratio > r) min_ratio = r, enter = cur;
		}
		assert(enter.id);
		struct cl_Row tmp;
		cl_takerow(solver, leave, &tmp);
		cl_solvefor(solver, &tmp, enter, leave);
		cl_substitute_rows(solver, enter, &tmp);
		cl_putrow(solver, enter, &tmp);
	}
}

void cl_newsolver(struct cl_Solver *solver) {
	memset(solver, 0, sizeof(*solver));
	cl_initrow(&solver->objective);
	cl_inittable(&solver->vars, sizeof(cl_VarEntry));
	cl_inittable(&solver->constraints, sizeof(cl_ConsEntry));
	cl_inittable(&solver->rows, sizeof(struct cl_Row));
}

void cl_delsolver(struct cl_Solver *solver) {
	cl_ConsEntry *ce = NULL;
	while (cl_nextentry(&solver->constraints, (cl_Entry**)&ce)) {
		cl_freerow(&ce->constraint->expression);
	}
	struct cl_Row *row = NULL;
	while (cl_nextentry(&solver->rows, (cl_Entry**)&row)) {
		cl_freerow(row);
	}
	cl_freerow(&solver->objective);
	cl_freetable(&solver->vars);
	cl_freetable(&solver->constraints);
	cl_freetable(&solver->rows);
}

void cl_updatevars(struct cl_Solver *solver) {
	while (solver->dirty_vars.id) {
		struct cl_Variable *var = cl_sym2var(solver, solver->dirty_vars);
		struct cl_Row *row = (struct cl_Row*)cl_gettable(&solver->rows, var->sym);
		solver->dirty_vars = var->dirty_next;
		var->dirty_next = cl_null();
		var->value = row ? row->constant : 0;
	}
}

int cl_add(struct cl_Solver *solver, struct cl_Constraint *cons) {
	assert(cons && !cons->marker.id);
	int oldsym = solver->symbol_count;
	struct cl_Row row = cl_makerow(solver, cons);
	int ret = cl_try_addrow(solver, &row, cons);
	if (ret != CL_OK) {
		cl_remove_errors(solver, cons);
		solver->symbol_count = oldsym;
	} else {
		cl_optimize(solver, &solver->objective);
		if (solver->auto_update) cl_updatevars(solver);
	}
	assert(!solver->infeasible_rows.id);
	return ret;
}

void cl_remove(struct cl_Solver *solver, struct cl_Constraint *cons) {
	if (!cons || !cons->marker.id) return;
	cl_Symbol marker = cons->marker;
	cl_remove_errors(solver, cons);
	struct cl_Row tmp;
	if (!cl_takerow(solver, marker, &tmp)) {
		cl_Symbol exit = cl_get_leaving_row(solver, marker);
		assert(exit.id);
		cl_takerow(solver, exit, &tmp);
		cl_solvefor(solver, &tmp, marker, exit);
		cl_substitute_rows(solver, marker, &tmp);
	}
	cl_freerow(&tmp);
	cl_optimize(solver, &solver->objective);
	if (solver->auto_update) cl_updatevars(solver);
}

void cl_setstrength(struct cl_Solver *solver, struct cl_Constraint *cons, double strength) {
	assert(cons);
	strength = cl_nearzero(strength) ? CL_REQUIRED : strength;
	if (cons->strength == strength) return;
	if (cons->strength >= CL_REQUIRED || strength >= CL_REQUIRED) {
		cl_remove(solver, cons);
		cons->strength = strength;
		cl_add(solver, cons);
		return;
	}
	if (cons->marker.id) {
		double diff = strength - cons->strength;
		cl_mergerow(solver, &solver->objective, cons->marker, diff);
		cl_mergerow(solver, &solver->objective, cons->other, diff);
		cl_optimize(solver, &solver->objective);
		if (solver->auto_update) cl_updatevars(solver);
	}
	cons->strength = strength;
}

void cl_addedit(struct cl_Solver *solver, struct cl_Variable *var, double strength) {
	assert(var);
	if (strength >= CL_STRONG) strength = CL_STRONG;
	if (var->constraint) {
		cl_setstrength(solver, var->constraint, strength);
		return;
	}
	assert(var->sym.id);
	struct cl_Constraint *cons = malloc(sizeof(*cons));
	cl_newconstraint(solver, cons, strength, true);
	cl_addterm(solver, cons, var, 1); /* var must be positive */
	cl_addconstant(cons, -var->value);
	assert(cl_add(solver, cons) == CL_OK);
	var->constraint = cons;
	var->edit_value = var->value;
}

void cl_deledit(struct cl_Solver *solver, struct cl_Variable *var) {
	if (!var || !var->constraint) return;
	cl_delconstraint(solver, var->constraint);
	var->constraint = NULL;
	var->edit_value = 0;
}

void cl_suggest(struct cl_Solver *solver, struct cl_Variable *var, double value) {
	assert(var);
	if (!var->constraint) {
		cl_addedit(solver, var, CL_STRONG);
		assert(var->constraint);
	}
	double delta = value - var->edit_value;
	var->edit_value = value;
	cl_delta_edit_constant(solver, delta, var->constraint);
	cl_dual_optimize(solver);
	if (solver->auto_update) cl_updatevars(solver);
}









void cl_dumpkey(cl_Symbol sym) {
	printf("%c%u", "vsed"[sym.type], sym.id);
}

void cl_dumprow(struct cl_Row *row) {
	cl_Term *term = NULL;
	printf("%g", row->constant);
	while (cl_nextentry(&row->terms, (cl_Entry**)&term)) {
		printf(" %c ", signbit(term->multiplier) ? '-' : '+');
		if (!cl_nearzero(fabs(term->multiplier) - 1)) printf("%g*", fabs(term->multiplier));
		cl_dumpkey(cl_key(term));
	}
	putchar('\n');
}

void cl_dumpsolver(struct cl_Solver *solver) {
	struct cl_Row *row = NULL;
	printf("Solver: ");
	cl_dumprow(&solver->objective);
	printf("Rows(%d):\n", (int)solver->rows.count);
	while (cl_nextentry(&solver->rows, (cl_Entry**)&row)) {
		cl_dumpkey(cl_key(row));
		printf(" = ");
		cl_dumprow(row);
	}
}

int main() {
	struct cl_Solver S; cl_newsolver(&S);

	struct cl_Variable x1; cl_newvariable(&S, &x1);
	struct cl_Variable xm; cl_newvariable(&S, &xm);
	struct cl_Variable x2; cl_newvariable(&S, &x2);

	// c3: x1 >= 0
	struct cl_Constraint c3; cl_newconstraint(&S, &c3, CL_REQUIRED, false);
	cl_addterm(&S, &c3, &x1, 1);
	cl_add(&S, &c3);

	// c4: x2 <= 100
	struct cl_Constraint c4; cl_newconstraint(&S, &c4, CL_REQUIRED, false);
	cl_addterm(&S, &c4, &x2, -1);
	cl_addconstant(&c4, 100);
	cl_add(&S, &c4);

	// c2: x2 >= x1 + 20
	struct cl_Constraint c2; cl_newconstraint(&S, &c2, CL_REQUIRED, false);
	cl_addterm(&S, &c2, &x2, 1);
	cl_addterm(&S, &c2, &x1, -1);
	cl_addconstant(&c2, -20);
	cl_add(&S, &c2);

	// c1: xm is in middle of x1 and x2:
	// i.e. xm = (x1 + x2) / 2, or 2*xm = x1 + x2
	struct cl_Constraint c1; cl_newconstraint(&S, &c1, CL_REQUIRED, true);
	cl_addterm(&S, &c1, &xm, -2);
	cl_addterm(&S, &c1, &x1, 1);
	cl_addterm(&S, &c1, &x2, 1);
	cl_add(&S, &c1);

	// c5: x1 == 40
	struct cl_Constraint c5; cl_newconstraint(&S, &c5, CL_WEAK, true);
	cl_addterm(&S, &c5, &x1, -1);
	cl_addconstant(&c5, 40);
	cl_add(&S, &c5);

	cl_suggest(&S, &xm, 60);
	cl_updatevars(&S);
	cl_dumpsolver(&S);
	printf("%g, %g, %g\n\n", x1.value, xm.value, x2.value);

	cl_suggest(&S, &xm, 90);
	cl_updatevars(&S);
	cl_dumpsolver(&S);
	printf("%g, %g, %g\n", x1.value, xm.value, x2.value);

	cl_delsolver(&S);
}
