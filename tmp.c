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

struct {
	int spec_offset; // number of pen edges between h and the initial offset
	struct mp_knot *spec_p1, *spec_p2, *path_tail;
} mpd, *mp = &mpd;

void mp_remove_cubic(struct mp_knot *p) {
	struct mp_knot *q = mp_next_knot(p);
	mp_next_knot(p) = mp_next_knot(q);
	mp_right_x(p) = mp_right_x(q);
	mp_right_y(p) = mp_right_y(q);
	//free(q);
}
static struct mp_knot *mp_htap_ypoc(struct mp_knot *p) {
	struct mp_knot *q = malloc(sizeof(*q));
	struct mp_knot *pp = p, *qq = q, *rr;
	for (;;) {
		mp_right_type(qq) = mp_left_type(pp);
		mp_left_type(qq) = mp_right_type(pp);
		mp_x_coord(qq) = mp_x_coord(pp);
		mp_y_coord(qq) = mp_y_coord(pp);
		mp_right_x(qq) = mp_left_x(pp);
		mp_right_y(qq) = mp_left_y(pp);
		mp_left_x(qq) = mp_right_x(pp);
		mp_left_y(qq) = mp_right_y(pp);
		if (mp_next_knot(pp) == p) {
			mp_next_knot(q) = qq;
			mp->path_tail = pp;
			return q;
		}
		rr = malloc(sizeof(*rr));
		mp_next_knot(rr) = qq;
		qq = rr;
		pp = mp_next_knot(pp);
	}
}
struct mp_knot *mp_insert_knot(struct mp_knot *q, double x, double y) {
	struct mp_knot *r = malloc(sizeof(*r));
	mp_next_knot(r) = mp_next_knot(q);
	mp_next_knot(q) = r;
	mp_right_x(r) = mp_right_x(q);
	mp_right_y(r) = mp_right_y(q);
	mp_x_coord(r) = x;
	mp_y_coord(r) = y;
	mp_right_x(q) = mp_x_coord(q);
	mp_right_y(q) = mp_y_coord(q);
	mp_left_x(r) = mp_x_coord(r);
	mp_left_y(r) = mp_y_coord(r);
	mp_left_type(r) = mp_explicit;
	mp_right_type(r) = mp_explicit;
	return r;
}

// Walk k steps around a pen from w.
struct mp_knot *mp_pen_walk(struct mp_knot *w, int k) {
	if (k > 0) while (k > 0) {
		w = mp_next_knot(w);
		k--;
	} else while (k < 0) {
		w = mp_prev_knot(w);
		k++;
	}
	return w;
}

// Decide how many pen offsets to go away from w in order to find the offset for dz.
// w is assumed to be the offset for some direction from which the angle to dz in the sense determined by ccw is less than or equal to 180°.
int mp_get_turn_amt(struct mp_knot *w, double complex dz, bool clockwise) {
	int s = 0;
	// If the pen polygon has only two edges, they could both be parallel to dz. Care must be taken to stop after crossing the first such edge to avoid an infinite loop.
	if (clockwise) {
		struct mp_knot *ww = mp_prev_knot(w);
		while (cimag(dz * conj(ww->coord - w->coord)) > 0) {
			s--;
			w = ww;
			ww = mp_prev_knot(ww);
		}
	} else {
		struct mp_knot *ww = mp_next_knot(w);
		while (cimag(dz * conj(ww->coord - w->coord)) > 0) {
			s++;
			w = ww;
			ww = mp_next_knot(ww);
		}
		if (cimag(dz * conj(ww->coord - w->coord)) == 0) s++;
	}
	return s;
}

