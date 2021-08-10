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

int mp_get_turn_amt(struct mp_knot *w, double dx, double dy, bool ccw) {
	int s = 0;
	if (ccw) {
		struct mp_knot *ww = mp_next_knot(w);
		int t;
		do {
			t = dy * (mp_x_coord(ww) - mp_x_coord(w)) - dx * (mp_y_coord(ww) - mp_y_coord(w));
			if (t < 0) break;
			s++;
			w = ww;
			ww = mp_next_knot(ww);
		} while (t > 0);
	} else {
		struct mp_knot *ww = mp_prev_knot(w);
		while (dy * (mp_x_coord(w) - mp_x_coord(ww)) < dx * (mp_y_coord(w) - mp_y_coord(ww))) {
			s--;
			w = ww;
			ww = mp_prev_knot(ww);
		}
	}
	return s;
}

void mp_fin_offset_prep(struct mp_knot *p, struct mp_knot *w, double x0, double x1, double x2, double y0, double y1, double y2, int rise, int turn_amt) {
	struct mp_knot *ww;
	double du, dv, t, s, v;
	double t0, t1, t2;
	struct mp_knot *q = mp_next_knot(p);
	for (;;) {
		ww = rise > 0 ? mp_next_knot(w) : mp_prev_knot(w);
		du = mp_x_coord(ww) - mp_x_coord(w);
		dv = mp_y_coord(ww) - mp_y_coord(w);
		if (fabs(du) >= fabs(dv)) {
			s = dv / du;
			t0 = x0 * s - y0;
			t1 = x1 * s - y1;
			t2 = x2 * s - y2;
			if (du < 0) {
				t0 = -t0;
				t1 = -t1;
				t2 = -t2;
			}
		} else {
			s = du / dv;
			t0 = x0 - y0 * s;
			t1 = x1 - y1 * s;
			t2 = x2 - y2 * s;
			if (dv < 0) {
				t0 = -t0;
				t1 = -t1;
				t2 = -t2;
			}
		}
		t0 = fmax(0, t0);
		t = mp_crossing_point(t0, t1, t2);
		if (t >= 1) {
			if (turn_amt <= 0) return;
			t = 1;
		}

		mp_split_cubic(p, t);
		p = p->next;
		mp_knot_info(p) = rise;
		turn_amt--;
		v = t_of_the_way(x0, x1);
		x1 = t_of_the_way(x1, x2);
		x0 = t_of_the_way(v, x1);
		v = t_of_the_way(y0, y1);
		y1 = t_of_the_way(y1, y2);
		y0 = t_of_the_way(v, y1);
		if (turn_amt < 0) {
			t1 = fmin(0, t_of_the_way(t1, t2));
			t = fmin(1, mp_crossing_point(0, -t1, -t2));
			turn_amt++;
			if (t == 1 && p->next != q) {
				mp_knot_info(p->next) -= rise;
			} else {
				mp_split_cubic(p, t);
				mp_knot_info(mp_next_knot(p)) = -rise;
				v = t_of_the_way(x1, x2);
				x1 = t_of_the_way(x0, x1);
				x2 = t_of_the_way(x1, v);
				v = t_of_the_way(y1, y2);
				y1 = t_of_the_way(y0, y1);
				y2 = t_of_the_way(y1, v);
			}
		}
		w = ww;
	}
}
static struct mp_knot *mp_offset_prep(struct mp_knot *c, struct mp_knot *h) {
	int n = 0;
	struct mp_knot *p = h, *q, *q0, *r, *w, *ww;
	int turn_amt;

	double x0, x1, x2, y0, y1, y2;
	double t0, t1, t2;
	double du, dv, dx, dy;
	double dx0 = 0, dy0 = 0;
	double x0a, x1a, x2a, y0a, y1a, y2a;
	double t;
	double s;

	double u0, u1, v0, v1;
	double ss = 0;
	int d_sign;

	do {
		n++;
		p = p->next;
	} while (p != h);

