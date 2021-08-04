//
// Altium Designer cannot output Gerber files with 6 decimal digits.
// Autodesk EAGLE has no native support for rout data in the drill files.
// KiCad cannot find similar objects or autoroute natively.
// EasyEDA outputs Gerber files in inches with 5 decimal digits.
// Minimal Board Editor vector-fills regions and approximates arcs with segments.
// MeowCAD looks more like an unfinished proof-of-concept than a usable program.
// None of the software in the market provides a convenient way to fill a shape with holes in it.

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <complex.h>
#include <time.h>
#include <assert.h>

struct gbr_file {
	FILE *fp;
	int pen_count;
	int current_g;
	unsigned set_attributes;
	bool arced, painting;
	double complex current_z;
	double complex path_start_z;
	double complex translation; // for gbr_begin_repeat
};

const double degree = 0.017453292519943295; // π / 180
const double mil = 0.0254;
const double in = 25.4;
const char *gbr_attributes[] = {
	"A.AperFunction",
	"A.DrillTolerance",
	"A.FlashText",
	"O.C",
	"O.P",
	"O.N",
};

const char *iso8601(time_t t) {
	static char buffer[24];
	strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
	return buffer;
}

#define _Imaginary_I I
#define CMPLX(x, y) ((double complex) ((double) (x) + _Imaginary_I * (double) (y)))

double complex cpolar(double magnitude, double phase) {
	assert(magnitude >= 0 && isfinite(phase));
	return CMPLX(magnitude * cos(phase), magnitude * sin(phase));
}

double cnorm(double complex z) {
	return creal(z) * creal(z) + cimag(z) * cimag(z);
}

