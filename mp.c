#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <complex.h>

// mf \tracingchoices:=tracingonline:=1;path p;

static double complex delta[1000]; // knot differences
static double psi[1000]; // turning angles

double reduce_angle(double x) {
	if (fabs(x) > acos(-1)) x -= 4 * ((x > 0) - .5) * acos(-1);
	return x;
}

#ifndef CMPLX
#define _Imaginary_I I
#define CMPLX(x, y) ((double complex) ((double) (x) + _Imaginary_I * (double) (y)))
#endif

double complex cpolar(double magnitude, double phase) {
	assert(magnitude >= 0 && isfinite(phase));
	return CMPLX(magnitude * cos(phase), magnitude * sin(phase));
}

// Adobe Photoshop: anchor point
// https://helpx.adobe.com/photoshop/using/editing-paths.html
// Microsoft Windows: path point
// https://docs.microsoft.com/en-us/dotnet/api/system.drawing.drawing2d.pathpointtype
// Affinity Designer: node
// https://affinity.serif.com/en-us/tutorials/designer/desktop/video/301802454/

struct mp_knot {
	double complex coord;
	struct {
		// Non-explicit control points will be chosen based on “tension” parameters in the left tension and right tension fields. The ‘atleast’ option is represented by negative tension values.
		enum mp_knot_type {
			mp_endpoint = 0,
			// If mp_right_type = mp_explicit, the Bézier control point for leaving this knot has already been computed; it is in the mp_right_x and mp_right_y fields.
			mp_explicit,
			// If mp_right_type = mp_given, the curve should leave the knot in a nonzero direction stored as an angle in right given.
			mp_given,
			// If mp_right_type = mp_curl, the curve should leave the knot in a direction depending on the angle at which it enters the next knot and on the curl parameter stored in right curl.
			mp_curl,
			// If mp_right_type = mp_open, the curve should leave the knot in the same direction it entered; METAPOST will figure out a suitable direction.
			mp_open,
			mp_end_cycle,
		} type;
		static_assert(mp_given > mp_explicit, "enum mp_knot_type");
		static_assert(mp_curl > mp_explicit, "enum mp_knot_type");
		static_assert(mp_open > mp_explicit, "enum mp_knot_type");
		static_assert(mp_end_cycle > mp_explicit, "enum mp_knot_type");
		union {
			double complex coord;
			struct {
				double parameter;
				double tension;
			};
		};
	} left, right;
	struct mp_knot *next;
};


void mp_print_path(struct mp_knot *head) {
	struct mp_knot *p, *q;
	p = head;
	do {
		q = p->next;
		assert(p && q);
		printf("(%g, %g)", creal(p->coord), cimag(p->coord));
		switch (p->right.type) {
		case mp_endpoint:
			assert(p->left.type != mp_open);
			if (q->left.type != mp_endpoint || q != head) q = NULL;
			goto done;
		case mp_explicit:
			printf(" .. controls (%g, %g) and (%g, %g)", creal(p->right.coord), cimag(p->right.coord), creal(q->left.coord), cimag(q->left.coord));
			assert(q->left.type == mp_explicit);
			goto done;
		case mp_open:
			assert(p->left.type == mp_explicit || p->left.type == mp_open);
			break;
		case mp_curl:
		case mp_given:
			assert(p->left.type != mp_open);
			printf("{%s %g}", p->right.type == mp_curl ? "curl" : "dir", p->right.parameter);
			break;
		default:
			abort();
		}
		assert(q->left.type > mp_explicit);
		if (p->right.tension != 1 || q->left.tension != 1) {
			printf(" .. tension %s%g", p->right.tension < 0 ? "atleast " : "", fabs(p->right.tension));
			if (p->right.tension != q->left.tension) {
				printf(" and %s%g", q->left.tension < 0 ? "atleast " : "", fabs(q->left.tension));
			}
		}
	done:
		p = q;
		if (p != head || head->left.type != mp_endpoint) {
			printf(" .. ");
			if (p->left.type == mp_given || p->left.type == mp_curl) {
				printf("{%s %g}", p->left.type == mp_curl ? "curl" : "dir", p->left.parameter);
			}
		}
	} while (p != head);
	if (head->left.type != mp_endpoint) printf("cycle");
	putchar('\n');
}

