#define mp_x_coord(A)   (((double*)&((A)->coord))[0])
#define mp_y_coord(A)   (((double*)&((A)->coord))[1])
#define mp_left_x(A)    (((double*)&((A)->left.coord))[0])
#define mp_left_y(A)    (((double*)&((A)->left.coord))[1])
#define mp_right_x(A)   (((double*)&((A)->right.coord))[0])
#define mp_right_y(A)   (((double*)&((A)->right.coord))[1])
#define mp_next_knot(A)     (A)->next
#define mp_left_type(A)     (A)->left.type
#define mp_right_type(A)     (A)->right.type
#define mp_prev_knot(A)     (A)->prev
#define mp_knot_info(A)     (A)->info
#define left_curl mp_left_x
#define left_given mp_left_x
#define left_tension mp_left_y
#define right_curl mp_right_x
#define right_given mp_right_x
#define right_tension mp_right_y
#define incr(A) (A) = (A) + 1
#define decr(A) (A) = (A) - 1
#define negate(A) (A) = -(A)
#define double(A) (A) = (A) + (A)
#define odd(A) ((A) % 2 == 1)
#define mp_make_fraction(a, b) ((a) / (b))
#define mp_take_fraction(a, b) ((a) * (b))
#define unity 1.0
#define two 2.0
#define three 3.0
#define half_unit .5
#define three_quarter_unit .75
#define fraction_one 1.0
#define fraction_half .5
#define fraction_two 2.0
#define fraction_three 3.0
#define fraction_four 4.0

#define ninety_deg (asin(1))
#define one_eighty_deg (acos(-1))
#define three_sixty_deg (2 * acos(-1))

static int spec_offset; // number of pen edges between h and the initial offset

// The C operator / on integers truncates towards zero (quot) since C99.
// quot: truncated → 0; (a quot b) × b + (a rem b) = a
// div: truncated → −∞; (a div b) × b + (a mod b) = a
#define mod(a, b) (((a) % (b) + (b)) % (b))

// Decide how many pen offsets to go away from w in order to find the offset for dz.
// w is assumed to be the offset for some direction from which the angle to dz in the sense determined by ccw is less than or equal to 180°.
int mp_get_turn_amt(double complex vertices[], int vertex_count, int w, double complex dz, bool clockwise) {
	// If the pen polygon has only two edges, they could both be parallel to dz. Care must be taken to stop after crossing the first such edge to avoid an infinite loop.
	for (int s = 0, ww = w; ; s += clockwise ? -1 : 1, w = ww) {
		ww = (ww + vertex_count + (clockwise ? -1 : 1)) % vertex_count;
		if (cimag(dz * conj(vertices[ww] - vertices[w])) <= 0) {
			return s + (!cimag(dz * conj(vertices[ww] - vertices[w])) && !clockwise);
		}
	}
}

void mp_fin_offset_prep(struct mp_knot *p, double complex vertices[], int vertex_count, int w, double complex z0, double complex z1, double complex z2, bool rise, int turn_amt) {
	struct mp_knot *q = mp_next_knot(p);
	for (;;) {
		int ww = mod(w + (rise ? 1 : -1), vertex_count);
		double complex dw = vertices[ww] - vertices[w];
		double t1 = cimag(conj(z1) * dw);
		double t2 = cimag(conj(z2) * dw);
		double t = mp_crossing_point(fmax(0, cimag(conj(z0) * dw)), t1, t2);
		if (t >= 1 && turn_amt <= 0) return;

		mp_split_cubic(p, fmin(1, t));
		p = p->next;
		mp_knot_info(p) = rise ? 1 : -1;
		turn_amt--;
		double complex v = t_of_the_way(z0, z1);
		z1 = t_of_the_way(z1, z2);
		z0 = t_of_the_way(v, z1);
		if (turn_amt < 0) {
			t1 = fmin(0, t_of_the_way(t1, t2));
			t = fmin(1, mp_crossing_point(0, -t1, -t2));
			turn_amt++;
			if (t == 1 && p->next != q) {
				mp_knot_info(p->next) += rise ? -1 : 1;
			} else {
				mp_split_cubic(p, t);
				mp_knot_info(p->next) = rise ? -1 : 1;
				v = t_of_the_way(z1, z2);
				z1 = t_of_the_way(z0, z1);
				z2 = t_of_the_way(z1, v);
			}
		}
		w = ww;
	}
}