void gbr_begin_file(struct gbr_file *this, const char *filename, const char *file_function) {
	assert(file_function && *file_function);
	this->fp = fopen(filename, "wb");
	if (!this->fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	fputs("%FSLAX66Y66*%\n%MOMM*%\n", this->fp);
	fprintf(this->fp, "%%TF.FileFunction,%s*%%\n", file_function);
	if (strncmp(file_function, "Soldermask,", 11) == 0) {
		fputs("%TF.FilePolarity,Negative*%\n", this->fp);
	} else if (strncmp(file_function, "Profile,", 8) != 0 && strchr("CGHLPST", *file_function)) {
		fputs("%TF.FilePolarity,Positive*%\n", this->fp);
	}
	fputs("%TF.SameCoordinates*%\n%TF.GenerationSoftware,Aniced,PCBAD,0.0*%\n", this->fp);
	fprintf(this->fp, "%%TF.CreationDate,%s*%%\n", iso8601(time(NULL)));
	this->pen_count = 10;
	this->arced = false;
	this->painting = true;
	this->current_g = INT_MIN;
	this->set_attributes = 0;
	this->current_z = this->path_start_z = CMPLX(NAN, NAN);
	this->translation = CMPLX(-0.0, -0.0);
}

void gbr_end_file(struct gbr_file *this) {
	fputs("M02*\n", this->fp);
	fclose(this->fp);
	this->fp = NULL;
}

void gbr_set_attribute(struct gbr_file *this, unsigned id, const char *value) {
	if (value && *value) fprintf(this->fp, "%%T%s,%s*%%\n", gbr_attributes[id], value);
	this->set_attributes |= 1 << id;
}

void gbr_clear_attributes(struct gbr_file *this, unsigned mask) {
	if (!this->set_attributes) return;
	if ((this->set_attributes & mask) == this->set_attributes) {
		fputs("%TD*%\n", this->fp);
	} else {
		for (unsigned i = 0; i < sizeof(unsigned) * CHAR_BIT; i++) {
			if (this->set_attributes & mask & (1 << i)) {
				fprintf(this->fp, "%%TD%s*%%\n", gbr_attributes[i] + 1);
			}
		}
	}
	this->set_attributes = 0;
}

int gbr_begin_pen(struct gbr_file *this, const char *pen_function) {
	assert(this->pen_count >= 10);
	gbr_set_attribute(this, 0, pen_function);
	fprintf(this->fp, "%%ADD%d", this->pen_count);
	return this->pen_count;
}

void gbr_end_pen(struct gbr_file *this) {
	fputs("*%\n", this->fp);
	gbr_clear_attributes(this, UINT_MAX);
	this->pen_count++;
}

int gbr_select_pen(struct gbr_file *this, int pen) {
	assert(pen < this->pen_count);
	fprintf(this->fp, "D%d*\n", pen);
	return pen;
}

void gbr_set_paint_or_erase(struct gbr_file *this, bool paint) {
	if (this->painting == paint) return;
	fprintf(this->fp, "%%LP%c*%%\n", paint ? 'D' : 'C');
	this->painting = paint;
}

void gbr_begin_region(struct gbr_file *this, const char *pen_function) {
	gbr_set_attribute(this, 0, pen_function);
	fputs("G36*\n", this->fp);
	this->path_start_z = this->current_z;
}

void gbr_end_region(struct gbr_file *this) {
	assert(this->current_z == this->path_start_z);
	fputs("G37*\n", this->fp);
	gbr_clear_attributes(this, 1 << 0);
	this->current_z = this->path_start_z = CMPLX(NAN, NAN);
}

static bool gbr_xy(struct gbr_file *this, double complex z) {
	assert(isfinite(creal(z)) && isfinite(cimag(z)));
	if (this->current_z == z) return false;
	if (creal(this->current_z) != creal(z)) {
		fprintf(this->fp, "X%.0f", creal(z + this->translation) * 1e6);
	}
	if (cimag(this->current_z) != cimag(z)) {
		fprintf(this->fp, "Y%.0f", cimag(z + this->translation) * 1e6);
	}
	this->current_z = z;
	return true;
}

void gbr_move_to(struct gbr_file *this, double complex z) {
	if (gbr_xy(this, z)) fputs("D02*\n", this->fp);
	this->path_start_z = z;
}

void gbr_line_to(struct gbr_file *this, double complex z) {
	assert(isfinite(creal(this->current_z)) && isfinite(cimag(this->current_z)));
	if (this->current_g != 1) fprintf(this->fp, "G%02d*\n", (this->current_g = 1));
	if (gbr_xy(this, z)) fputs("D01*\n", this->fp);
}

void gbr_circle_here(struct gbr_file *this, double complex c) {
	assert(isfinite(creal(this->current_z)) && isfinite(cimag(this->current_z)));
	assert(isfinite(creal(c)) && isfinite(cimag(c)));
	if (!this->arced) {
		fputs("G75*\n", this->fp);
		this->arced = true;
	}
	if (this->current_g != 2 && this->current_g != 3) {
		fprintf(this->fp, "G%02d*\n", (this->current_g = 2));
	}
	c -= this->current_z;
	fprintf(this->fp, "I%.0fJ%.0fD01*\n", creal(c) * 1e6, cimag(c) * 1e6);
}

void gbr_arc_to(struct gbr_file *this, double complex z1, double complex c, bool clockwise) {
	assert(isfinite(creal(this->current_z)) && isfinite(cimag(this->current_z)));
	assert(isfinite(creal(c)) && isfinite(cimag(c)));
	if (cabs(this->current_z - z1) <= 0.001) {
		gbr_line_to(this,z1);
		return;
	}
	if (!this->arced) {
		fputs("G75*\n", this->fp);
		this->arced = true;
	}
	if (this->current_g != 3 - clockwise) fprintf(this->fp, "G%02d*\n", (this->current_g = 3 - clockwise));
	c -= this->current_z;
	gbr_xy(this, z1);
	fprintf(this->fp, "I%.0fJ%.0fD01*\n", creal(c) * 1e6, cimag(c) * 1e6);
}

void gbr_arc(struct gbr_file *this, double complex c, double r, double start_angle, double end_angle) {
	assert(isfinite(creal(c)) && isfinite(cimag(c)) && isfinite(r));
	assert(isfinite(start_angle) && isfinite(end_angle));
	if (r < 0) {
		r = -r;
		start_angle += 180;
		end_angle += 180;
	}
	start_angle *= degree;
	end_angle *= degree;
	gbr_move_to(this, c + cpolar(r, start_angle));
	if (fabs(start_angle - end_angle) >= 2 * acos(-1)) {
		gbr_circle_here(this, c);
	} else if (r * fabs(start_angle - end_angle) <= 0.001) {
		gbr_line_to(this, c + cpolar(r, end_angle));
	} else {
		gbr_arc_to(this, c + cpolar(r, end_angle), c, start_angle > end_angle);
	}
}

void gbr_close_path(struct gbr_file *this) {
	assert(isfinite(creal(this->path_start_z)) && isfinite(cimag(this->path_start_z)));
	gbr_line_to(this, this->path_start_z);
}

void gbr_draw_dot(struct gbr_file *this, double complex z) {
	gbr_xy(this, z);
	fputs("D03*\n", this->fp);
}

void gbr_begin_repeat(struct gbr_file *this, int nx, int ny, double complex pitch) {
	assert(!this->translation && signbit(creal(this->translation)) && signbit(cimag(this->translation)));
	assert(nx > 0 && ny > 0 && (nx > 1 || ny > 1));
	assert(isfinite(creal(pitch)) && isfinite(cimag(pitch)) && pitch);
	fprintf(this->fp, "%%SRX%dY%dI%fJ%f*%%\n", nx, ny, fabs(creal(pitch)), fabs(cimag(pitch)));
	this->translation = 0;
	if (creal(pitch) < 0) this->translation += (nx - 1) * creal(pitch);
	if (cimag(pitch) < 0) this->translation += (ny - 1) * cimag(pitch) * I;
	this->current_z = this->path_start_z = CMPLX(NAN, NAN);
}

void gbr_end_repeat(struct gbr_file *this) {
	assert(this->translation || !signbit(creal(this->translation)) || !signbit(cimag(this->translation)));
	fprintf(this->fp, "%%SR*%%\n");
	this->translation = CMPLX(-0.0, -0.0);
	this->current_z = this->path_start_z = CMPLX(NAN, NAN);
}

double complex bernstein3(double complex z0, double complex z0p, double complex z1m, double complex z1, double t) {
	double mt = 1 - t;
	double t2 = t * t, mt2 = mt * mt;
	return mt2 * (mt * z0 + 3 * t * z0p) + t2 * (3 * mt * z1m + t * z1);
}

// arc from three points in order
struct arc {
	double s, e, r, ts, te;
	double complex z;
};

double complex circle_center(double complex z0, double complex zm, double complex z1) {
	double d = ((creal(z1) - creal(zm)) * cimag(z0) + (creal(z0) - creal(z1)) * cimag(zm) + (creal(zm) - creal(z0)) * cimag(z1)) * 2;
	return CMPLX(
		creal(z1) * creal(z1) * (cimag(z0) - cimag(zm))
			+ (creal(z0) * creal(z0) + (cimag(z0) - cimag(zm)) * (cimag(z0) - cimag(z1))) * (cimag(zm) - cimag(z1))
			+ creal(zm) * creal(zm) * (cimag(z1) - cimag(z0)),
		cimag(z1) * cimag(z1) * (creal(zm) - creal(z0))
			+ (cimag(z0) * cimag(z0) + (creal(z0) - creal(zm)) * (creal(z0) - creal(z1))) * (creal(z1) - creal(zm))
			+ cimag(zm) * cimag(zm) * (creal(z0) - creal(z1))
	) / d;
}

void gbr_cubic_bezier_to(struct gbr_file *this, const double complex z0p, const double complex z1m, const double complex z1) {
	// https://pomax.github.io/bezierinfo/#arcapproximation
	const double complex z0 = this->current_z;
	double ts = 0, te;
	do {
		// step 1: start with the maximum possible arc
		te = 1;
		double complex zs = bernstein3(z0, z0p, z1m, z1, ts);
		struct arc arc, prev_arc;
		arc.s = NAN;
		bool currGood = false, prevGood;
		bool done = false;
		double prev_e = 1;
		// step 2: find the best possible arc
		int safety = 0;
		do {
			prevGood = currGood;
			prev_arc = arc;
			double q = (te - ts) / 4;
			double tm = ts + q * 2;

			double complex zm = bernstein3(z0, z0p, z1m, z1, tm);
			double complex ze = bernstein3(z0, z0p, z1m, z1, te);

			arc.z = circle_center(zs, zm, ze);
			arc.r = cabs(arc.z - zs);

			// arc start/end angles
			arc.s = carg(zs - arc.z);
			double d = carg(zm - arc.z);
			arc.e = carg(ze - arc.z);
			// direction correction
			if (arc.s < arc.e && (d < arc.s || d > arc.e)) arc.e -= 2 * acos(-1);
			if (arc.s > arc.e && (d < arc.e || d > arc.s)) arc.e += 2 * acos(-1);

			double complex c1 = bernstein3(z0, z0p, z1m, z1, tm - q);
			double complex c2 = bernstein3(z0, z0p, z1m, z1, tm + q);
			currGood = fabs(cabs(arc.z - c1) - arc.r) + fabs(cabs(arc.z - c2) - arc.r) <= 0.01;

			done = prevGood && !currGood;
			if (!done) prev_e = te;

			// this arc is fine: try a wider arc
			if (currGood) {
				// if e is already at max, then we're done for this arc.
				if (te >= 1) {
					// make sure we cap at t=1
					arc.te = prev_e = 1;
					prev_arc = arc;
					// if we capped the arc segment to t=1 we also need to make sure that
					// the arc's end angle is correct with respect to the bezier end point.
					if (te > 1) arc.e += carg((z1 - arc.z) / cpolar(arc.r, arc.e));
					break;
				}
				// if not, move it up by half the iteration distance
				te += (te - ts) / 2;
			} else {
				// This is a bad arc: we need to move 'e' down to find a good arc
				te = tm;
			}
		} while (!done && safety++ < 100);
		if (safety >= 100) break;

		if (isnan(prev_arc.s)) prev_arc = arc;
		gbr_arc_to(this, prev_arc.z + cpolar(prev_arc.r, prev_arc.e), prev_arc.z, prev_arc.s > prev_arc.e);
		ts = prev_e;
	} while (te < 1);
}

#define a(x, y) a_[(y) * width + (x)]
void flood_fill(int a_[], int width, int height, int x, int y, int old_value, int new_value, bool dir8) {
	int stack[width * 6][2];
	stack[0][0] = x;
	stack[0][1] = y;
	int stack_size = 1;
	if (a(x, y) != old_value || old_value == new_value) return;
	while (stack_size--) {
		x = stack[stack_size][0];
		y = stack[stack_size][1];
		int x0 = x;
		while (x0 > 0 && a(x0 - 1, y) == old_value) x0--;
		int x1 = x0;
		while (x1 < width && a(x1, y) == old_value) a(x1++, y) = new_value;
		if (dir8) {
			if (x0 > 0) x0--;
			if (x1 < width) x1++;
		}
		if (y > 0) for (x = x0; x < x1; x++) if (a(x, y - 1) == old_value) {
			stack[stack_size][0] = x;
			stack[stack_size][1] = y - 1;
			stack_size++;
			while (x < x1 && a(x, y - 1) == old_value) x++;
		}
		if (y < height - 1) for (x = x0; x < x1; x++) if (a(x, y + 1) == old_value) {
			stack[stack_size][0] = x;
			stack[stack_size][1] = y + 1;
			stack_size++;
			while (x < x1 && a(x, y + 1) == old_value) x++;
		}
		assert(stack_size <= width * 6);
	}
}

// in XBM format
void gbr_draw_bitmap(struct gbr_file *this, double complex gbr_z, double complex gbr_pitch, const char *pen_function, int width, int height, const void *bits) {
	// https://yal.cc/grid-based-contour-traversal/
	int *a_ = calloc(width * height, sizeof(int));
	const bool painting = this->painting;
	// a(x, y)
	// 0 = unprocessed, transparent
	// 1 = unprocessed, ink
	// -2 = processed, transparent
	// -1 = processed, ink
	const int pitch = width / 8 + !!(width % 8);
	for (int y = 0; y < height; y++) {
		int x;
		for (x = 0; x < width / 8; x++) {
			for (int i = 0; i < 8; i++) {
				if (((const unsigned char *) bits)[y * pitch + x] & (1 << i)) a(x * 8 + i, height - 1 - y) = 1;
			}
		}
		for (int i = 0; i < width % 8; i++) {
			if (((const unsigned char *) bits)[y * pitch + x] & (1 << i)) a(x * 8 + i, height - 1 - y) = 1;
		}
	}
	for (int x = 0; x < width; x++) {
		flood_fill(a_, width, height, x, 0, 0, -2, true);
		flood_fill(a_, width, height, x, height - 1, 0, -2, true);
	}
	for (int y = 0; y < height; y++) {
		flood_fill(a_, width, height, 0, y, 0, -2, true);
		flood_fill(a_, width, height, width - 1, y, 0, -2, true);
	}
	int shapes[width * height / 2][2];
	int shape_count = 0;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			if (a(x, y) >= 0) {
				shapes[shape_count][0] = x;
				shapes[shape_count][1] = y;
				shape_count++;
				flood_fill(a_, width, height, x, y, a(x, y), a(x, y) - 2, false);
			}
		}
	}
	assert(shape_count <= width * height / 2);
	for (int i = 0; i < shape_count; i++) {
		int x = shapes[i][0];
		int y = shapes[i][1];
		int z = a(x, y);
		int dir = 1; // ENWS
		#define gbr_scale(x, y) (gbr_z + CMPLX(creal(gbr_pitch) * (x), cimag(gbr_pitch) * (y)))
		gbr_set_paint_or_erase(this, painting ^ (z == -2));
		gbr_begin_region(this, pen_function);
		gbr_move_to(this, gbr_scale(x, y));
		do {
			int dx = dir == 3 ? 0 : 1 - dir;
			int dy = dir == 0 ? 0 : 2 - dir;
			int ex = dx ? dx : -dy;
			int ey = dx ? dx : dy;
			if (x + dx >= 0 && x + dx < width && y + dy >= 0 && y + dy < height && a(x + dx, y + dy) == z) {
				if (x + ex >= 0 && x + ex < width && y + ey >= 0 && y + ey < height && a(x + ex, y + ey) == z) {
					gbr_line_to(this, gbr_scale(x + (ex >= 0), y + (ey >= 0)));
					dir = (dir + 1) % 4;
				}
				x += dx;
				y += dy;
			} else {
				gbr_line_to(this, gbr_scale(x + (ex >= 0), y + (ey >= 0)));
				dir = (dir + 3) % 4;
			}
		} while (x != shapes[i][0] || y != shapes[i][1] || dir != 1);
		gbr_end_region(this);
		#undef gbr_scale
	}
	//for(int y=height-1;y>=0;putchar(10),y--)for(int x=0;x<width;printf("%5d",a(x,y)%10000),x++);
	free(a_);
	gbr_set_paint_or_erase(this, painting);
}
#undef a