	double complex dzin = h->next->coord - mp_prev_knot(h)->coord;
	if (!dzin) dzin = I * (h->coord - mp_prev_knot(h)->coord);
	struct mp_knot *w0 = h, *c0 = c;
	p = c;
	int k_needed = 0;
	do {
		q = p->next;

		mp_knot_info(p) = k_needed;
		k_needed = 0;

		x0 = mp_right_x(p) - mp_x_coord(p);
		x2 = mp_x_coord(q) - mp_left_x(q);
		x1 = mp_left_x(q) - mp_right_x(p);
		y0 = mp_right_y(p) - mp_y_coord(p);
		y2 = mp_y_coord(q) - mp_left_y(q);
		y1 = mp_left_y(q) - mp_right_y(p);
		double max_coef = fmax(fmax(fmax(fmax(fmax(fabs(x0), fabs(x1)), fabs(x2)), fabs(y0)), fabs(y1)), fabs(y2));
		if (!max_coef) goto NOT_FOUND;

		dx = x0;
		dy = y0;
		if (!dx && !dy) {
			dx = x1;
			dy = y1;
			if (!dx && !dy) {
				dx = x2;
				dy = y2;
printf("@@@@@%d\n",__LINE__);
			}
		}
		if (p == c) {
			dx0 = dx;
			dy0 = dy;
		}

		turn_amt = mp_get_turn_amt(w0, dx, dy, dy * creal(dzin) >= dx * cimag(dzin));
		w = mp_pen_walk(w0, turn_amt);
		w0 = w;
		mp_knot_info(p) += turn_amt;
		dzin = x2 + y2 * I;
		if (!dzin) {
			dzin = x1 + y1 * I;
printf("@@@@@%d\n",__LINE__);
			if (!dzin) {
				dzin = x0 + y0 * I;
printf("@@@@@%d\n",__LINE__);
			}
		}

		d_sign = dx * cimag(dzin) - creal(dzin) * dy;
		d_sign = (d_sign > 0) - (d_sign < 0);
		if (!d_sign) {
			u0 = mp_x_coord(q) - mp_x_coord(p);
			u1 = mp_y_coord(q) - mp_y_coord(p);
			int tmp1 = dx * u1 - u0 * dy;
			int tmp2 = u0 * cimag(dzin) - creal(dzin) * u1;
			d_sign = ((tmp1>0)-(tmp1<0) + (tmp2>0)-(tmp2<0)) / 2;
printf("@@@@@%d\n",__LINE__);
		}
		if (!d_sign) d_sign = dx ? (dx > 0) - (dx < 0) : (dy > 0) - (dy < 0);

		t0 = (x0 * y2 - x2 * y0) / 2;
		t1 = (x1 * (y0 + y2) - y1 * (x0 + x2)) / 2;
		if (!t0) t0 = d_sign;
		if (t0 > 0) {
			t = mp_crossing_point(t0, t1, -t0);
			u0 = t_of_the_way(x0, x1);
			u1 = t_of_the_way(x1, x2);
			v0 = t_of_the_way(y0, y1);
			v1 = t_of_the_way(y1, y2);
		} else {
			t = mp_crossing_point(-t0, t1, t0);
			u0 = t_of_the_way(x2, x1);
			u1 = t_of_the_way(x1, x0);
			v0 = t_of_the_way(y2, y1);
			v1 = t_of_the_way(y1, y0);
		}
		ss = (x0 + x2) * t_of_the_way(u0, u1) + (y0 + y2) * t_of_the_way(v0, v1);
		turn_amt = mp_get_turn_amt(w, creal(dzin), cimag(dzin), (d_sign > 0));
		if (ss < 0) turn_amt = turn_amt - d_sign * n;

		ww = mp_prev_knot(w);

		du = mp_x_coord(ww) - mp_x_coord(w);
		dv = mp_y_coord(ww) - mp_y_coord(w);
		if (fabs(du) >= fabs(dv)) {
			s = dv / du;
			t0 = x0 * s - y0;
			t1 = x1 * s - y1;
			t2 = x2 * s - y2;
			if (du < 0) {
				t0 = -t0;
				t1 = -t1;
				t2 = -t2;
			}
		} else {
			s = du / dv;
			t0 = x0 - y0 * s;
			t1 = x1 - y1 * s;
			t2 = x2 - y2 * s;
			if (dv < 0) {
				t0 = -t0;
				t1 = -t1;
				t2 = -t2;
			}
		}
		if (t0 < 0) t0 = 0;

		t = mp_crossing_point(t0, t1, t2);
		if (turn_amt >= 0) {
			if (t2 < 0) {
				t = nextafter(1, INFINITY);
			} else {
				u0 = t_of_the_way(x0, x1);
				u1 = t_of_the_way(x1, x2);
				ss = -du / t_of_the_way(u0, u1);
				v0 = t_of_the_way(y0, y1);
				v1 = t_of_the_way(y1, y2);
				ss -= dv * t_of_the_way(v0, v1);
				if (ss < 0) t = nextafter(1, INFINITY);
			}
		} else if (t > 1) {
			t = 1;
		}
		if (t > 1) {
			mp_fin_offset_prep(p, w, x0, x1, x2, y0, y1, y2, 1, turn_amt);
		} else {
			mp_split_cubic(p, t);
			r = mp_next_knot(p);
			x1a = t_of_the_way(x0, x1);
			x1 = t_of_the_way(x1, x2);
			x2a = t_of_the_way(x1a, x1);
			y1a = t_of_the_way(y0, y1);
			y1 = t_of_the_way(y1, y2);
			y2a = t_of_the_way(y1a, y1);
			mp_fin_offset_prep(p, w, x0, x1a, x2a, y0, y1a, y2a, 1, 0);
			x0 = x2a;
			y0 = y2a;
			mp_knot_info(r) = -1;
			if (turn_amt >= 0) {
				t1 = fmin(0, t_of_the_way(t1, t2));
				t = fmin(1, mp_crossing_point(0, -t1, -t2));

				mp_split_cubic(r, t);
				mp_knot_info(r->next) = 1;
				x1a = t_of_the_way(x1, x2);
				x1 = t_of_the_way(x0, x1);
				x0a = t_of_the_way(x1, x1a);
				y1a = t_of_the_way(y1, y2);
				y1 = t_of_the_way(y0, y1);
				y0a = t_of_the_way(y1, y1a);
				mp_fin_offset_prep(r->next, w, x0a, x1a, x2, y0a, y1a, y2, 1, turn_amt);
				x2 = x0a;
				y2 = y0a;
				mp_fin_offset_prep(r, ww, x0, x1, x2, y0, y1, y2, -1, 0);
			} else {
				mp_fin_offset_prep(r, ww, x0, x1, x2, y0, y1, y2, -1, -1 - turn_amt);
			}
		}
		w0 = mp_pen_walk(w0, turn_amt);
NOT_FOUND:

		q0 = q;
		do {
			r = p->next;
			if (p->coord == p->right.coord && p->coord == r->left.coord && p->coord == r->coord && r != p) {
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
				mp_remove_cubic(p);
			}
			p = r;
		} while (p != q);

		if (q != q0 && (q != c || c == c0)) q = q->next;
	} while (q != c);