// Given a pointer c to a cyclic path, and an array of vertices of a pen polygon, the mp_offset_prep routine changes the path into cubics that are associated with particular pen offsets.
// Thus if the cubic between p and q is associated with the kth offset and the cubic between q and r has offset l then mp_info(q) = l − k.
// After overwriting the type information with offset differences, we no longer have a true path so we refer to the knot list returned by offset prep as an “envelope spec.” Since an envelope spec only determines relative changes in pen offsets, mp_offset_prep sets a global variable mp->spec_offset to the relative change from the first vertex to the first offset.
static struct mp_knot *mp_offset_prep(struct mp_knot *c, double complex vertices[], int vertex_count) {
	struct mp_knot *p = c, *q, *r, *c0 = c;
	int w, w0 = 0; // indices into vertices
	double complex dz0, dzin = vertices[1] - vertices[vertex_count - 1];
	if (!dzin) dzin = I * (vertices[0] - vertices[vertex_count - 1]);
	int k_needed = 0;
	do {
		q = p->next;

		mp_knot_info(p) = k_needed;
		k_needed = 0;

		double complex z0 = p->right.coord - p->coord;
		double complex z1 = q->left.coord - p->right.coord;
		double complex z2 = q->coord - q->left.coord;
		if (!z0 && !z1 && !z2) goto NOT_FOUND;

		double complex dz = z0 ? z0 : z1 ? z1 : z2;
		if (p == c) dz0 = dz;

		int turn_amt = mp_get_turn_amt(vertices, vertex_count, w0, dz, cimag(dzin * conj(dz)) > 0);
		w = w0 = mod(w0 + turn_amt, vertex_count);
		mp_knot_info(p) += turn_amt;
		dzin = z2 ? z2 : z1 ? z1 : z0;

		int d_sign = cimag(dzin * conj(dz));
		d_sign = (d_sign > 0) - (d_sign < 0);
		if (!d_sign) {
			// We check rotation direction by looking at the vector connecting the current node with the next.
			// If its angle with incoming and outgoing tangents has the same sign, we pick this as d_sign, since it means we have a flex, not a cusp. Otherwise we proceed to the cusp code.
			double complex tmp0 = q->coord - p->coord;
			double tmp1 = cimag(conj(dz) * tmp0);
			double tmp2 = cimag(dzin * conj(tmp0));
			d_sign = ((tmp1 > 0) - (tmp1 < 0) + (tmp2 > 0) - (tmp2 < 0)) / 2;
printf("@@@@@%d\n",__LINE__);
		}
		// If the cubic almost has a cusp, it is a numerically ill-conditioned problem to decide which way it loops around but that's OK as long we're consistent.
		if (!d_sign) d_sign = creal(dz) ? (creal(dz) > 0) - (creal(dz) < 0) : (cimag(dz) > 0) - (cimag(dz) < 0);

		// Make ss negative if and only if the total change in direction is more than 180°.
		double t0 = cimag(conj(z0) * z2) / 2;
		double t1 = cimag(conj(z1) * (z0 + z2)) / 2;
		if (!t0) t0 = d_sign; // Path reversal always negates d_sign.
		double t = mp_crossing_point(fabs(t0), t1, -fabs(t0));
		double complex u0 = t_of_the_way(t > 0 ? z0 : z2, z1);
		double complex u1 = t_of_the_way(z1, t > 0 ? z2 : z0);
		// To make stroke envelopes work properly, reversing the path should always change the sign of turn_amt.
		turn_amt = mp_get_turn_amt(vertices, vertex_count, w, dzin, d_sign < 0);
		if (creal(conj(z0 + z2) * t_of_the_way(u0, u1)) < 0) turn_amt -= d_sign * vertex_count;

		int ww = mod(w - 1, vertex_count);

		double complex dw = vertices[ww] - vertices[w];
		t1 = cimag(conj(z1) * dw);
		double t2 = cimag(conj(z2) * dw);
		t = mp_crossing_point(fmax(0, cimag(conj(z0) * dw)), t1, t2);
		// At this point, the direction of the incoming pen edge is -dw. When the component of d(t) perpendicular to -dw crosses zero, we need to decide whether the directions are parallel or antiparallel.
		// We can test this by finding the dot product of d(t) and -dw, but this should be avoided when the value of turn_amt already determines the answer.
		// If t2 < 0, there is one crossing and it is antiparallel only if turn_amt ≥ 0.
		// If turn_amt < 0, there should always be at least one crossing and the first crossing cannot be antiparallel.
		if (turn_amt >= 0) {
			if (t2 < 0 || creal(conj(dw) * t_of_the_way(t_of_the_way(z0, z1), t_of_the_way(z1, z2))) > 0) {
				t = nextafter(1, INFINITY);
			}
		} else if (t > 1) {
			t = 1;
		}
		// Split the cubic into at most three parts with respect to d[k−1] and apply mp_fin_offset_prep to each part.
		if (t > 1) {
			mp_fin_offset_prep(p, vertices, vertex_count, w, z0, z1, z2, true, turn_amt);
		} else {
			mp_split_cubic(p, t);
			r = mp_next_knot(p);
			double complex z1a = t_of_the_way(z0, z1);
			z1 = t_of_the_way(z1, z2);
			double complex z2a = t_of_the_way(z1a, z1);
			mp_fin_offset_prep(p, vertices, vertex_count, w, z0, z1a, z2a, true, 0);
			z0 = z2a;
			mp_knot_info(r) = -1;
			if (turn_amt >= 0) {
				t1 = fmin(0, t_of_the_way(t1, t2));
				t = fmin(1, mp_crossing_point(0, -t1, -t2));

				mp_split_cubic(r, t);
				mp_knot_info(r->next) = 1;
				double complex z1a = t_of_the_way(z1, z2);
				z1 = t_of_the_way(z0, z1);
				double complex z0a = t_of_the_way(z1, z1a);
				mp_fin_offset_prep(r->next, vertices, vertex_count, w, z0a, z1a, z2, true, turn_amt);
				z2 = z0a;
				mp_fin_offset_prep(r, vertices, vertex_count, ww, z0, z1, z2, false, 0);
			} else {
				mp_fin_offset_prep(r, vertices, vertex_count, ww, z0, z1, z2, false, -1 - turn_amt);
			}
		}
		w0 = mod(w0 + turn_amt, vertex_count);
NOT_FOUND:;

		struct mp_knot *q0 = q;
		do {
			r = p->next;
			// Remove any “dead” cubics that might have been introduced by the splitting process.
			if (p->coord == p->right.coord && p->coord == r->left.coord && p->coord == r->coord && r != p) {
				// Update the data structures to merge r into p.
				k_needed = mp_knot_info(p);
				if (r == q) {
					q = p;
printf("@@@@@%d\n",__LINE__);
				} else {
					mp_knot_info(p) = k_needed + mp_knot_info(r);
					k_needed = 0;
printf("@@@@@%d\n",__LINE__);
				}
				if (r == c) {
					mp_knot_info(p) = mp_knot_info(c);
					c = p;
printf("@@@@@%d\n",__LINE__);
				}
printf("@@@@@%d\n",__LINE__);
				r = p;
				// Remove the cubic following p.
				p->right = p->next->right;
				p->next = p->next->next;
			}
			p = r;
		} while (p != q);
		// Be careful not to remove the only cubic in a cycle.
		if (q != q0 && (q != c || c == c0)) q = q->next;
	} while (q != c);