void gbr_draw_acid_test(struct gbr_file *this, const double complex z0, const double complex pitch, const double d, const double auxiliary_line_width) {
	const double eps = 1e-3;
	const double r = d / 2;
	const char *const long_name = "Acid_"
		"NamesConsistOfUpperOrLowercaseASCIILetters"
		"_Underscores.Dots$ADollarSign123456789AndDigits."
		"NamesAreFrom1To127CharactersLong";
	assert(strlen(long_name) == 127);
	double complex z[25];
	const bool array = creal(pitch) && cimag(pitch);

	for (int i = 0; i < 25; i++) {
		z[i] = z0 + (array ? creal(pitch) * (i % 5) + cimag(pitch) * (i / 5) * I : pitch * i);
	}

	int pens[25];
	pens[0] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "C,%f", d);
	gbr_end_pen(this);

	pens[1] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "C,%fX%f", d, r);
	gbr_end_pen(this);

	pens[2] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "C,%f", r + eps * 2);
	gbr_end_pen(this);

	pens[3] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "R,%fX%fX%f", d * sqrt(.5), d * sqrt(.5), r);
	gbr_end_pen(this);

	pens[4] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "O,%fX%f", d, d);
	gbr_end_pen(this);

	pens[5] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "O,%fX%f", d - eps * 2, r - eps * 2);
	gbr_end_pen(this);

	pens[6] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "O,%fX%f", r - eps * 2, d - eps * 2);
	gbr_end_pen(this);

	pens[7] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "P,%fX%dX%fX%f", d - eps * 2, 5, 0.0, r);
	gbr_end_pen(this);

	pens[8] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "P,%fX%dX%.0f", d - eps * 2, 3, 30.0);
	gbr_end_pen(this);

	pens[9] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "P,%fX%dX%f", d - eps * 2, 12, -1 / degree);
	gbr_end_pen(this);

	pens[10] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "C,%f", 0.0);
	gbr_end_pen(this);

	pens[11] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "C,%.0f", 0.0);
	gbr_end_pen(this);

	fprintf(this->fp,
		"%%AMAcid_.$E*"
		"1,1,%f,0,0*"
		"21,0,%f,%f,%f,%f,45*%%"
		"%%AMAcid_.$e*"
		"4,1,3,0,0,%f,%f,%f,%f,0,0,0*%%"
		"%%AM%s*"
		"0 %-126s*"
		"20,1,%f,%f,%f,%f,%f,0*"
		"21,0,%f,%f,0,0,45*%%\n"
		"%%AMAcid_Thermal*\n"
		"0 $1 = outer radius*\n"
		"0 $2 = inner radius*\n"
		"0 $3 = ½ gap*\n"
		"0 $4 = radians counterclockwise*\n"
		"7,%f,0,$1+$1,$2x2,-$3+(+3x$3),$4/%.18f*%%\n",
		d,
		r * .75, r / 2, r * .375, -r / 4,
		r, r, -r, r,
		long_name,
		"\xc2\xb1\xc3\x97\xc3\xb7", // ±×÷
		r / 4, r * sqrt(.0703125), r * sqrt(.0703125), -r * sqrt(.0703125), -r * sqrt(.0703125),
		r / 4, r * .75,
		r, degree
	);
	// Names are case-sensitive.
	pens[12] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "Acid_.$E");
	gbr_end_pen(this);

	pens[13] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "Acid_Thermal,%fX%fX%fX%f", r, r * .75, r / 4, -acos(-1));
	gbr_end_pen(this);

	pens[14] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "Acid_Thermal,%fX%fX%fX%f", r - eps, eps * 4, r / 6, atan(1));
	gbr_end_pen(this);

	int auxiliary_pen = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "C,%f", auxiliary_line_width);
	gbr_end_pen(this);

	gbr_select_pen(this, auxiliary_pen);
	for (int i = 0; i < 25; i++) {
		double complex za0 = r + r * I;
		double complex za1 = za0 - r / 2, za2 = za0 - r / 2 * I;
		for (int j = 0; j < 4; j++) {
			gbr_move_to(this, z[i] + za1);
			gbr_line_to(this, z[i] + za0);
			gbr_line_to(this, z[i] + za2);
			za0 *= I;
			za1 *= I;
			za2 *= I;
		}
	}

	gbr_select_pen(this, pens[0]);
	gbr_begin_repeat(this,
		array ? 5 : creal(pitch) ? 15 : 1,
		array ? 3 : creal(pitch) ? 1 : 15,
		pitch
	);
	gbr_draw_dot(this, z[10]);
	gbr_end_repeat(this);

	// Zero-size objects do not affect the image.
	gbr_select_pen(this, pens[10]);
	gbr_move_to(this, z[0] + r);
	gbr_line_to(this, z[24] - r);
	gbr_select_pen(this, pens[11]);
	gbr_move_to(this, z[1] + r + r * I);
	gbr_line_to(this, z[23] + r - r * I);
	gbr_line_to(this, z[12]);
	gbr_circle_here(this, z[12] + r / 2);
	gbr_circle_here(this, z[8]);

	// 0..1
	// Zero-length draws and arcs are allowed. As an image, the resulting image is identical to a flash of the same aperture.
	gbr_select_pen(this, pens[0]);
	gbr_move_to(this, z[0]);
	gbr_line_to(this, z[0]);
	gbr_move_to(this, z[1]);
	gbr_circle_here(this, z[1]);

	// 2
	// Names, parameters and objects must be defined before they are used.
	pens[24] = gbr_begin_pen(this, "NonConductor");
	fprintf(this->fp, "C,%f", d);
	gbr_end_pen(this);
	gbr_select_pen(this, pens[24]);
	gbr_draw_dot(this, z[2]);

	// 3
	// Objects under a hole remain visible through the hole.
	gbr_select_pen(this, pens[2]);
	gbr_draw_dot(this, z[3]);
	gbr_select_pen(this, pens[1]);
	gbr_draw_dot(this, z[3]);

	// 4
	// When start point and end point coincide the result is a full 360° arc.
	gbr_move_to(this, z[4] + cpolar(r / 2 - eps, 7));
	gbr_select_pen(this, pens[2]);
	gbr_circle_here(this, z[4]);

	// 5
	// Primitives with exposure ‘off’ erase the solid part created earlier in the same macro.
	gbr_begin_region(this, "NonConductor");
	gbr_move_to(this, z[5] - eps);
	gbr_line_to(this, z[5] + r * sqrt(.125) - (r * sqrt(.125) + eps) * I);
	gbr_line_to(this, z[5] + r * sqrt(.78125) + eps + r * sqrt(.03125) * I);
	gbr_line_to(this, z[5] + r * sqrt(.28125) + (r * sqrt(.28125) + eps) * I);
	gbr_close_path(this);
	gbr_end_region(this);
	gbr_select_pen(this, pens[12]);
	gbr_draw_dot(this, z[5]);

	// 6
	// The thermal primitive is a ring (annulus) interrupted by four gaps. Exposure is always on.
	gbr_select_pen(this, pens[13]);
	gbr_draw_dot(this, z[6] + r);

	// 7
	// For the avoidance of doubt, not allowed are, amongst others: …, full arcs except when the contour consist solely of that full arc or the full arc is at the end of a cut-in.
	gbr_begin_region(this, "NonConductor");
	gbr_move_to(this, z[7] + cpolar(r, 9));
	gbr_circle_here(this, z[7]);
	gbr_end_region(this);

	// 8
	// TODO
	// 9

	// 10
	// Note that if the (gap thickness) × √2 ≥ (inner diameter) the inner circle disappears. This is not invalid.
	gbr_set_paint_or_erase(this, false);
	gbr_select_pen(this, pens[14]);
	gbr_draw_dot(this, z[10] - r * sqrt(.5) - r * sqrt(.5) * I);
	gbr_set_paint_or_erase(this, true);
	{
		double complex v0 = r * sqrt(1 / 18.0);
		double complex v1 = v0 + r * (sqrt(70) - sqrt(2)) / 12 * (1 + I);
		double complex v2 = conj(v1);
		v0 -= eps;
		for (int j = 0; j < 4; j++) {
			gbr_begin_region(this, "NonConductor");
			gbr_move_to(this, z[10] + v0);
			gbr_line_to(this, z[10] + v1);
			gbr_arc_to(this, z[10] + v2, z[10], true);
			gbr_close_path(this);
			gbr_end_region(this);
			v0 *= I;
			v1 *= I;
			v2 *= I;
		}
	}

	// 11
	// TODO
}