// This is METAPOST's magic fudge factor for placing the first control point
// of a curve that starts at an angle θ and ends at an angle φ from the straight path.
// It's f(θ, φ) / (3τ) with f(θ, φ) defined on p. 131, The METAFONTbook.
double mp_velocity(double st, double ct, double sf, double cf, double t) {
	assert(t >= .75);
	double num = 2 + sqrt(2) * (st - sf / 16) * (sf - st / 16) * (ct - cf);
	double denom = 3 + 1.5 * (sqrt(5) - 1) * ct + 1.5 * (3 - sqrt(5)) * cf;
	assert(num > 0 && num <= 5.333333333333333 && denom >= 0 && denom <= 6);
	denom *= t;
	return fmin(4, num / denom);
}

// curl_ratio(γ, α⁻¹, β⁻¹) = min(4, (3 − α)α²γ + β³) / (α³γ + (3 − β)β²)
// The reduction of curl is necessary only if the curl and tension are both large.
// The values of α and β will be at most 4/3.
double mp_curl_ratio(double gamma, double a, double b) {
	double ff = b / a;
	a = 1 / a;
	b = 1 / b;
	gamma *= ff * ff;
	return fmin(4, (gamma * (3 - a) + b) / (gamma * a + 3 - b));
}

static void mp_set_controls(struct mp_knot *p, struct mp_knot *q, double theta, double phi, int k) {
	// velocities, divided by thrice the tension
	double complex eit = cexp(theta * I), eif = cexp(phi * I);
	double rr = mp_velocity(cimag(eit), creal(eit), cimag(eif), creal(eif), fabs(p->right.tension));
	double ss = mp_velocity(cimag(eif), creal(eif), cimag(eit), creal(eit), fabs(q->left.tension));
	// Decrease the velocities, if necessary, to stay inside the bounding triangle
	// The boundedness conditions
	//   rr ≤ sin(φ)/sin(θ + φ)  and  ss ≤ sin(θ)/sin(θ + φ)
	// are to be enforced if sin(θ), sin(φ), and sin(θ + φ) all have the same sign.
	// Otherwise there is no “bounding triangle”.
	if ((p->right.tension < 0 || q->left.tension < 0) && cimag(eit) * cimag(eif) >= 0) {
		double sine = cimag(eit * eif); // sin(θ + φ)
		if (cimag(eit) < 0 || cimag(eif) < 0) sine = -sine;
		if (sine > 0) {
			if (p->right.tension < 0) rr = fmin(rr, fabs(cimag(eif) / sine));
			if (q->left.tension < 0) ss = fmin(ss, fabs(cimag(eit) / sine));
		}
	}
	q->left.type = mp_explicit;
	q->left.coord = q->coord - delta[k] * conj(eif) * ss;
	p->right.type = mp_explicit;
	p->right.coord = p->coord + delta[k] * eit * rr;
}







































































