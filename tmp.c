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
#define zero_off 16384
#define incr(A) (A) = (A) + 1
#define decr(A) (A) = (A) -1
#define negate(A) (A) = -(A)
#define double(A) (A) = (A) + (A)
#define odd(A) ((A) % 2 == 1)
#define fix_by(A) mp_knot_info(c) = mp_knot_info(c) + (A)
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
#define three_sixty_deg (2*acos(-1))

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
	struct mp_knot *q = malloc(sizeof(*q)), *pp, *qq, *rr;
	qq = q;
	pp = p;
	while (1) {
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
	struct mp_knot *ww;
	int s = 0, t;
	if (ccw) {
		ww = mp_next_knot(w);
		do {
			t = dy * (mp_x_coord(ww) - mp_x_coord(w)) - dx * (mp_y_coord(ww) - mp_y_coord(w));
			if (t < 0) break;
			incr(s);
			w = ww;
			ww = mp_next_knot(ww);
		} while (t > 0);
	} else {
		ww = mp_prev_knot(w);
		while (dy * (mp_x_coord(w) - mp_x_coord(ww)) < dx * (mp_y_coord(w) - mp_y_coord(ww))) {
			decr(s);
			w = ww;
			ww = mp_prev_knot(ww);
		}
	}
	return s;
}
void mp_fin_offset_prep(struct mp_knot *p, struct mp_knot *w, double x0, double x1, double x2, double y0, double y1, double y2, int rise, int turn_amt) {
	struct mp_knot *ww;
	double du, dv;
	double t0, t1, t2;
	double t;
	double s;
	double v;
	struct mp_knot *q;
	q = mp_next_knot(p);
	for (;;) {
		if (rise > 0)
			ww = mp_next_knot(w);
		else
			ww = mp_prev_knot(w);


		du = mp_x_coord(ww) - mp_x_coord(w);
		dv = mp_y_coord(ww) - mp_y_coord(w);
		if (fabs(du) >= fabs(dv)) {
			s = mp_make_fraction(dv, du);
			t0 = mp_take_fraction(x0, s) - y0;
			t1 = mp_take_fraction(x1, s) - y1;
			t2 = mp_take_fraction(x2, s) - y2;
			if (du < 0) {
				negate(t0);
				negate(t1);
				negate(t2);
			}
		} else {
			s = mp_make_fraction(du, dv);
			t0 = x0 - mp_take_fraction(y0, s);
			t1 = x1 - mp_take_fraction(y1, s);
			t2 = x2 - mp_take_fraction(y2, s);
			if (dv < 0) {
				negate(t0);
				negate(t1);
				negate(t2);
			}
		}
		if (t0 < 0)
			t0 = 0;
		t = mp_crossing_point(t0, t1, t2);
		if (t >= fraction_one) {
			if (turn_amt > 0)
				t = fraction_one;
			else
				return;
		}


		{
			mp_split_cubic(p, t);
			p = mp_next_knot(p);
			mp_knot_info(p) = zero_off + rise;
			decr(turn_amt);
			v = t_of_the_way(x0, x1);
			x1 = t_of_the_way(x1, x2);
			x0 = t_of_the_way(v, x1);
			v = t_of_the_way(y0, y1);
			y1 = t_of_the_way(y1, y2);
			y0 = t_of_the_way(v, y1);
			if (turn_amt < 0) {
				t1 = t_of_the_way(t1, t2);
				if (t1 > 0)
					t1 = 0;
				t = mp_crossing_point(0, -t1, -t2);
				if (t > fraction_one)
					t = fraction_one;
				incr(turn_amt);
				if ((t == fraction_one) && (mp_next_knot(p) != q)) {
					mp_knot_info(mp_next_knot(p)) = mp_knot_info(mp_next_knot(p)) - rise;
				} else {
					mp_split_cubic(p, t);
					mp_knot_info(mp_next_knot(p)) = zero_off - rise;
					v = t_of_the_way(x1, x2);
					x1 = t_of_the_way(x0, x1);
					x2 = t_of_the_way(x1, v);
					v = t_of_the_way(y1, y2);
					y1 = t_of_the_way(y0, y1);
					y2 = t_of_the_way(y1, v);
				}
			}
		};
		w = ww;
	}
}
static struct mp_knot *mp_offset_prep(struct mp_knot *c, struct mp_knot *h) {
	int n;
	struct mp_knot *c0, *p, *q, *q0, *r, *w, *ww;
	int k_needed;
	struct mp_knot *w0;
	double dxin, dyin;
	int turn_amt;