struct xnc_file {
	FILE *fp;
	int tool_count;
	bool inside_header;
	bool drilling;
	int tool;
	unsigned set_attributes;
};

void xnc_begin_file(struct xnc_file *this, const char *filename, const char *file_function) {
	assert(file_function && *file_function);
	this->fp = fopen(filename, "wb");
	if (!this->fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	fputs("M48\nMETRIC\n", this->fp);
	fprintf(this->fp,
		"; #@! TF.FileFunction,%s\n"
		"; #@! TF.SameCoordinates\n"
		"; #@! TF.GenerationSoftware,Aniced,PCBAD,0.0\n"
		"; #@! TF.CreationDate,%s\n",
		file_function,
		iso8601(time(NULL))
	);
	this->tool_count = 1;
	this->tool = INT_MIN;
	this->inside_header = true;
	this->drilling = false;
}

static void xnc_end_header(struct xnc_file *this) {
	if (this->inside_header) {
		fputs("%\n", this->fp);
		this->inside_header = false;
	}
}

void xnc_end_file(struct xnc_file *this) {
	xnc_end_header(this);
	fputs("M30\n", this->fp);
	fclose(this->fp);
	this->fp = NULL;
}

int xnc_define_tool(struct xnc_file *this, double diameter, double plus_tolerance, double minus_tolerance, const char *tool_function) {
	assert(this->inside_header);
	assert(this->tool_count >= 1 && this->tool_count <= 99);
	// The default precision in printf() is 6 digits.
	fprintf(this->fp, "; #@! TA.AperFunction,%s\n", tool_function);
	assert(!!isfinite(plus_tolerance) == !!isfinite(minus_tolerance));
	if (isfinite(plus_tolerance)) {
		fprintf(this->fp, "; #@! TA.DrillTolerance,%f,%f\n", plus_tolerance, minus_tolerance);
	}
	fprintf(this->fp, "T%02dC%.3f\n", this->tool_count, diameter);
	return this->tool_count++;
}

void xnc_select_tool(struct xnc_file *this, int tool) {
	xnc_end_header(this);
	fprintf(this->fp, "T%02d\n", tool);
	this->tool = tool;
}

static void xnc_x(struct xnc_file *this, double x) {
	char s[30];
	int i = sprintf(s, "%.3f", x);
	// A '.' must be encountered so a bounary check is unneceassary.
	while (s[i - 1] == '0') i--;
	s[i + (s[i - 1] == '.')] = 0;
	fputs(s + (strcmp(s, "-0.0") == 0), this->fp);
}

static void xnc_xy(struct xnc_file *this, double complex z) {
	// https://gitlab.com/kicad/code/kicad/-/blob/master/pcbnew/exporters/gendrill_Excellon_writer.cpp
	// EXCELLON_WRITER::writeCoordinates()
	// In Excellon files, resolution is 1μm or .1mil.
	// Although in decimal format, Excellon specifications do not specify clearly the resolution. However it seems to be 1/1000mm or 0.1 mil like in non decimal formats, so we truncate coordinates to 3 or 4 digits in mantissa
	// Decimal format just prohibit useless leading 0: 0.45 or .45 is right, but 00.54 is incorrect.
	fputc('X', this->fp);
	xnc_x(this, creal(z));
	fputc('Y', this->fp);
	xnc_x(this, cimag(z));
}

void xnc_drill(struct xnc_file *this, double complex z) {
	assert(!this->inside_header); // header ended by selecting a tool
	if (!this->drilling) {
		fputs("G05\n", this->fp);
		this->drilling = true;
	}
	xnc_xy(this, z);
	fputc('\n', this->fp);
}

void xnc_rout(struct xnc_file *this, double complex z0, double complex z1) {
	if (z0 == z1) {
		xnc_drill(this, z0);
		return;
	}
	assert(!this->inside_header);
	this->drilling = false;
	fputs("G00", this->fp);
	xnc_xy(this, z0);
	fputs("\nM15\nG01", this->fp);
	xnc_xy(this, z1);
	fputs("\nM16\n", this->fp);
}

void xnc_drill_evenly_spaced(struct xnc_file *this, double complex z0, double complex z1, int n) {
	if (z0 == z1) {
		xnc_drill(this, z0);
		return;
	}
	assert(n >= 2);
	n--;
	for (int i = 0; i <= n; i++) {
		xnc_drill(this, (1 - (double) i / n) * z0 + (double) i / n * z1);
	}
}

int main() {
	struct gbr_file gtl, gbl, gts, gbs, gto, gbo, gm1;
	struct xnc_file pth, npth;
	gbr_begin_file(&gm1, "z.gm1", "Profile,NP");
	gbr_begin_file(&gtl, "z.gtl", "Copper,L1,Top");
	gbr_begin_file(&gbl, "z.gbl", "Copper,L2,Bot");
	gbr_begin_file(&gts, "z.gts", "Soldermask,Top");
	gbr_begin_file(&gbs, "z.gbs", "Soldermask,Bot");
	gbr_begin_file(&gto, "z.gto", "Legend,Top");
	gbr_begin_file(&gbo, "z.gbo", "Legend,Bot");
	xnc_begin_file(&pth, "z-PTH.drl", "Plated,1,2,PTH");
	xnc_begin_file(&npth, "z-NPTH.drl", "NonPlated,1,2,NPTH");

	int pencircle = gbr_begin_pen(&gbo, "Material");
	fprintf(gbo.fp, "C,%f", 6 * mil);
	gbr_end_pen(&gbo);
	gbr_select_pen(&gbo, pencircle);
	gbr_move_to(&gbo, 2 + 7 * I);
	gbr_cubic_bezier_to(&gbo, 2 + 9 * I, 8 + 5 * I, 7 + 6 * I);
	gbr_cubic_bezier_to(&gbo, 7 + 7 * I, 8 + 4 * I, 3 + 6 * I);

	gbr_end_file(&gm1);
	gbr_end_file(&gtl);
	gbr_end_file(&gbl);
	gbr_end_file(&gts);
	gbr_end_file(&gbs);
	gbr_end_file(&gto);
	gbr_end_file(&gbo);
	xnc_end_file(&pth);
	xnc_end_file(&npth);
}


// ### Gerbv: Most likely found a RS-274D file ###
// Gerbv's logic (see gerber_is_rs274x_p() and gerber_is_rs274d_p() in gerbv-2.7.0/src/gerber.c) as of 2.7.0 is that a file may be a Gerber file (standard or extended) if it
// - contains any of the following: “D00”, “D0”, “D02”, “D2”, “M00”, “M0”, “M02”, “M2”,
// - contains any of the following: “X<digit>”, “Y<digit>”, and
// - contains at least one “*”.
// A file is likely RS-274D if it
// - may be a Gerber file,
// - has no special characters other than the printable ones and \n\r\t, and
// - does not contain the string “%ADD”.
// A valid RS-274-X file made up solely of regions need not define any aperture, thus considered RS-274D.