// Search for John Hobby in the METAFONT source document for more details of the mathematics involved.
static void mp_solve_choices(struct mp_knot *p, struct mp_knot *q, int n) {
	struct mp_knot *r = NULL, *s = p, *t = p->next;
	double aa, bb, cc, dd, ee, ff;
	double theta[n + 1], uu[n], vv[n + 1], ww[n + 1];
	// Get the linear equations started; or return with the control points in place, if linear equations needn't be solved
	switch (s->right.type) {
	case mp_given:
		if (t->left.type == mp_given) {
			aa = carg(delta[0]);
			mp_set_controls(p, q, p->right.parameter - aa, aa - q->left.parameter, 0);
			return;
		} else {
			vv[0] = reduce_angle(s->right.parameter - carg(delta[0]));
			uu[0] = 0;
			ww[0] = 0;
		}
		break;
	case mp_curl:
		if (t->left.type == mp_curl) {
			p->right.type = mp_explicit;
			q->left.type = mp_explicit;
			p->right.coord = p->coord + delta[0] / (3 * fabs(p->right.tension));
			q->left.coord = q->coord - delta[0] / (3 * fabs(q->left.tension));
			return;
		} else {
			cc = s->right.parameter;
			uu[0] = mp_curl_ratio(cc, fabs(s->right.tension), fabs(t->left.tension));
			vv[0] = -psi[1] * uu[0];
			ww[0] = 0;
		}
		break;
	case mp_open:
		uu[0] = 0;
		vv[0] = 0;
		ww[0] = 1;
		break;
	default:
		abort();
	}
	r = s;
	s = t;
	t = s->next;

	for (int k = 1; ; k++, r = s, s = t, t = s->next) switch (s->left.type) {
	case mp_end_cycle:
	case mp_open:
		// Set up equation to match mock curvatures at zₖ
		aa = 1 / (3 * fabs(r->right.tension) - 1);
		bb = 1 / (3 * fabs(t->left.tension) - 1);
		cc = 1 - uu[k - 1] * aa;
		dd = cabs(delta[k]) * (3 - 1 / fabs(r->right.tension));
		ee = cabs(delta[k - 1]) * (3 - 1 / fabs(t->left.tension));
		ff = fabs(s->left.tension) / fabs(s->right.tension);
		ff = ee / (ee + cc * dd * ff * ff);

		uu[k] = ff * bb;
		if (r->right.type == mp_curl) {
			ww[k] = 0;
			vv[k] = -psi[k + 1] * uu[k] - psi[1] * (1 - ff);
		} else {
			ff = (1 - ff) / cc;
			vv[k] = -psi[k + 1] * uu[k] - psi[k] * ff - vv[k - 1] * ff * aa;
			ww[k] = -ww[k - 1] * ff * aa;
		}

		// adjust θₙ to equal θ₀ if a cycle has ended
		if (s->left.type == mp_end_cycle) {
			aa = 0;
			bb = 1;
			do {
				k--;
				if (!k) k = n;
				aa = vv[k] - aa * uu[k];
				bb = ww[k] - bb * uu[k];
			} while (k != n);
			aa /= 1 - bb;
			theta[n] = vv[0] = aa;
			for (int k = 1; k < n; k++) {
				vv[k] += aa * ww[k];
			}
			goto finish;
		}
		break;
	case mp_curl:
		// Set up equation for a curl at θₙ
		cc = s->left.parameter;
		ff = mp_curl_ratio(cc, fabs(s->left.tension), fabs(r->right.tension));
		theta[n] = -vv[n - 1] * ff / (1 - ff * uu[n - 1]);
		goto finish;
	case mp_given:
		// Calculate the given value of θₙ
		theta[n] = reduce_angle(s->left.parameter - carg(delta[n - 1]));
		goto finish;
	default:
		abort();
	}
finish:
	// Finish choosing angles and assigning control points
	for (int k = n - 1; k >= 0; k--) {
		theta[k] = vv[k] - theta[k + 1] * uu[k];
	}
	s = p;
	int k = 0;
	do {
		t = s->next;
		mp_set_controls(s, t, theta[k], -psi[k + 1] - theta[k + 1], k);
		k++;
		s = t;
	} while (k != n);
}

void mp_make_choices(struct mp_knot *knots) {
	// the first breakpoint, whose left and right types are neither mp_open
	struct mp_knot *h;
	// consecutive breakpoints being processed
	struct mp_knot *p, *q;

	// If consecutive knots are equal, join them with an explicit “curve” whose control points are identical to the knots.
	p = knots;
	do {
		q = p->next;
		if (p->coord == q->coord && p->right.type > mp_explicit) {
printf("\n\n\n!!!@@@@@%d\n\n\n\n", __LINE__);
			p->right.type = mp_explicit;
			p->right.coord = p->coord;
			if (p->left.type == mp_open) {
printf("\n\n\n!!!@@@@@%d\n\n\n\n", __LINE__);
				p->left.type = mp_curl;
				p->left.parameter = 1;
			}
			q->left.type = mp_explicit;
			q->left.coord = p->coord;
			if (q->right.type == mp_open) {
printf("\n\n\n!!!@@@@@%d\n\n\n\n", __LINE__);
				q->right.type = mp_curl;
				q->right.parameter = 1;
			}
		}
		p = q;
	} while (p != knots);

	// Find the first breakpoint, h
	h = knots;
	while (h->left.type == mp_open && h->right.type == mp_open) {
		h = h->next;
		// temporarily change left.type of the first node to mp_end_cycle if there are no breakpoints
		if (h == knots) {
			h->left.type = mp_end_cycle;
			break;
		}
	}

	p = h;
	do {
		// Fill in the control points between p and the next breakpoint
		q = p->next;
		if (p->right.type > mp_explicit) {
			while (q->left.type == mp_open && q->right.type == mp_open) q = q->next;
			// Calculate the turning angles ψₖ and the distances
			int k = 0, n = INT_MAX; // current and final knot numbers
			struct mp_knot *s = p;
			do {
				delta[k] = s->next->coord - s->coord;
				if (k) psi[k] = carg(conj(delta[k - 1]) * delta[k]);
				k++;
				s = s->next;
				// set n to the length of the path
				if (s == q) n = k;
			} while (k < n || s->left.type == mp_end_cycle);
			psi[k] = k == n ? 0 : psi[1];
			// Remove open types at the breakpoints
			if (p->right.type == mp_open && p->left.type == mp_explicit) {
				if (p->coord == p->left.coord) {
					// If the velocity coming into this knot is zero, the mp_open type is converted to a mp_curl, since we don't know the incoming direction.
					p->right.type = mp_curl;
					p->right.parameter = 1;
				} else {
					// the mp_open type is converted to mp_given
					p->right.type = mp_given;
					p->right.parameter = carg(p->coord - p->left.coord);
				}
			}
			if (q->left.type == mp_open) {
				if (q->right.coord == q->coord) {
					q->left.type = mp_curl;
					q->left.parameter = 1;
				} else {
					q->left.type = mp_given;
					q->left.parameter = carg(q->right.coord - q->coord);
				}
			}
			mp_solve_choices(p, q, n);
		} else if (p->right.type == mp_endpoint) {
			// Give reasonable values for the unused control points between p and q
			// This step makes it possible to transform an explicitly computed path without checking the mp_left_type and mp_right_type fields.
			p->right.coord = p->coord;
			q->left.coord = q->coord;
		}
		// advance p to the next breakpoint
		p = q;
	} while (p != h);
}

