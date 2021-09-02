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

double reduce_angle(double x) {
	if (fabs(x) > acos(-1)) x -= 4 * ((x > 0) - .5) * acos(-1);
	return x;
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
		union {
			// Non-explicit control points will be chosen based on “tension” parameters in the left.tension and right.tension fields. The “atleast” option is represented by negative tension values.
			enum mp_knot_type {
				mp_endpoint = 0,
				// If right.type = mp_explicit, the Bézier control point for leaving this knot has already been computed; it is in the mp_right_x and mp_right_y fields.
				mp_explicit,
				// If right.type = mp_given, the curve should leave the knot in a nonzero direction stored as an angle in right given.
				mp_given,
				// If right.type = mp_curl, the curve should leave the knot in a direction depending on the angle at which it enters the next knot and on the curl parameter stored in right curl.
				mp_curl,
				// If right.type = mp_open, the curve should leave the knot in the same direction it entered; METAPOST will figure out a suitable direction.
				mp_open,
				mp_end_cycle, // for internal use in mp_make_choices
			} type;
			int info; // for internal use in mp_make_envelope
		};
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

// The METAFONT path specification
//   z0 .. z1 .. tension atleast 1 .. {curl 2} z2
//      .. z3 {-1, -2} .. tension 3 and 4
//      .. z4 .. controls z45 and z54 .. z5
// will be represented by the six knots
//   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//   ┌───────── left ─────────┐       ┌──────── right ─────────┐
//   type     parameter tension coord type     parameter tension
//   ───────────────────────────────────────────────────────────
//   endpoint —         —       z0    curl     1.0       1.0
//   open     —         1.0     z1    open     —         −1.0
//   curl     2.0       −1.0    z2    curl     2.0       1.0
//   given    π+atan(2) 1.0     z3    given    π+atan(2) 3.0
//   open     —         4.0     z4    explicit x45*      y45*
//   explicit x54*      y54*    z5    endpoint —         —
//   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// * (x45, y45) and (x54, y54) are stored in the left.coord field and the right.coord field respectively. They should occupy the same storage as the parameter and tension fields.
// If a path is cyclic, there will be no knots with endpoint types.

void mp_print_path(const struct mp_knot *const head) {
	const struct mp_knot *p = head, *q;
	do {
		q = p->next;
		assert(p && q);
		printf("(%g, %g)", creal(p->coord), cimag(p->coord));
		switch (p->right.type) {
		case mp_endpoint:
			assert(p->left.type != mp_open);
			assert(q == head && q->left.type == mp_endpoint);
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
			// A curl of 1 is shown explicitly, so that the user sees clearly that the default curl is present.
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

// This is METAPOST's magic fudge factor for placing the first control point of a curve that starts at an angle θ and ends at an angle φ from the straight path.
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

static void mp_set_controls(struct mp_knot *p, struct mp_knot *q, double theta, double phi, double complex delta_k) {
	double complex eit = cexp(theta * I), eif = cexp(phi * I);
	// velocities, divided by thrice the tension
	double rr = mp_velocity(cimag(eit), creal(eit), cimag(eif), creal(eif), fabs(p->right.tension));
	double ss = mp_velocity(cimag(eif), creal(eif), cimag(eit), creal(eit), fabs(q->left.tension));
	// Decrease the velocities, if necessary, to stay inside the bounding triangle.
	// The boundedness conditions
	//   rr ≤ sin(φ)/sin(θ + φ)  and  ss ≤ sin(θ)/sin(θ + φ)
	// are to be enforced if sin(θ), sin(φ), and sin(θ + φ) all have the same sign.
	// Otherwise there is no “bounding triangle”.
	if ((p->right.tension < 0 || q->left.tension < 0) && cimag(eit) * cimag(eif) >= 0) {
		// sin(θ + φ) × what's this “safety factor”?
		double sine = cimag(eit * eif) * 1.000244140625;
		if (cimag(eit) < 0 || cimag(eif) < 0) sine = -sine;
		if (sine > 0) {
			if (p->right.tension < 0) rr = fmin(rr, fabs(cimag(eif) / sine));
			if (q->left.tension < 0) ss = fmin(ss, fabs(cimag(eit) / sine));
		}
	}
	q->left.type = mp_explicit;
	q->left.coord = q->coord - delta_k * conj(eif) * ss;
	p->right.type = mp_explicit;
	p->right.coord = p->coord + delta_k * eit * rr;
}

// Search for “mock curvature” in the METAFONT source document (weave mf.web) for more details of the mathematics involved.
static void mp_solve_choices(struct mp_knot *const p, struct mp_knot *const q, int n) {
	struct mp_knot *r = NULL, *s = p, *t = p->next;
	// Calculate the turning angles ψₖ and the distances.
	double complex delta[n + 1]; // knot differences
	double psi[n + 2]; // turning angles
	psi[n] = 0;
	for (int k = 0; k < n + (p->left.type == mp_end_cycle); k++) {
		delta[k] = s->next->coord - s->coord;
		if (k) psi[k] = carg(conj(delta[k - 1]) * delta[k]);
		s = s->next;
	}
	psi[n + 1] = psi[1];
	s = p;
	// Get the linear equations started; or return with the control points in place, if linear equations needn't be solved.
	// The first linear equation, if any, will have A₀ = B₀ = 0.
	double theta[n + 1], u[n], v[n + 1], w[n + 1];
	switch (s->right.type) {
	case mp_given:
		if (t->left.type == mp_given) {
			// Reduce to the simple case of two givens.
			mp_set_controls(p, q, p->right.parameter - carg(delta[0]), carg(delta[0]) - q->left.parameter, delta[0]);
			return;
		} else {
			// Set up the equation for a given value of θ₀.
			v[0] = reduce_angle(s->right.parameter - carg(delta[0]));
			u[0] = 0;
			w[0] = 0;
		}
		break;
	case mp_curl:
		if (t->left.type == mp_curl) {
			// Reduce to the simple case of straight line.
			p->right.type = mp_explicit;
			q->left.type = mp_explicit;
			p->right.coord = p->coord + delta[0] / (3 * fabs(p->right.tension));
			q->left.coord = q->coord - delta[0] / (3 * fabs(q->left.tension));
			return;
		} else {
			// Set up the equation for a curl at θ₀
			u[0] = mp_curl_ratio(s->right.parameter, fabs(s->right.tension), fabs(t->left.tension));
			v[0] = -psi[1] * u[0];
			w[0] = 0;
		}
		break;
	case mp_open:
		u[0] = v[0] = 0;
		w[0] = 1; // This begins a cycle.
		break;
	default:
		abort();
	}
	r = s;
	s = t;
	t = s->next;

	for (int k = 1; k <= n; k++, r = s, s = t, t = s->next) switch (s->left.type) {
	case mp_end_cycle:
	case mp_open:
		{
			// Set up equation to match mock curvatures at zₖ.
			double a = 1 / (3 * fabs(r->right.tension) - 1);
			double b = 1 / (3 * fabs(t->left.tension) - 1);
			double c = 1 - u[k - 1] * a;
			double d = cabs(delta[k]) * (3 - 1 / fabs(r->right.tension));
			double e = cabs(delta[k - 1]) * (3 - 1 / fabs(t->left.tension));
			double f = fabs(s->left.tension / s->right.tension);
			f = e / (e + c * d * f * f);

			u[k] = f * b;
			if (r->right.type == mp_curl) {
				v[k] = -psi[k + 1] * u[k] - psi[1] * (1 - f);
				w[k] = 0;
			} else {
				f = (1 - f) / c;
				v[k] = -psi[k + 1] * u[k] - (psi[k] + v[k - 1] * a) * f;
				w[k] = -w[k - 1] * f * a;
			}
		}
		break;
	case mp_curl:
		// Set up equation for a curl at θₙ.
		theta[n] = -v[n - 1] / (1 / mp_curl_ratio(s->left.parameter, fabs(s->left.tension), fabs(r->right.tension)) - u[n - 1]);
		break;
	case mp_given:
		// Calculate the given value of θₙ.
		theta[n] = reduce_angle(s->left.parameter - carg(delta[n - 1]));
		break;
	default:
		abort();
	}
	// Adjust θₙ to equal θ₀ if a cycle has ended.
	if (p->left.type == mp_end_cycle) {
		double a = 0, b = 1;
		for (int k = n - 1; k >= 1; k--) {
			a = v[k] - a * u[k];
			b = w[k] - b * u[k];
		}
		theta[n] = v[0] = (v[n] - a * u[n]) / (b * u[n] - w[n] + 1);
		for (int k = 1; k < n; k++) {
			v[k] += v[0] * w[k];
		}
	}
	// Finish choosing angles and assigning control points.
	for (int k = n - 1; k >= 0; k--) {
		theta[k] = v[k] - theta[k + 1] * u[k];
	}
	s = p;
	for (int k = 0; k < n; k++) {
		mp_set_controls(s, s->next, theta[k], -psi[k + 1] - theta[k + 1], delta[k]);
		s = s->next;
	}
}

void mp_make_choices(struct mp_knot *head) {
	struct mp_knot *p = head;
	do {
		struct mp_knot *q = p->next;
		// Knot types must satisfy certain restrictions because of the form of METAPOST's path syntax:
		// • mp_open never appears in the same node together with mp_endpoint, mp_given, or mp_curl.
		// • node->right.type is mp_explicit if and only if node->next->left.type is mp_explicit.
		// • mp_endpoint occur only at the ends.
		assert(p->left.type != mp_open || p->right.type == mp_open || p->right.type == mp_explicit);
		assert(p->right.type != mp_open || p->left.type == mp_open || p->left.type == mp_explicit);
		assert((p->right.type == mp_explicit) == (q->left.type == mp_explicit));
		assert(p == head || p->left.type != mp_endpoint);
		assert(q == head || p->right.type != mp_endpoint);
		// Join equal consecutive knots with an explicit “curve” whose control points are identical to the knots.
		if (p->coord == q->coord && p->right.type > mp_explicit) {
			p->right.type = mp_explicit;
			p->right.coord = p->coord;
			if (p->left.type == mp_open) {
				p->left.type = mp_curl;
				p->left.parameter = 1;
			}
			q->left.type = mp_explicit;
			q->left.coord = p->coord;
			if (q->right.type == mp_open) {
				q->right.type = mp_curl;
				q->right.parameter = 1;
			}
		}
		p = q;
	} while (p != head);

	// Find the first breakpoint, whose left and right types are neither mp_open.
	struct mp_knot *h = head;
	while (h->left.type == mp_open && h->right.type == mp_open) {
		h = h->next;
		// Temporarily change head->left.type to mp_end_cycle if there are no breakpoints.
		if (h == head) {
			h->left.type = mp_end_cycle;
			break;
		}
	}

	p = h;
	do {
		// Fill in the control points between p and the next breakpoint
		struct mp_knot *q = p->next;
		if (p->right.type > mp_explicit) {
			while (q->left.type == mp_open && q->right.type == mp_open) q = q->next;
			// Count the knots.
			int n = 0; // length of the path
			struct mp_knot *s = p;
			do {
				n++;
				s = s->next;
			} while (s != q);
			// Remove mp_open types at the breakpoints.
			if (p->right.type == mp_open && p->left.type == mp_explicit) {
				if (p->coord == p->left.coord) {
					// If the velocity coming into this knot is zero, the mp_open type is converted to a mp_curl, since we don't know the incoming direction.
					p->right.type = mp_curl;
					p->right.parameter = 1;
				} else {
					// The mp_open type is converted to mp_given.
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
			// Give reasonable values for the unused control points between p and q.
			// This step makes it possible to transform an explicitly computed path without checking the mp_left_type and mp_right_type fields.
			p->right.coord = p->coord;
			q->left.coord = q->coord;
		}
		// Advance p to the next breakpoint.
		p = q;
	} while (p != h);
}

#define t_of_the_way(a, b) ((1 - t) * (a) + t * (b))
// GLSL  genType mix(genType x, genType y, float a);
// HLSL  ret lerp(x, y, s)
// C++20  constexpr Promoted lerp(Arithmetic1 a, Arithmetic2 b, Arithmetic3 t) noexcept;
// Unity  public static float Lerp(float a, float b, float t);

double complex mp_transform(double complex z, double complex tx, double complex ty, double complex ta) {
	return creal(z) * tx + cimag(z) * ty + ta;
}

void mp_transform_path(struct mp_knot *head, double complex tx, double complex ty, double complex ta) {
	struct mp_knot *p = head;
	do {
		if (p->left.type != mp_endpoint) {
			p->left.coord = mp_transform(p->left.coord, tx, ty, ta);
		}
		p->coord = mp_transform(p->coord, tx, ty, ta);
		if (p->right.type != mp_endpoint) {
			p->right.coord = mp_transform(p->right.coord, tx, ty, ta);
		}
	} while (p != head);
}

// This function is also called htap_ypoc, for the reverse of copy_path.
struct mp_knot *mp_reverse_path(struct mp_knot *head) {
	struct mp_knot *p = head, *q = head->next;
	do {
		struct mp_knot tmp = *p, *r = q->next;
		q->next = p;
		p->left = tmp.right;
		p->right = tmp.left;
		p = q;
		q = r;
	} while (p != head);
	return p->right.type == mp_endpoint ? p->next : p;
}

// mp_crossing_point returns the unique fraction 0 ≤ t ≤ 1 at which B(a, b, c; t) changes from positive to negative, or t > 1 if no such value exists. If a < 0 (so that the polynomial is already negative at t = 0), zero is returned.
double mp_crossing_point(double a, double b, double c) {
	if (a < 0) return 0;
	if (c >= 0){
		if (b >= 0) {
			if (c > 0) return nextafter(1, INFINITY);
			if (!a && !b) return nextafter(1, INFINITY);
			return 1;
		}
		if (!a) return 0;
	} else if (!a && b <= 0) return 0;
	c += a - b * 2;
	b -= a;
	a = -(sqrt(b * b - a * c) + b) / c;
	return isnan(a) ? nextafter(1, INFINITY) : a;
}

double complex bernstein3(double z0, double z0p, double z1m, double z1, double t) {
	double mt = 1 - t;
	double t2 = t * t, mt2 = mt * mt;
	return mt2 * (mt * z0 + 3 * t * z0p) + t2 * (3 * mt * z1m + t * z1);
}

// mp_bound_cubic updates *min and *max based on B(x0, x0p, x1m, x1; t) for 0 < t ≤ 1.
void mp_bound_cubic(double x0, double x0p, double x1m, double x1, double *const min, double *const max) {
	*min = fmin(*min, x1);
	*max = fmax(*max, x1);
	// Return if there is no need to look for extremes.
	if (x0p >= *min && x0p <= *max && x1m >= *min && x1m <= *max) return;

	// proportional to the control points of a quadratic derived from a cubic
	double del1 = x0p - x0, del2 = x1m - x0p, del3 = x1 - x1m;
	// If del1 = del2 = del3 = 0, the cubic collapses into a point. We just set del = 0 in that case.
	double del = del1 ? del1 : del2 ? del2 : del3;
	if (del < 0) {
		del1 = -del1;
		del2 = -del2;
		del3 = -del3;
	}
	double t = mp_crossing_point(del1, del2, del3);
	if (t >= 1) return;
	// Test the first extreme of the cubic against the bounding box.
	double x = bernstein3(x0, x0p, x1m, x1, t);
	*min = fmin(*min, x);
	*max = fmax(*max, x);

	// Since mp_crossing_point has tried to choose t so that B(del1, del2, del3; τ) crosses zero at τ = t with negative slope, del2 should not be positive. But rounding error could make it slightly positive in which case we must cut it to zero to avoid confusion.
	del2 = fmin(0, t_of_the_way(del2, del3));
	// Now (0, del2, del3) represent the derivative on the remaining interval.
	double tt = mp_crossing_point(0, -del2, -del3);
	if (tt >= 1) return;
	// Test the second extreme against the bounding box.
	x = bernstein3(x0, x0p, x1m, x1, t_of_the_way(tt, 1));
	*min = fmin(*min, x);
	*max = fmax(*max, x);
}

void mp_path_bbox(struct mp_knot *head, double complex *const llcorner, double complex *const urcorner) {
	// Finding the bounding box of a path is basically a matter of applying bound cubic twice for each pair of adjacent knots.
	struct mp_knot *p = head, *q;
	double *const minx = (double *) llcorner;
	double *const miny = minx + 1;
	double *const maxx = (double *) urcorner;
	double *const maxy = maxx + 1;
	*llcorner = *urcorner = p->coord;
	do {
		if (p->right.type == mp_endpoint) return;
		q = p->next;
		mp_bound_cubic(creal(p->coord), creal(p->right.coord), creal(q->left.coord), creal(q->coord), minx, maxx);
		mp_bound_cubic(cimag(p->coord), cimag(p->right.coord), cimag(q->left.coord), cimag(q->coord), miny, maxy);
		p = q;
	} while (p != head);
}

// Split a Bézier curve between p and p->next at time t using de Casteljau's algorithm.
struct mp_knot *mp_split_cubic(struct mp_knot *p, double t) {
	struct mp_knot *q = p->next, *r = malloc(sizeof(*r));
	p->next = r;
	r->next = q;
	r->left.type = mp_explicit;
	r->right.type = mp_explicit;
	// When a cubic is split at a fraction value t, we obtain two cubics whose Bézier control points are obtained by a generalization of the bisection process.
	double complex v = t_of_the_way(p->right.coord, q->left.coord);
	p->right.coord = t_of_the_way(p->coord, p->right.coord);
	q->left.coord = t_of_the_way(q->left.coord, q->coord);
	r->left.coord = t_of_the_way(p->right.coord, v);
	r->right.coord = t_of_the_way(v, q->left.coord);
	r->coord = t_of_the_way(r->left.coord, r->right.coord);
	return r;
}
#include "tmp.c"

int main() {
	// To see how METAFONT solves a path, run
	//   mf \\tracingchoices:=tracingonline:=1;path p;
	// and say “p := ⟨path expression⟩”.
	// MetaPost has some bugs printing numbers in paths as of 2.01.
	#define tesp(z, x, y) \
	if (cabs((z) - ((x) + (y) * I)) > 2e-3) \
	printf("Test failure on line %d: expected (%g, %g), but got (%g, %g)\n", \
			__LINE__, (double) (x), (double) (y), \
			creal(z), cimag(z))
	#define test(control, x, y) tesp((control).coord, x, y)
	double complex ll, ur;
	// (0, 0)
	// .. (2, 20){curl 1}
	// .. {curl 1}(10, 5) .. controls (2, 2)
	// and (9, 4.5) .. (3, 10) .. tension 3
	// and atleast 4 .. (1, 14){2, 0}
	// .. {0, 1}(5, -4)
	struct mp_knot z1 = {
		.coord = 0,
		.left.type = mp_endpoint,
		.right.type = mp_curl,
		.right.parameter = 1,
		.right.tension = 1,
	};
	struct mp_knot z2 = {
		.coord = 2+20*I,
		.left.type = mp_curl,
		.left.parameter = 1,
		.left.tension = 1,
		.right.type = mp_curl,
		.right.parameter = 1,
		.right.tension = 1,
	};
	struct mp_knot z3 = {
		.coord = 10+5*I,
		.left.type = mp_curl,
		.left.parameter = 1,
		.left.tension = 1,
		.right.type = mp_explicit,
		.right.parameter = 2,
		.right.tension = 2,
	};
	struct mp_knot z4 = {
		.coord = 3+10*I,
		.left.type = mp_explicit,
		.left.coord = 9+4.5*I,
		.right.type = mp_open,
		.right.tension = 3,
	};
	struct mp_knot z5 = {
		.coord = 1+14*I,
		.left.type = mp_given,
		.left.parameter = 0,
		.left.tension= -4,
		.right.type = mp_given,
		.right.parameter = 0,
		.right.tension = 1,
	};
	struct mp_knot z6 = {
		.coord = 5-4*I,
		.left.type = mp_given,
		.left.parameter = asin(1),
		.left.tension = 1,
		.right.type = mp_endpoint,
	};
	z1.next = &z2;
	z2.next = &z3;
	z3.next = &z4;
	z4.next = &z5;
	z5.next = &z6;
	z6.next = &z1;
	mp_make_choices(&z1);
	test(z1.right, 0.66667, 6.66667);
	test(z2.left, 1.33333, 13.33333);
	test(z2.right, 4.66667, 15);
	test(z3.left, 7.33333, 10);
	test(z3.right, 2, 2);
	test(z4.left, 9, 4.5);
	test(z4.right, 2.34547, 10.59998);
	test(z5.left, 0.48712, 14);
	test(z5.right, 13.40117, 14);
	test(z6.left, 5, -35.58354);
	// show llcorner p; show urcorner p; % MetaPost only
	mp_path_bbox(&z1, &ll, &ur);
	tesp(ll, 0, -14.52234);
	tesp(ur, 10, 20);

	// (0, 0){dir 60°} ... {dir -10°}(400, 0)
	struct mp_knot z7 = {
		.coord = 0,
		.left.type = mp_endpoint,
		.right.type = mp_given,
		.right.parameter = acos(.5),
		.right.tension = -1,
	};
	struct mp_knot z8 = {
		.coord = 400,
		.left.type = mp_given,
		.left.parameter = asin(-1) / 9,
		.left.tension = -1,
		.right.type = mp_endpoint,
	};
	z7.next = &z8;
	z8.next = &z7;
	mp_make_choices(&z7);
	test(z7.right, 36.94897, 63.99768);
	test(z8.left, 248.95918, 26.63225);
	// show subpath (0, .3) of p & subpath (.3, 1) of p;
	mp_split_cubic(&z7, .3);
	test(z7.right, 11.08481, 19.1995);
	test(z7.next->left, 37.92545,29.27612);
	tesp(z7.next->coord, 74.1489, 33.25658);
	test(z7.next->right, 158.66904, 42.54419);
	test(z8.left, 294.2719, 18.64249);
	// Revert the changes due to mp_split_cubic().
	z7.next = &z8;
	z7.right.coord = 36.94897 + 63.99768 * I;
	z8.left.coord = 248.95918 + 26.63225 * I;
	// show envelope makepen ((0, -1) -- (3, -1) -- (6, 1) -- (1, 2) -- cycle) of p; (MetaPost only)
	double complex p1[4] = {-I, 3-I, 6+I, 1+2*I};
	struct mp_knot *e1 = mp_make_envelope(&z7, p1, 4);
	struct mp_knot *e2 = e1->next;
	struct mp_knot *e3 = e2->next;
	struct mp_knot *e4 = e3->next;
	struct mp_knot *e5 = e4->next;
	struct mp_knot *e6 = e5->next;
	struct mp_knot *e7 = e6->next;
	struct mp_knot *e8 = e7->next;
	struct mp_knot *e9 = e8->next;
	struct mp_knot *e10 = e9->next;
	struct mp_knot *e11 = e10->next;
	struct mp_knot *e12 = e11->next;
	assert(e12->next == e1);
	test(*e1, 1, 2);
	test(e1->right, 1, 2);
	test(e2->left, 0, -1);
	test(*e2, 0, -1);
	test(e2->right, 0, -1);
	test(e3->left, 3, -1);
	test(*e3, 3, -1);
	test(e3->right, 3, -1);
	test(e4->left, 6, 1);
	test(*e4, 6, 1);
	test(e4->right, 9.54506, 7.14023);
	test(e5->left, 14.70161, 12.34738);
	test(*e5, 21.26122, 16.72046);
	test(e5->right, 21.26122, 16.72046);
	test(e6->left, 18.26122, 14.72046);
	test(*e6, 18.26122, 14.72046);
	test(e6->right, 39.56245, 28.92128);
	test(e7->left, 75.65904, 34.32655);
	test(*e7, 119.41237, 34.32655);
	test(e7->right, 119.41237, 34.32655);
	test(e8->left, 116.41237, 34.32655);
	test(*e8, 116.41237, 34.32655);
	test(e8->right, 199.61642, 34.32655);
	test(e9->left, 310.50972, 14.77936);
	test(*e9, 400, -1);
	test(e9->right, 400, -1);
	test(e10->left, 403, -1);
	test(*e10, 403, -1);
	test(e10->right, 403, -1);
	test(e11->left, 406, 1);
	test(*e11, 406, 1);
	test(e11->right, 406, 1);
	test(e12->left, 401, 2);
	test(*e12, 401, 2);
	test(e12->right, 249.95918, 28.63225);
	test(e1->left, 37.94897, 65.99768);
	free(e1);


	// (0, 0) .. (10, 10) .. (10, -5) .. cycle
	struct mp_knot z9 = {
		.coord = 0,
		.left.type = mp_open,
		.left.tension = 1,
		.right.type = mp_open,
		.right.tension = 1,
	};
	struct mp_knot z10 = {
		.coord = 10+10*I,
		.left.type = mp_open,
		.left.tension = 1,
		.right.type = mp_open,
		.right.tension = 1,
	};
	struct mp_knot z11 = {
		.coord = 10-5*I,
		.left.type = mp_open,
		.left.tension = 1,
		.right.type = mp_open,
		.right.tension = 1,
	};
	z9.next = &z10;
	z10.next = &z11;
	z11.next = &z9;
	mp_make_choices(&z9);
	test(z9.right, -1.8685, 6.35925);
	test(z10.left, 4.02429, 12.14362);
	test(z10.right, 16.85191, 7.54208);
	test(z11.left, 16.9642, -2.22969);
	test(z11.right, 5.87875, -6.6394);
	test(z9.left, 1.26079, -4.29094);
	mp_reverse_path(&z9);
	assert(z9.next == &z11);
	assert(z10.next == &z9);
	assert(z11.next == &z10);
	test(z9, 0, 0);
	test(z10, 10, 10);
	test(z11, 10, -5);
	test(z9.left, -1.8685, 6.35925);
	test(z10.right, 4.02429, 12.14362);
	test(z10.left, 16.85191, 7.54208);
	test(z11.right, 16.9642, -2.22969);
	test(z11.left, 5.87875, -6.6394);
	test(z9.right, 1.26079, -4.29094);
	// show llcorner p; show urcorner p;
	mp_reverse_path(&z9);
	mp_path_bbox(&z9, &ll, &ur);
	tesp(ll, -0.35213, -5.5248);
	tesp(ur, 15.18112, 10.45483);
	// show envelope pensquare scaled 2 of p; (MetaPost only)
	// This gives the inner edge because the path goes clockwise.
	p1[0] = -1-I;
	p1[1] = 1-I;
	p1[2] = 1+I;
	p1[3] = -1+I;
	e1 = mp_make_envelope(&z9, p1, 4);
	e2 = e1->next;
	e3 = e2->next;
	e4 = e3->next;
	e5 = e4->next;
	e6 = e5->next;
	e7 = e6->next;
	e8 = e7->next;
	e9 = e8->next;
	e10 = e9->next;
	e11 = e10->next;
	e12 = e11->next;
	test(*e1, 1, 1);
	test(e1->right, 0.75981, 1.81746);
	test(e2->left, 0.64787, 2.62543);
	test(*e2, 0.64787, 3.40826);
	test(e2->right, 0.64787, 3.40826);
	test(e3->left, 0.64787, 1.40826);
	test(*e3, 0.64787, 1.40826);
	test(e3->right, 0.64787, 5.83273);
	test(e4->left, 4.22353, 9.45483);
	test(*e4, 8.43034, 9.45483);
	test(e4->right, 8.43034, 9.45483);
	test(e5->left, 6.43034, 9.45483);
	test(*e5, 6.43034, 9.45483);
	test(e5->right, 7.26956, 9.45483);
	test(e6->left, 8.13391, 9.31068);
	test(*e6, 9, 9);
	test(e6->right, 12.43988, 7.76605);
	test(e7->left, 14.18112, 4.68874);
	test(*e7, 14.18112, 1.57939);
	test(e7->right, 14.18112, 1.57939);
	test(e8->left, 14.18112, 3.57939);
	test(*e8, 14.18112, 3.57939);
	test(e8->right, 14.18112, 0.49522);
	test(e9->left, 12.46796, -2.62047);
	test(*e9, 9, -4);
	test(e9->right, 8.10278, -4.3569);
	test(e10->left, 7.18204, -4.5248);
	test(*e10, 6.27751, -4.5248);
	test(e10->right, 6.27751, -4.5248);
	test(e11->left, 8.27751, -4.5248);
	test(*e11, 8.27751, -4.5248);
	test(e11->right, 5.02718, -4.5248);
	test(e1->left, 1.98631, -2.35678);
	free(e1);

	// (1, 1) .. (4, 5) .. tension atleast 1  {curl 2}(1, 4)
	// .. (19, -1){-1, -2} .. tension 3 and 4 .. (9, -8)
	// .. controls (4, 5) and (5, 4) .. (1, 0)
	struct mp_knot z12 = {
		.coord = 1+1*I,
		.left.type = mp_endpoint,
		.right.type = mp_curl,
		.right.parameter = 1,
		.right.tension = 1,
	};
	struct mp_knot z13 = {
		.coord = 4+5*I,
		.left.type = mp_open,
		.left.tension = 1,
		.right.type = mp_open,
		.right.tension = -1,
	};
	struct mp_knot z14 = {
		.coord = 1+4*I,
		.left.type = mp_curl,
		.left.parameter = 2,
		.left.tension = -1,
		.right.type = mp_curl,
		.right.parameter = 2,
		.right.tension = 1,
	};
	struct mp_knot z15 = {
		.coord = 19-1*I,
		.left.type = mp_given,
		.left.parameter = atan2(-2, -1),
		.left.tension = 1,
		.right.type = mp_given,
		.right.parameter = atan2(-2, -1),
		.right.tension = 3,
	};
	struct mp_knot z16 = {
		.coord = 9-8*I,
		.left.type = mp_open,
		.left.tension = 4,
		.right.type = mp_explicit,
		.right.coord = 4+5*I,
	};
	struct mp_knot z17 = {
		.coord = 1,
		.left.type = mp_explicit,
		.left.coord = 5+4*I,
		.right.type = mp_endpoint,
	};
	z12.next = &z13;
	z13.next = &z14;
	z14.next = &z15;
	z15.next = &z16;
	z16.next = &z17;
	z17.next = &z12;
	mp_make_choices(&z12);
	test(z12.right, 3.51616, -0.21094);
	test(z13.left, 5.86702, 2.92355);
	test(z13.right, 2.73756, 6.40405);
	test(z14.left, 0.7107, 5.41827);
	test(z14.right, -5.32942, 20.68242);
	test(z15.left, 29.16524, 19.33046);
	test(z15.right, 17.90521, -3.18956);
	test(z16.left, 9.42474, -9.10431);
	test(z16.right, 4, 5);
	test(z17.left, 5, 4);

	// (0, 0) .. controls (0, 0.5) and (0, 1) .. (0, 1)
	// .. (1, 1) .. controls (1, 1) and (1, 0.5) .. (1, 0){dir 45°} .. (2, 0)
	struct mp_knot z18 = {
		.coord = 0,
		.left.type = mp_endpoint,
		.right.type = mp_explicit,
		.right.coord = .5*I,
	};
	struct mp_knot z19 = {
		.coord = I,
		.left.type = mp_explicit,
		.left.coord = I,
		.right.type = mp_open,
		.right.tension = 1,
	};
	struct mp_knot z20 = {
		.coord = 1+I,
		.left.type = mp_open,
		.left.tension = 1,
		.right.type = mp_explicit,
		.right.coord = 1+I,
	};
	struct mp_knot z21 = {
		.coord = 1,
		.left.type = mp_explicit,
		.left.coord = 1+.5*I,
		.right.type = mp_given,
		.right.parameter = atan(1),
		.right.tension = 1,
	};
	struct mp_knot z22 = {
		.coord = 2,
		.left.type = mp_curl,
		.left.parameter = 1,
		.left.tension = 1,
		.right.type = mp_endpoint,
	};
	z18.next = &z19;
	z19.next = &z20;
	z20.next = &z21;
	z21.next = &z22;
	z22.next = &z18;
	mp_make_choices(&z18);
	test(z18.right, 0, 0.5);
	test(z19.left, 0, 1);
	test(z19.right, 0.33333, 1);
	test(z20.left, 0.66667, 1);
	test(z20.right, 1, 1);
	test(z21.left, 1, 0.5);
	test(z21.right, 1.27614, 0.27614);
	test(z22.left, 1.72386, 0.27614);

	// (1.3, 3.7) .. cycle
	struct mp_knot z23 = {
		.coord = 1.3+3.7*I,
		.left.type = mp_open,
		.left.tension = 1,
		.right.type = mp_open,
		.right.tension = 1,
	};
	z23.next = &z23;
	mp_make_choices(&z23);
	test(z23.left, 1.3, 3.7);
	test(z23.right, 1.3, 3.7);
	mp_reverse_path(&z23);
	assert(z23.next == &z23);
	test(z23, 1.3, 3.7);
	test(z23.left, 1.3, 3.7);
	test(z23.right, 1.3, 3.7);

	// (1.3, 3.7) .. (1.3, 3.7) .. (1.3, 3.7)
	struct mp_knot z24 = {
		.coord = 1.3+3.7*I,
		.left.type = mp_endpoint,
		.right.type = mp_curl,
		.right.parameter = 1,
		.right.tension = 1,
	};
	struct mp_knot z25 = {
		.coord = 1.3+3.7*I,
		.left.type = mp_open,
		.left.tension = 1,
		.right.type = mp_open,
		.right.tension = 1,
	};
	struct mp_knot z26 = {
		.coord = 1.3+3.7*I,
		.left.type = mp_curl,
		.left.parameter = 1,
		.left.tension = 1,
		.right.type = mp_endpoint,
	};
	z24.next = &z25;
	z25.next = &z26;
	z26.next = &z24;
	mp_make_choices(&z24);
	test(z24.right, 1.3, 3.7);
	test(z25.left, 1.3, 3.7);
	test(z25.right, 1.3, 3.7);
	test(z26.left, 1.3, 3.7);
	#undef test
	assert(fabs(mp_crossing_point(2, .5, -3) - .5) < 1e-5);
	assert(fabs(mp_crossing_point(2, .5, -4) - (sqrt(11.0 / 12.0) - .5)) < 1e-5);
	assert(mp_crossing_point(2, .5, 6) > 1);
}