	// When we're all done, the final offset is w0 and the final curve direction is dzin.
	// With this knowledge of the incoming direction at c, we can correct mp_info(c) which was erroneously based on an
	// incoming offset of the first vertex.
	spec_offset = mp_knot_info(c);
	if (c->next == c) {
		mp_knot_info(c) = vertex_count;
	} else {
		mp_knot_info(c) = mod(mp_knot_info(c) + k_needed - w0, vertex_count);
		if (mp_knot_info(c) && cimag(dzin * conj(dz0)) > 0) mp_knot_info(c) -= vertex_count;
	}
	return c;
}





/*void mp_print_spec(struct mp_knot *cur_spec, struct mp_knot *cur_pen) {
	struct mp_knot *p = cur_spec, *q, *w = mp_pen_walk(cur_pen, mp->spec_offset);
	printf("Envelope spec\n(%g, %g) %% beginning with offset (%g, %g)", mp_x_coord(cur_spec), mp_y_coord(cur_spec), mp_x_coord(w), mp_y_coord(w));
	do {
		do {
			q = p->next;
			printf("\n .. controls (%g, %g) and (%g, %g)\n .. (%g, %g)", creal(p->right.coord), cimag(p->right.coord), creal(q->left.coord), cimag(q->left.coord), creal(q->coord), cimag(q->coord));
			p = q;
		} while (p != cur_spec && mp_knot_info(p));
		if (mp_knot_info(p)) {
			w = mp_pen_walk(w, mp_knot_info(p));
			printf(" %% ");
			if (mp_knot_info(p) > 0) printf("counter");
			printf("clockwise to offset (%g, %g)", creal(w->coord), cimag(w->coord));
		}
	} while (p != cur_spec);
	puts("\n & cycle");
}
*/