	double x0, x1, x2, y0, y1, y2;
	double t0, t1, t2;
	double du, dv, dx, dy;
	double dx0, dy0;
	double max_coef;
	double x0a, x1a, x2a, y0a, y1a, y2a;
	double t;
	double s;


	double u0, u1, v0, v1;
	double ss = 0;
	int d_sign;
	;
	dx0 = 0;
	dy0 = 0;


	n = 0;
	p = h;
	do {
		incr(n);
		p = mp_next_knot(p);
	} while (p != h);


	dxin = mp_x_coord(mp_next_knot(h)) - mp_x_coord(mp_prev_knot(h));
	dyin = mp_y_coord(mp_next_knot(h)) - mp_y_coord(mp_prev_knot(h));
	if ((dxin == 0) && (dyin == 0)) {
		dxin = mp_y_coord(mp_prev_knot(h)) - mp_y_coord(h);
		dyin = mp_x_coord(h) - mp_x_coord(mp_prev_knot(h));
	}
	w0 = h;
	p = c;
	c0 = c;
	k_needed = 0;
	do {
		q = mp_next_knot(p);


		mp_knot_info(p) = zero_off + k_needed;
		k_needed = 0;


		x0 = mp_right_x(p) - mp_x_coord(p);
		x2 = mp_x_coord(q) - mp_left_x(q);
		x1 = mp_left_x(q) - mp_right_x(p);
		y0 = mp_right_y(p) - mp_y_coord(p);
		y2 = mp_y_coord(q) - mp_left_y(q);
		y1 = mp_left_y(q) - mp_right_y(p);
		max_coef = fabs(x0);
		if (fabs(x1) > max_coef)
			max_coef = fabs(x1);
		if (fabs(x2) > max_coef)
			max_coef = fabs(x2);
		if (fabs(y0) > max_coef)
			max_coef = fabs(y0);
		if (fabs(y1) > max_coef)
			max_coef = fabs(y1);
		if (fabs(y2) > max_coef)
			max_coef = fabs(y2);
		if (max_coef == 0)
			goto NOT_FOUND;
		while (max_coef < fraction_half) {
			double(max_coef);
			double(x0);
			double(x1);
			double(x2);
			double(y0);
			double(y1);
			double(y2);
		};


		dx = x0;
		dy = y0;
		if (dx == 0 && dy == 0) {
			dx = x1;
			dy = y1;
			if (dx == 0 && dy == 0) {
				dx = x2;
				dy = y2;
			}
		}
		if (p == c) {
			dx0 = dx;
			dy0 = dy;
		};


		turn_amt =
			mp_get_turn_amt(w0, dx, dy, dy * dxin >= dx * dyin);
		w = mp_pen_walk(w0, turn_amt);
		w0 = w;
		mp_knot_info(p) = mp_knot_info(p) + turn_amt;


		dxin = x2;
		dyin = y2;
		if (dxin == 0 && dyin == 0) {
			dxin = x1;
			dyin = y1;
			if (dxin == 0 && dyin == 0) {
				dxin = x0;
				dyin = y0;
			}
		};


		d_sign = dx * dyin - dxin * dy;
		d_sign = (d_sign > 0) - (d_sign < 0);
		if (d_sign == 0) {
			u0 = mp_x_coord(q) - mp_x_coord(p);
			u1 = mp_y_coord(q) - mp_y_coord(p);
			int tmp1 = dx * u1 - u0 * dy;
			int tmp2 = u0 * dyin - dxin * u1;
			d_sign = ((tmp1>0)-(tmp1<0) + (tmp2>0)-(tmp2<0)) / 2;
		}
		if (d_sign == 0) {
			if (dx == 0) {
				if (dy > 0)
					d_sign = 1;
				else
					d_sign = -1;
			} else {
				if (dx > 0)
					d_sign = 1;
				else
					d_sign = -1;
			}
		}


		t0 = (mp_take_fraction(x0, y2) - mp_take_fraction(x2, y0)) / 2;
		t1 = (mp_take_fraction(x1, (y0 + y2)) - mp_take_fraction(y1, (x0 + x2))) / 2;
		if (t0 == 0)
			t0 = d_sign;
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
		ss = mp_take_fraction((x0 + x2), t_of_the_way(u0, u1)) + mp_take_fraction((y0 + y2), t_of_the_way(v0, v1));
		turn_amt = mp_get_turn_amt(w, dxin, dyin, (d_sign > 0));
		if (ss < 0)
			turn_amt = turn_amt - d_sign * n;


		ww = mp_prev_knot(w);


		du = mp_x_coord(ww) - mp_x_coord(w);
		dv = mp_y_coord(ww) - mp_y_coord(w);
		if (fabs(du) >= fabs(dv)) {
			s = mp_make_fraction(dv, du);
			t0 = mp_take_fraction(x0, s) - y0;
			t1 = mp_take_fraction(x1, s) - y1;
			t2 = mp_take_fraction(x2, s) - y2;
			if (du < 0) {
				negate(t0);
				negate(t1);
				negate(t2);
			}
		} else {
			s = mp_make_fraction(du, dv);
			t0 = x0 - mp_take_fraction(y0, s);
			t1 = x1 - mp_take_fraction(y1, s);
			t2 = x2 - mp_take_fraction(y2, s);
			if (dv < 0) {
				negate(t0);
				negate(t1);
				negate(t2);
			}
		}
		if (t0 < 0)
			t0 = 0;


		t = mp_crossing_point(t0, t1, t2);
		if (turn_amt >= 0) {
			if (t2 < 0) {
				t = fraction_one + 1;
			} else {
				u0 = t_of_the_way(x0, x1);
				u1 = t_of_the_way(x1, x2);
				ss = mp_take_fraction(-du, t_of_the_way(u0, u1));
				v0 = t_of_the_way(y0, y1);
				v1 = t_of_the_way(y1, y2);
				ss = ss + mp_take_fraction(-dv, t_of_the_way(v0, v1));
				if (ss < 0)
					t = fraction_one + 1;
			}
		} else if (t > fraction_one) {
			t = fraction_one;
		};
		if (t > fraction_one) {
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
			mp_knot_info(r) = zero_off - 1;
			if (turn_amt >= 0) {
				t1 = t_of_the_way(t1, t2);
				if (t1 > 0)
					t1 = 0;
				t = mp_crossing_point(0, -t1, -t2);
				if (t > fraction_one)
					t = fraction_one;


				mp_split_cubic(r, t);
				mp_knot_info(mp_next_knot(r)) = zero_off + 1;
				x1a = t_of_the_way(x1, x2);
				x1 = t_of_the_way(x0, x1);
				x0a = t_of_the_way(x1, x1a);
				y1a = t_of_the_way(y1, y2);
				y1 = t_of_the_way(y0, y1);
				y0a = t_of_the_way(y1, y1a);
				mp_fin_offset_prep(mp_next_knot(r), w, x0a, x1a, x2, y0a, y1a, y2, 1,
					turn_amt);
				x2 = x0a;
				y2 = y0a;
				mp_fin_offset_prep(r, ww, x0, x1, x2, y0, y1, y2, -1, 0);
			} else {
				mp_fin_offset_prep(r, ww, x0, x1, x2, y0, y1, y2, -1, (-1 - turn_amt));
			}
		};
		w0 = mp_pen_walk(w0, turn_amt);
	NOT_FOUND:


		q0 = q;
		do {
			r = mp_next_knot(p);
			if (mp_x_coord(p) == mp_right_x(p) && mp_y_coord(p) == mp_right_y(p) && mp_x_coord(p) == mp_left_x(r) && mp_y_coord(p) == mp_left_y(r) && mp_x_coord(p) == mp_x_coord(r) && mp_y_coord(p) == mp_y_coord(r) && r != p) {
				{
					k_needed = mp_knot_info(p) - zero_off;
					if (r == q) {
						q = p;
					} else {
						mp_knot_info(p) = k_needed + mp_knot_info(r);
						k_needed = 0;
					}
					if (r == c) {
						mp_knot_info(p) = mp_knot_info(c);
						c = p;
					}
					if (r == mp->spec_p1)
						mp->spec_p1 = p;
					if (r == mp->spec_p2)
						mp->spec_p2 = p;
					r = p;
					mp_remove_cubic(p);
				};
			}
			p = r;
		} while (p != q);

		if ((q != q0) && (q != c || c == c0))
			q = mp_next_knot(q);
	} while (q != c);