void mp_fin_offset_prep(struct mp_knot *p, struct mp_knot *w, double complex z0, double complex z1, double complex z2, int rise, int turn_amt) {
	struct mp_knot *q = mp_next_knot(p);
	for (;;) {
		struct mp_knot *ww = rise > 0 ? mp_next_knot(w) : mp_prev_knot(w);
		double complex dw = ww->coord - w->coord;
		double t1 = cimag(conj(z1) * dw);
		double t2 = cimag(conj(z2) * dw);
		double t = mp_crossing_point(fmax(0, cimag(conj(z0) * dw)), t1, t2);
		if (t >= 1 && turn_amt <= 0) return;

		mp_split_cubic(p, fmin(1, t));
		p = p->next;
		mp_knot_info(p) = rise;
		turn_amt--;
		double complex v = t_of_the_way(z0, z1);
		z1 = t_of_the_way(z1, z2);
		z0 = t_of_the_way(v, z1);
		if (turn_amt < 0) {
			t1 = fmin(0, t_of_the_way(t1, t2));
			t = fmin(1, mp_crossing_point(0, -t1, -t2));
			turn_amt++;
			if (t == 1 && p->next != q) {
				mp_knot_info(p->next) -= rise;
			} else {
				mp_split_cubic(p, t);
				mp_knot_info(p->next) = -rise;
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
static struct mp_knot *mp_offset_prep(struct mp_knot *c, struct mp_knot *vertices, int vertex_count) {
	struct mp_knot *p = vertices, *q, *r, *w;

	vertex_count = 0;
	do {
		vertex_count++;
		p = p->next;
	} while (p != vertices);

	double complex dzin = p->next->coord - mp_prev_knot(p)->coord;
	double complex dz0;
	if (!dzin) dzin = I * (p->coord - mp_prev_knot(p)->coord);
	struct mp_knot *w0 = vertices, *c0 = c;
	p = c;
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

		int turn_amt = mp_get_turn_amt(w0, dz, cimag(dzin * conj(dz)) > 0);
		w = mp_pen_walk(w0, turn_amt);
		w0 = w;
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
		turn_amt = mp_get_turn_amt(w, dzin, d_sign < 0);
		if (creal(conj(z0 + z2) * t_of_the_way(u0, u1)) < 0) turn_amt -= d_sign * vertex_count;

		struct mp_knot *ww = mp_prev_knot(w);

		double complex dw = ww->coord - w->coord;
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
			mp_fin_offset_prep(p, w, z0, z1, z2, 1, turn_amt);
		} else {
			mp_split_cubic(p, t);
			r = mp_next_knot(p);
			double complex z1a = t_of_the_way(z0, z1);
			z1 = t_of_the_way(z1, z2);
			double complex z2a = t_of_the_way(z1a, z1);
			mp_fin_offset_prep(p, w, z0, z1a, z2a, 1, 0);
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
				mp_fin_offset_prep(r->next, w, z0a, z1a, z2, 1, turn_amt);
				z2 = z0a;
				mp_fin_offset_prep(r, ww, z0, z1, z2, -1, 0);
			} else {
				mp_fin_offset_prep(r, ww, z0, z1, z2, -1, -1 - turn_amt);
			}
		}
		w0 = mp_pen_walk(w0, turn_amt);
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
				if (r == mp->spec_p1) mp->spec_p1 = p;
				if (r == mp->spec_p2) mp->spec_p2 = p;
printf("@@@@@%d\n",__LINE__);
				r = p;
				// Remove the cubic following p.
				mp_remove_cubic(p);
			}
			p = r;
		} while (p != q);
		// Be careful not to remove the only cubic in a cycle.
		if (q != q0 && (q != c || c == c0)) q = q->next;
	} while (q != c);

	// When we're all done, the final offset is w0 and the final curve direction is dzin.
	// With this knowledge of the incoming direction at c, we can correct mp_info(c) which was erroneously based on an
	// incoming offset of the first vertex.
	mp->spec_offset = mp_knot_info(c);
	if (c->next == c) {
		mp_knot_info(c) = vertex_count;
	} else {
		mp_knot_info(c) += k_needed;
		while (w0 != vertices) {
			mp_knot_info(c)++;
			w0 = w0->next;
		}
		while (mp_knot_info(c) <= -vertex_count) mp_knot_info(c) += vertex_count;
		while (mp_knot_info(c) > 0) mp_knot_info(c) -= vertex_count;
		if (mp_knot_info(c) && cimag(dzin * conj(dz0)) <= 0) mp_knot_info(c) += vertex_count;
	}
	return c;
}





