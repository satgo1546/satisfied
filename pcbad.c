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
	// The default precision for printf is 6 digits.
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
	fputs(s, this->fp);
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