	mp->spec_offset = mp_knot_info(c) - zero_off;
	if (mp_next_knot(c) == c) {
		mp_knot_info(c) = zero_off + n;
	} else {
		fix_by(k_needed);
		while (w0 != h) {
			fix_by(1);
			w0 = mp_next_knot(w0);
		}
		while (mp_knot_info(c) <= zero_off - n)
			fix_by(n);
		while (mp_knot_info(c) > zero_off)
			fix_by(-n);
		if ((mp_knot_info(c) != zero_off) && dy0 * dxin >= dx0 * dyin)
			fix_by(n);
	};
	return c;
}









void mp_print_spec(struct mp_knot *cur_spec, struct mp_knot *cur_pen) {
	struct mp_knot *p, *q, *w;
	p = cur_spec;
	w = mp_pen_walk(cur_pen, mp->spec_offset);
	printf("Envelope spec\n(%g, %g) %% beginning with offset (%g, %g)", mp_x_coord(cur_spec), mp_y_coord(cur_spec), mp_x_coord(w), mp_y_coord(w));
	do {
		for (;;) {
			q = mp_next_knot(p);
			printf("\n   ..controls (%g, %g) and (%g, %g)\n .. (%g, %g)", mp_right_x(p), mp_right_y(p), mp_left_x(q), mp_left_y(q),mp_x_coord(q), mp_y_coord(q));
			p = q;
			if ((p == cur_spec) || (mp_knot_info(p) != zero_off)) break;
		}
		if (mp_knot_info(p) != zero_off) {
				w = mp_pen_walk(w, (mp_knot_info(p) - zero_off));
				printf(" %% ");
				if (mp_knot_info(p) > zero_off) printf("counter");
				printf("clockwise to offset (%g, %g)", mp_x_coord(w), mp_y_coord(w));
		}
	} while (p != cur_spec);
	puts(" & cycle");
}