	mp->spec_offset = mp_knot_info(c);
	if (c->next == c) {
		mp_knot_info(c) = n;
	} else {
		mp_knot_info(c) += k_needed;
		while (w0 != h) {
			mp_knot_info(c)++;
			w0 = w0->next;
		}
		while (mp_knot_info(c) <= -n) mp_knot_info(c) += n;
		while (mp_knot_info(c) > 0) mp_knot_info(c) -= n;
		if (mp_knot_info(c) && dy0 * creal(dzin) >= dx0 * cimag(dzin)) mp_knot_info(c) += n;
	}
	return c;
}









void mp_print_spec(struct mp_knot *cur_spec, struct mp_knot *cur_pen) {
	struct mp_knot *p = cur_spec, *q, *w;
	w = mp_pen_walk(cur_pen, mp->spec_offset);
	printf("Envelope spec\n(%g, %g) %% beginning with offset (%g, %g)", mp_x_coord(cur_spec), mp_y_coord(cur_spec), mp_x_coord(w), mp_y_coord(w));
	do {
		for (;;) {
			q = p->next;
			printf("\n   ..controls (%g, %g) and (%g, %g)\n .. (%g, %g)", mp_right_x(p), mp_right_y(p), mp_left_x(q), mp_left_y(q),mp_x_coord(q), mp_y_coord(q));
			p = q;
			if (p == cur_spec || mp_knot_info(p)) break;
		}
		if (mp_knot_info(p)) {
			w = mp_pen_walk(w, mp_knot_info(p));
			printf(" %% ");
			if (mp_knot_info(p) > 0) printf("counter");
			printf("clockwise to offset (%g, %g)", mp_x_coord(w), mp_y_coord(w));
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
	int kk;
	struct mp_knot *ww;
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

	c = mp_offset_prep(c, h);
	mp_print_spec(c, h);
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
					kk = 0;
					ww = w;
					for (;;) {
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