int main() {
	// (0, 0)
	// .. (2, 20){curl 1}
	// .. {curl 1}(10, 5) .. controls (2, 2)
	// and (9, 4.5) .. (3, 10) .. tension 3
	// and atleast 4 .. (1, 14){2, 0}
	// .. {0, 1}(5, -4)
	//
	// (0, 0)
	// ..controls (0.66667, 6.66667) and (1.33333, 13.33333)
	// ..(2, 20)
	// ..controls (4.66667, 15) and (7.33333, 10)
	// ..(10, 5)
	// ..controls (2, 2) and (9, 4.5)
	// ..(3, 10)
	// ..controls (2.34547, 10.59998) and (0.48712, 14)
	// ..(1, 14)
	// ..controls (13.40117, 14) and (5, -35.58354)
	// ..(5, -4)
	struct mp_knot z1 =
	{
		.coord=0,
		.left.type = mp_endpoint,
		.right.type = mp_curl,
		.right.parameter = 1,
		.right.tension = 1,
	},
		z2 =
		{
			.coord = 2+20*I,
			.left.type = mp_curl,
			.left.parameter = 1,
			.left.tension = 1,
			.right.type = mp_curl,
			.right.parameter = 1,
			.right.tension = 1,
		},
		z3 =
		{
			.coord = 10+5*I,
			.left.type = mp_curl,
			.left.parameter = 1,
			.left.tension = 1,
			.right.type = mp_explicit,
			.right.parameter = 2,
			.right.tension = 2,
		},
		z4 =
		{
			.coord = 3+10*I,
			.left.type = mp_explicit,
			.left.coord = 9+4.5*I,
			.right.type = mp_open,
			.right.tension = 3,
		},
		z5 =
		{
			.coord = 1+14*I,
			.left.type = mp_given,
			.left.parameter = 0,
			.left.tension= -4,
			.right.type = mp_given,
			.right.parameter = 0,
			.right.tension = 1,
		},
		z6 = {
			.coord = 5-4*I,
			.left.type = mp_given,
			.left.parameter = asin(1),
			.left.tension = 1,
			.right.type = mp_endpoint,
		},
		// (0, 0){dir 60°} ... {dir -10°}(400, 0)
		// (0, 0) .. controls (36.94897, 63.99768) and (248.95918, 26.63225) .. (400, 0)
		z7 =
		{
			.coord = 0,
			.left.type = mp_endpoint,
			.right.type = mp_given,
			.right.parameter = acos(.5),
			.right.tension = -1,
		},
		z8 = {
			.coord = 400,
			.left.type = mp_given,
			.left.parameter = asin(-1) / 9,
			.left.tension = -1,
			.right.type = mp_endpoint,
		},
		// (0, 0) .. (10, 10) .. (10, -5) .. cycle
		// (0,0)..controls (-1.8685,6.35925) and (4.02429,12.14362)
		// ..(10,10)..controls (16.85191,7.54208) and (16.9642,-2.22969)
		// ..(10,-5)..controls (5.87875,-6.6394) and (1.26079,-4.29094)..cycle
		z9 =
		{
			.coord = 0,
			.left.type = mp_open,
			.left.tension = 1,
			.right.type = mp_open,
			.right.tension = 1,
		},
		z10 =
		{
			.coord = 10+10*I,
			.left.type = mp_open,
			.left.tension = 1,
			.right.type = mp_open,
			.right.tension = 1,
		},
		z11 =
		{
			.coord = 10-5*I,
			.left.type = mp_open,
			.left.tension = 1,
			.right.type = mp_open,
			.right.tension = 1,
		},
		// (1,1)..(4,5)..tension atleast 1..{curl 2}(1,4)..(19,-1){-1,-2}..tension 3 and 4..(9,-8)..controls (4,5) and (5,4)..(1,0)
		// (1,1)..controls (3.51616,-0.21094) and (5.86702,2.92355)
		// ..(4,5)..controls (2.73756,6.40405) and (0.7107,5.41827)
		// ..(1,4)..controls (-5.32942,20.68242) and (29.16524,19.33046)
		// ..(19,-1)..controls (17.90521,-3.18956) and (9.42474,-9.10431)
		// ..(9,-8)..controls (4,5) and (5,4)
		// ..(1,0)
		z12 =
		{
			.coord = 1+1*I,
			.left.type = mp_endpoint,
			.right.type = mp_curl,
			.right.parameter = 1,
			.right.tension = 1,
		},
		z13 =
		{
			.coord = 4+5*I,
			.left.type = mp_open,
			.left.tension = 1,
			.right.type = mp_open,
			.right.tension = -1,
		},
		z14 =
		{
			.coord = 1+4*I,
			.left.type = mp_curl,
			.left.parameter = 2,
			.left.tension = -1,
			.right.type = mp_curl,
			.right.parameter = 2,
			.right.tension = 1,
		},
		z15 =
		{
			.coord = 19-1*I,
			.left.type = mp_given,
			.left.parameter = atan2(-2, -1),
			.left.tension = 1,
			.right.type = mp_given,
			.right.parameter = atan2(-2, -1),
			.right.tension = 3,
		},
		z16 =
		{
			.coord = 9-8*I,
			.left.type = mp_open,
			.left.tension = 4,
			.right.type = mp_explicit,
			.right.coord = 4+5*I,
		},
		z17 =
		{
			.coord = 1,
			.left.type = mp_explicit,
			.left.coord = 5+4*I,
			.right.type = mp_endpoint,
		},
		// (0,0)..controls (0,0.5) and (0,1)..(0,1)..(1,1)..controls (1,1) and (1,0.5)..(1,0){dir 45}..(2,0)
		// (0,0)..controls (0,0.5) and (0,1)
		// ..(0,1)..controls (0.33333,1) and (0.66667,1)
		// ..(1,1)..controls (1,1) and (1,0.5)
		// ..(1,0)..controls (1.27614,0.27614) and (1.72386,0.27614)
		// ..(2,0)
		z18 =
		{
			.coord = 0,
			.left.type = mp_endpoint,
			.right.type = mp_explicit,
			.right.coord = .5*I,
		},
		z19 =
		{
			.coord = I,
			.left.type = mp_explicit,
			.left.coord = I,
			.right.type = mp_open,
			.right.tension = 1,
		},
		z20 =
		{
			.coord = 1+I,
			.left.type = mp_open,
			.left.tension = 1,
			.right.type = mp_explicit,
			.right.coord = 1+I,
		},
		z21 =
		{
			.coord = 1,
			.left.type = mp_explicit,
			.left.coord = 1+.5*I,
			.right.type = mp_given,
			.right.parameter = atan(1),
			.right.tension = 1,
		},
		z22 =
		{
			.coord = 2,
			.left.type = mp_curl,
			.left.parameter = 1,
			.left.tension = 1,
			.right.type = mp_endpoint,
		};
	z1.next = &z2;
	z2.next = &z3;
	z3.next = &z4;
	z4.next = &z5;
	z5.next = &z6;
	z6.next = &z1;
	mp_print_path(&z1);mp_make_choices(&z1);mp_print_path(&z1);
	z7.next = &z8;
	z8.next = &z7;
	mp_print_path(&z7);mp_make_choices(&z7);mp_print_path(&z7);
	z9.next = &z10;
	z10.next = &z11;
	z11.next = &z9;
	mp_print_path(&z9);mp_make_choices(&z9);mp_print_path(&z9);
	z12.next = &z13;
	z13.next = &z14;
	z14.next = &z15;
	z15.next = &z16;
	z16.next = &z17;
	z17.next = &z12;
	mp_print_path(&z12);mp_make_choices(&z12);mp_print_path(&z12);
	z18.next = &z19;
	z19.next = &z20;
	z20.next = &z21;
	z21.next = &z22;
	z22.next = &z18;
	mp_print_path(&z18);mp_make_choices(&z18);mp_print_path(&z18);
}