struct mp_knot *mp_make_envelope(struct mp_knot *c, double complex *vertices, int vertex_count) {
	int n = 0;
	struct mp_knot *p = c;
	do {
		n++;
		p = p->next;
	} while (p != c);
	struct mp_knot *knots = malloc(n * vertex_count * 2 * sizeof(struct mp_knot));
	int allocated_knot_count = 0;
	#define new_knot (&knots[allocated_knot_count++])
	// Make a copy of the path c.
	do {
		struct mp_knot *pp = new_knot;
		*pp = *p;
		pp->next = pp + 1;
		p = p->next;
	} while (p != c);
	if (c->left.type == mp_endpoint) {
		// Do doublepath c.
		// p1 and p2 corresponds to global variables mp->spec_p1 and mp->spec_p2 in MetaPost source.
		//     knots → knots->next → … → p2
		//     ↑                          ↓
		//     p1 ← p1+1 ←   …   ← p2->next
		struct mp_knot *p1 = &knots[allocated_knot_count], *p2 = p1 - 1;
		do {
			struct mp_knot *pp = new_knot;
			*pp = *p;
			pp->next = pp + 1;
			p = p->next;
		} while (p != c);
		knots[allocated_knot_count - 1].next = p1;
		p2->next = mp_reverse_path(p1);
		p1[1].next = knots;
		knots->left = p1->left;
		if (knots == knots->next) {
			// Make the path look like a cycle of length one.
			knots->left.type = knots->right.type = mp_explicit;
			knots->left.coord = knots->right.coord = knots->coord;
		} else {
			p2->right = p2->next->right;
			p2->next = p2->next->next;
		}
	} else {
		knots[allocated_knot_count - 1].next = knots;
	}
	p = c = knots;

	mp_offset_prep(c, vertices, vertex_count);
	int h = mod(spec_offset, vertex_count);
//mp_print_spec(c, h);
	struct mp_knot *q0;
	int w = h;
	do {
		struct mp_knot *q = p->next;
		q0 = q;
		double complex qc = q->coord;
		int k = mp_knot_info(q);

		p->right.coord += vertices[w];
		q->left.type = q->right.type = mp_explicit;
		q->left.coord += vertices[w];
		q->coord += vertices[w];
		while (k) {
			if (k > 0) {
				w = (w + 1) % vertex_count;
				k--;
			} else {
				w = (w + vertex_count - 1) % vertex_count;
				k++;
			}
			struct mp_knot *r = new_knot;
			r->next = q->next;
			q->next = r;
			r->right = q->right;
			r->coord = qc + vertices[w];
			q->right.coord = q->coord;
			r->left.type = mp_explicit;
			r->left.coord = r->coord;
			q = r;
		}
		p = q;
	} while (q0 != c);
	return c;
}