void mp_print_spec(struct mp_knot *cur_spec, struct mp_knot *cur_pen) {
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


struct mp_knot *mp_make_envelope(struct mp_knot *c, struct mp_knot *h, int ljoin, int lcap, double miterlim) {
	struct mp_knot *p, *q, *r, *q0;
	struct mp_knot *w, *w0;
	double qx, qy;
	int k, k0;
	int join_type = 0;
	double complex dzin = 0, dzout = 0;
	double tmp;
	double det;
	double ht_x, ht_y;
	double max_ht;
	mp->spec_p1 = NULL;
	mp->spec_p2 = NULL;

	if (mp_left_type(c) == mp_endpoint) {
		mp->spec_p1 = mp_htap_ypoc(c);
		mp->spec_p2 = mp->path_tail;
		mp->spec_p2->next = mp->spec_p1->next;
		mp->spec_p1->next = c;
		mp_remove_cubic(mp->spec_p1);
		c = mp->spec_p1;
		if (c != c->next) {
			mp_remove_cubic(mp->spec_p2);
		} else {
			mp_left_type(c) = mp_explicit;
			mp_right_type(c) = mp_explicit;
printf("@@@@@%d\n",__LINE__);
			mp_left_x(c) = mp_x_coord(c);
			mp_left_y(c) = mp_y_coord(c);
			mp_right_x(c) = mp_x_coord(c);
			mp_right_y(c) = mp_y_coord(c);
		}
	}

	c = mp_offset_prep(c, h, 0);
//mp_print_spec(c, h);
	h = mp_pen_walk(h, mp->spec_offset);
	w = h;
	p = c;
	do {
		q = p->next;
		q0 = q;
		qx = mp_x_coord(q);
		qy = mp_y_coord(q);
		k = mp_knot_info(q);
		k0 = k;
		w0 = w;
		if (k) {
			if (k < 0) {
				join_type = 2;
			} else {
				if ((q != mp->spec_p1) && (q != mp->spec_p2))
					join_type = ljoin;
				else if (lcap == 2)
					join_type = 3;
				else
					join_type = 2 - lcap;
				if (join_type == 0 || join_type == 3) {
					dzin = q->coord - q->left.coord;
					if (!dzin) {
						dzin = q->coord - p->right.coord;
printf("@@@@@%d\n",__LINE__);
						if (!dzin) {
							dzin = q->coord - p->coord;
printf("@@@@@%d\n",__LINE__);
							if (p != c) {
								dzin += w->coord;
printf("@@@@@%d\n",__LINE__);
							}
						}
					}
					tmp = cabs(dzin);
					if (!tmp) {
						join_type = 2;
printf("@@@@@%d\n",__LINE__);
					} else {
						dzin /= tmp;

						dzout = q->right.coord - q->coord;
						if (!dzout) {
							r = q->next;
							dzout = r->left.coord - q->coord;
printf("@@@@@%d\n",__LINE__);
							if (!dzout) {
								dzout = r->coord - q->coord;
printf("@@@@@%d\n",__LINE__);
							}
						}
						if (q == c) {
							dzout -= -h->coord;
printf("@@@@@%d\n",__LINE__);
						}
						tmp = cabs(dzout);
						assert(tmp); // tmp = 0 → degenerate spec
						dzout /= tmp;
printf("@@@@@%d\n",__LINE__);
					}
					if (join_type == 0) {
						tmp = miterlim * (creal(dzin * conj(dzout)) + 1) / 2;
						if (tmp < 1 && miterlim * tmp < 1) join_type = 2;
printf("@@@@@%d\n",__LINE__);
					}
				}
			}
		}

		p->right.coord += w->coord;
		q->left.coord += w->coord;
		q->coord += w->coord;
		q->left.type = q->right.type = mp_explicit;
		while (k) {
			if (k > 0) {
				w = w->next;
				k--;
			} else {
				w = mp_prev_knot(w);
				k++;
			}
			if (join_type == 1 || !k)
				q = mp_insert_knot(q, qx + mp_x_coord(w), qy + mp_y_coord(w));
		}
		if (q != p->next) {
			p = p->next;
			if (join_type == 0 || join_type == 3) {
				if (join_type == 0) {
					det = cimag(conj(dzin) * dzout);
printf("@@@@@%d\n",__LINE__);
					if (fabs(det) < 1e-4) {
						r = NULL;
printf("@@@@@%d\n",__LINE__);
					} else {
						tmp = cimag(conj(q->coord - p->coord) * dzout) / det;
printf("@@@@@%d\n",__LINE__);
						r = mp_insert_knot(p, mp_x_coord(p) + tmp * creal(dzin), mp_y_coord(p) + tmp * cimag(dzin));
					}
				} else {
					ht_x = mp_y_coord(w) - mp_y_coord(w0);
					ht_y = mp_x_coord(w0) - mp_x_coord(w);

printf("@@@@@%d\n",__LINE__);
					max_ht = 0;
					struct mp_knot *ww = w;
					for (int kk = 0;;) {
						if (kk > k0) {
							ww = ww->next;
							kk--;
printf("@@@@@%d\n",__LINE__);
						} else {
							ww = mp_prev_knot(ww);
printf("@@@@@%d\n",__LINE__);
							kk++;
						}
						if (kk == k0) break;
						tmp = (mp_x_coord(ww) - mp_x_coord(w0)) * ht_x + (mp_y_coord(ww) - mp_y_coord(w0)) * ht_y;
printf("@@@@@%d\n",__LINE__);
						if (tmp > max_ht) max_ht = tmp;
					}
					tmp = mp_make_fraction(max_ht, mp_take_fraction(creal(dzin), ht_x) + mp_take_fraction(cimag(dzin), ht_y));
					r = mp_insert_knot(p, mp_x_coord(p) + tmp * creal(dzin), mp_y_coord(p) + tmp * cimag(dzin));
printf("@@@@@%d\n",__LINE__);
					tmp = mp_make_fraction(max_ht, mp_take_fraction(creal(dzout), ht_x) + mp_take_fraction(cimag(dzout), ht_y));
					r = mp_insert_knot(r, mp_x_coord(q) + tmp * creal(dzout), mp_y_coord(q) + tmp * creal(dzout));
				}
				if (r) r->right.coord = r->coord;
			}
		}
		p = q;
	} while (q0 != c);
	return c;
}