struct mp_knot *mp_make_envelope(struct mp_knot *c, struct mp_knot *h, int ljoin, int lcap, double miterlim) {
	struct mp_knot *p, *q, *r, *q0;
	struct mp_knot *w, *w0;
	double qx, qy;
	int k, k0;
	int join_type = 0;
	double dxin, dyin, dxout, dyout;
	double tmp;
	double det;
	double ht_x, ht_y;
	double max_ht;
	int kk;
	struct mp_knot *ww;
	dxin = 0;
	dyin = 0;
	dxout = 0;
	dyout = 0;
	mp->spec_p1 = NULL;
	mp->spec_p2 = NULL;


	if (mp_left_type(c) == mp_endpoint) {
		mp->spec_p1 = mp_htap_ypoc(c);
		mp->spec_p2 = mp->path_tail;
		mp_next_knot(mp->spec_p2) = mp_next_knot(mp->spec_p1);
		mp_next_knot(mp->spec_p1) = c;
		mp_remove_cubic(mp->spec_p1);
		c = mp->spec_p1;
		if (c != mp_next_knot(c)) {
			mp_remove_cubic(mp->spec_p2);
		} else {
				mp_left_type(c) = mp_explicit;
				mp_right_type(c) = mp_explicit;
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
		q = mp_next_knot(p);
		q0 = q;
		qx = mp_x_coord(q);
		qy = mp_y_coord(q);
		k = mp_knot_info(q);
		k0 = k;
		w0 = w;
		if (k != zero_off) {
			if (k < zero_off) {
				join_type = 2;
			} else {
				if ((q != mp->spec_p1) && (q != mp->spec_p2))
					join_type = ljoin;
				else if (lcap == 2)
					join_type = 3;
				else
					join_type = 2 - lcap;
				if ((join_type == 0) || (join_type == 3)) {
					dxin = mp_x_coord(q) - mp_left_x(q);
					dyin = mp_y_coord(q) - mp_left_y(q);
					if ((dxin == 0) && (dyin == 0)) {
						dxin = mp_x_coord(q) - mp_right_x(p);
						dyin = mp_y_coord(q) - mp_right_y(p);
						if ((dxin == 0) && (dyin == 0)) {
							dxin = mp_x_coord(q) - mp_x_coord(p);
							dyin = mp_y_coord(q) - mp_y_coord(p);
							if (p != c) {
								dxin = dxin + mp_x_coord(w);
								dyin = dyin + mp_y_coord(w);
							}
						}
					}
					tmp = hypot(dxin, dyin);
					if (tmp == 0) {
						join_type = 2;
					} else {
						dxin = mp_make_fraction(dxin, tmp);
						dyin = mp_make_fraction(dyin, tmp);


						dxout = mp_right_x(q) - mp_x_coord(q);
						dyout = mp_right_y(q) - mp_y_coord(q);
						if ((dxout == 0) && (dyout == 0)) {
							r = mp_next_knot(q);
							dxout = mp_left_x(r) - mp_x_coord(q);
							dyout = mp_left_y(r) - mp_y_coord(q);
							if ((dxout == 0) && (dyout == 0)) {
								dxout = mp_x_coord(r) - mp_x_coord(q);
								dyout = mp_y_coord(r) - mp_y_coord(q);
							}
						}
						if (q == c) {
							dxout = dxout - mp_x_coord(h);
							dyout = dyout - mp_y_coord(h);
						}
						tmp = hypot(dxout, dyout);
						assert(tmp); // tmp = 0 → degenerate spec
						dxout = mp_make_fraction(dxout, tmp);
						dyout = mp_make_fraction(dyout, tmp);
					};
					if (join_type == 0) {
							tmp = mp_take_fraction(miterlim, fraction_half + (mp_take_fraction(dxin, dxout) + mp_take_fraction(dyin, dyout)) / 2);
							if (tmp < unity)
								if (miterlim * tmp < unity)
									join_type = 2;
					}
				}
			};
		}


		mp_right_x(p) = mp_right_x(p) + mp_x_coord(w);
		mp_right_y(p) = mp_right_y(p) + mp_y_coord(w);
		mp_left_x(q) = mp_left_x(q) + mp_x_coord(w);
		mp_left_y(q) = mp_left_y(q) + mp_y_coord(w);
		mp_x_coord(q) = mp_x_coord(q) + mp_x_coord(w);
		mp_y_coord(q) = mp_y_coord(q) + mp_y_coord(w);
		mp_left_type(q) = mp_explicit;
		mp_right_type(q) = mp_explicit;
		while (k != zero_off) {
			if (k > zero_off) {
				w = mp_next_knot(w);
				decr(k);
			} else {
				w = mp_prev_knot(w);
				incr(k);
			};
			if ((join_type == 1) || (k == zero_off))
				q = mp_insert_knot(q, qx + mp_x_coord(w), qy + mp_y_coord(w));
		};
		if (q != mp_next_knot(p)) {
			{
				p = mp_next_knot(p);
				if ((join_type == 0) || (join_type == 3)) {
					if (join_type == 0) {
						{
							det = mp_take_fraction(dyout, dxin) - mp_take_fraction(dxout, dyin);
							if (fabs(det) < 1e-4) {
								r = NULL;
							} else {
								tmp = mp_take_fraction(mp_x_coord(q) - mp_x_coord(p), dyout) - mp_take_fraction(mp_y_coord(q) - mp_y_coord(p), dxout);
								tmp = mp_make_fraction(tmp, det);
								r =
									mp_insert_knot(p, mp_x_coord(p) + mp_take_fraction(tmp, dxin),
										mp_y_coord(p) + mp_take_fraction(tmp, dyin));
							}
						}


					} else {
						{
							ht_x = mp_y_coord(w) - mp_y_coord(w0);
							ht_y = mp_x_coord(w0) - mp_x_coord(w);
							while ((fabs(ht_x) < fraction_half) && (fabs(ht_y) < fraction_half)) {
								ht_x += ht_x;
								ht_y += ht_y;
							}


							max_ht = 0;
							kk = zero_off;
							ww = w;
							while (1) {
								if (kk > k0) {
									ww = mp_next_knot(ww);
									decr(kk);
								} else {
									ww = mp_prev_knot(ww);
									incr(kk);
								};
								if (kk == k0)
									break;
								tmp = mp_take_fraction((mp_x_coord(ww) - mp_x_coord(w0)), ht_x) + mp_take_fraction((mp_y_coord(ww) - mp_y_coord(w0)), ht_y);
								if (tmp > max_ht)
									max_ht = tmp;
							};
							tmp = mp_make_fraction(max_ht, mp_take_fraction(dxin, ht_x) + mp_take_fraction(dyin, ht_y));
							r = mp_insert_knot(p, mp_x_coord(p) + mp_take_fraction(tmp, dxin),
								mp_y_coord(p) + mp_take_fraction(tmp, dyin));
							tmp = mp_make_fraction(max_ht, mp_take_fraction(dxout, ht_x) + mp_take_fraction(dyout, ht_y));
							r = mp_insert_knot(r, mp_x_coord(q) + mp_take_fraction(tmp, dxout),
								mp_y_coord(q) + mp_take_fraction(tmp, dyout));
						};
					}
					if (r != NULL) {
						mp_right_x(r) = mp_x_coord(r);
						mp_right_y(r) = mp_y_coord(r);
					}
				}
			};
		}
		p = q;
	} while (q0 != c);
	return c;
}
