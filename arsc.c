// https://justanapplication.wordpress.com/android-internals/

// https://android.googlesource.com/platform/frameworks/base/+/master/libs/androidfw/include/androidfw/ResourceTypes.h
// https://android.googlesource.com/platform/frameworks/base/+/master/libs/androidfw/ResourceTypes.cpp
// The above files are placed accordingly as of Android S.
// They have long existed before Android 1.6, but the directory structure has changed much and may still change in the future.

// aapt dump xmltree a.apk AndroidManifest.xml
// aapt dump xmlstrings a.apk AndroidManifest.xml

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bits.c"

// UTF8_FLAG gets introduced in Android 2.2.
// utf8_to_utf16_length is used to get the “length” of a string in ResourceTypes.cpp.
// The “length” field is therefore in UTF-16 codes (i.e., codepoints outside BMP count as two) instead of codepoints.
size_t utf8_utf16_length(const char *s) {
	size_t r = 0;
	while (*s) {
		r += ((*s & 0xc0) != 0x80) + ((unsigned char) *s >= 0xf0);
		s++;
	}
	return r;
}

// This function does not check for encoding errors and buffer overflow.
void utf8_to_utf16(uint16_t *dest, const char *src) {
	uint32_t x;
	while ((x = (unsigned char) *src++)) {
		uint32_t n = 0;
		while (x & (0x80 >> n)) n++;
		if (n) {
			// Read the following continuation bytes.
			x &= 0x7f >> n;
			while (--n) {
				x <<= 6;
				x |= (unsigned char) *src++ & 63;
			}
		}
		if (x & 0xff0000) {
			// Write a UTF-16 surrogate pair.
			x -= 0x10000;
			*dest++ = 0xd800 | (x >> 10);
			*dest++ = 0xdc00 | (x & 0x3ff);
		} else {
			*dest++ = x;
		}
	}
}

// arsc_string_pool implements the ResStringPool chunk in a simple-minded manner.
// With the storage allocated ahead of time, strings too long are rejected and cause panic.
// All the strings are null-terminated, laid out sequentially in memory, and encoded in UTF-8.
// The last string in the pool comes with an additional zero byte which indicates the end.
//   begin = #0      #1                            #2                         end
//   ['H']['i'][ 0 ]['H']['e']['l']['l']['o'][ 0 ]['i'][ 0 ][ 0 ][   ] … [   ]
//   styles
//   [ −1 ][  2 ][  0 ][  0 ][  2 ][  2 ][  3 ][ −1 ][ −1 ][    ] …
// The example above illustrates a pool with three strings, namely "Hi", HTML "<i>H</i>e<i>ll</i>o" and "i".
// Since styling in an identifier pool is meaningless, the styleCount field in ResStringPool_header is either 0 or equal to stringCount in this implementation. The storage for styles is allocated upon the first call to arsc_append_styled.
struct arsc_string_pool {
	char *begin;
	const char *end;
	size_t count;
	uint32_t *styles;
	size_t style_word_count;
};

void arsc_begin_string_pool(struct arsc_string_pool *this) {
	this->begin = malloc(4096);
	if (!this->begin) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	*this->begin = 0;
	this->end = this->begin + 4096;
	// To ease pooling, the first string is always the empty string.
	this->count = 1;
	this->styles = NULL;
	this->style_word_count = 0;
}

size_t arsc_intern(struct arsc_string_pool *this, const char *s) {
	// getAttributeNamespaceID returns -1 if there is no namespace.
	if (!s) return SIZE_MAX;
	if (!*s) return 0;
	// Do a linear search for an existing entry.
	char *p;
	size_t n = 1;
	for (p = this->begin; *p; p += strlen(p) + 1) {
		if (strcmp(s, p) == 0) return n;
		n++;
	}
	if (p + strlen(s) + 1 >= this->end) fprintf(stderr, "%s\n", __func__), exit(EXIT_FAILURE);
	strcpy(p, s);
	p[strlen(p) + 1] = 0;
	this->count = n + 1;
	if (this->styles) {
		this->styles[this->style_word_count++] = UINT32_MAX;
	}
	return n;
}

void arsc_end_string_pool(struct arsc_string_pool *this, FILE *fp) {
	long start = ftell(fp);
	write16(fp, 0x0001); // RES_STRING_POOL_TYPE
	write16(fp, 28); // headerSize
	write32(fp, 0); // size (to be calculated)
	write32(fp, this->count); // stringCount
	if (this->styles) {
		write32(fp, this->count); // styleCount
		write32(fp, 1 << 8); // UTF8_FLAG
		write32(fp, this->count * 8 + 28); // stringsStart
		write32(fp, 0); // stylesStart (to be calculated)
		for (size_t i = 0; i < this->count * 2; i++) write32(fp, 0);
	} else {
		write32(fp, 0); // styleCount
		write32(fp, 1 << 8); // UTF8_FLAG
		write32(fp, this->count * 4 + 28); // stringsStart
		write32(fp, 0); // stylesStart
		for (size_t i = 0; i < this->count; i++) write32(fp, 0);
	}

	long pos[this->count];
	pos[0] = ftell(fp);
	write8(fp, 0);
	write8(fp, 0);
	write8(fp, 0);
	const char *p = this->begin;
	for (size_t i = 1; i < this->count; i++) {
		pos[i] = ftell(fp);
		size_t len = strlen(p);
		writelen7or15(fp, utf8_utf16_length(p));
		writelen7or15(fp, len);
		fwrite(p, 1, len + 1, fp);
		p += len + 1;
	}
	write_align_to(fp, 4);

	long size = ftell(fp) - start;
	if (this->styles) {
		for (size_t i = 0; i < this->style_word_count; i++) {
			write32(fp, this->styles[i]);
		}
		fseek(fp, start + 24, SEEK_SET);
		write32(fp, size); // stylesStart
		for (size_t i = 0; i < this->count; i++) {
			write32(fp, pos[i] - pos[0]);
		}
		uint32_t *p = this->styles;
		for (size_t i = 0; i < this->count; i++) {
			write32(fp, (p - this->styles) * 4);
			while (*p != UINT32_MAX) p++;
			p++;
		}
		// It is said that the style data section should be terminated by two more 0xffffffff's. No more words are written here; no problems are encountered so far, either.
		size += this->style_word_count * 4;
	} else {
		fseek(fp, start + 28, SEEK_SET);
		for (size_t i = 0; i < this->count; i++) {
			write32(fp, pos[i] - pos[0]);
		}
	}
	fseek(fp, start + 4, SEEK_SET);
	write32(fp, size);
	fseek(fp, 0, SEEK_END);

	free(this->begin);
	free(this->styles); // free(NULL) does nothing.
}

//   \1 = Start Of tag Header
//   \2 = Start Of tag Text
//   \3 = End Of tag Text and also the tag itself
// because writing an HTML parser for the sole purpose of generating rich text is a coder's shame.
// For instance, the markup
//   <font face="monospace" color="#663399">It is <u>true</u>.</font>
// translates to
//   "\1font;face=monospace;color=#663399\2It is \1u\2true\3.\3"
// The first four ASCII control characters are now seeing such a perfectly matched renaissance, if not a bit ugly.
// arsc_intern matches against the plain text, which should be how resources work. However, Android refuses to install the APK blatantly if (for example) the app name is associated with style data. If a true plain entry is needed, call arsc_intern first.
size_t arsc_append_styled(struct arsc_string_pool *this, const char *control) {
	if (!this->styles) {
		this->styles = malloc(1024 * 4);
		if (!this->styles) {
			perror("malloc");
			return arsc_intern(this, control);
		}
		memset(this->styles, 0xff, this->count * 4);
		this->style_word_count = this->count;
	}

	// As s is scanned, plain text is filtered in-place, as the plain text will always be shorter than the formatted control string.
	char s[strlen(control) + 1];
	strcpy(s, control);
	// \1i\2\3 is the shortest possible tag, <i></i>.
	size_t tags[(sizeof(s) - 1) / 4 + 1][3];
	size_t tag_count = 0;
	char *src = s, *dest = s;
	while (*src) switch (*src++) {
	case 1:
		{
			char *const p = strchr(src, 2);
			*p = 0;
			tags[tag_count][0] = arsc_intern(this, src); // name
			tags[tag_count][1] = dest - s; // firstChar
			tags[tag_count][2] = SIZE_MAX; // lastChar
			tag_count++;
			src = p + 1;
		}
		break;
	case 2:
		abort();
		break;
	case 3:
		for (size_t i = tag_count - 1; i != SIZE_MAX; i--) {
			if (tags[i][2] == SIZE_MAX) {
				tags[i][2] = dest - s - 1;
				break;
			}
		}
		break;
	default:
		*dest++ = src[-1];
	}
	*dest = 0;

	char *p;
	size_t n = 1;
	for (p = this->begin; *p; p += strlen(p) + 1) n++;
	strcpy(p, s);
	p[strlen(p) + 1] = 0;
	this->count = n + 1;
	for (size_t i = 0; i < tag_count; i++) {
		// ResStringPool_span
		for (size_t j = 0; j < 3; j++) {
			this->styles[this->style_word_count++] = tags[i][j];
		}
	}
	this->styles[this->style_word_count++] = UINT32_MAX;
	return n;
}



struct arsc_file {
	FILE *fp;
	uint16_t package_name[128];
	struct arsc_string_pool string_pool;
	// A string pool has to be used because names are not required to stay unique across different resource types.
	struct arsc_string_pool keys;
	uint8_t spec_type_id;
	size_t spec_entry_count;
	// The number of name-value pairs in the current complex resource entry. Used internally.
	size_t current_map_size;
	long config_start;
};

#define arsc_type_count 20
static_assert(arsc_type_count < 57, "arsc_type_count");
static const unsigned char arsc_types[284] = {
	0x01, 0x00, // RES_STRING_POOL_TYPE
	28, 0, // headerSize
	sizeof(arsc_types) & 0xff, sizeof(arsc_types) >> 8, // size
	[8] = arsc_type_count, // stringCount
	[16] = 0, 1, // UTF8_FLAG
	#define stringsStart (28 + arsc_type_count * 4)
	[20] = stringsStart, // stringsStart

	// The following types, except <plurals>, <menu> and <font>, are used in public.xml.
	// <array>, <integer-array> and <string-array> are collected under the same type, array.
	[28 + 0 * 4] = 0, [stringsStart + 0] = 4, 4, 'a', 't', 't', 'r', 0, // 0x01
	[28 + 1 * 4] = 7, [stringsStart + 7] = 2, 2, 'i', 'd', 0, // 0x02
	[28 + 2 * 4] = 12, [stringsStart + 12] = 5, 5, 's', 't', 'y', 'l', 'e', 0, // 0x03
	[28 + 3 * 4] = 20, [stringsStart + 20] = 6, 6, 's', 't', 'r', 'i', 'n', 'g', 0, // 0x04
	[28 + 4 * 4] = 29, [stringsStart + 29] = 5, 5, 'd', 'i', 'm', 'e', 'n', 0, // 0x05
	[28 + 5 * 4] = 37, [stringsStart + 37] = 5, 5, 'c', 'o', 'l', 'o', 'r', 0, // 0x06
	[28 + 6 * 4] = 45, [stringsStart + 45] = 5, 5, 'a', 'r', 'r', 'a', 'y', 0, // 0x07
	[28 + 7 * 4] = 53, [stringsStart + 53] = 8, 8, 'd', 'r', 'a', 'w', 'a', 'b', 'l', 'e', 0, // 0x08
	[28 + 8 * 4] = 64, [stringsStart + 64] = 6, 6, 'l', 'a', 'y', 'o', 'u', 't', 0, // 0x09
	[28 + 9 * 4] = 73, [stringsStart + 73] = 4, 4, 'a', 'n', 'i', 'm', 0, // 0x0a
	[28 + 10 * 4] = 80, [stringsStart + 80] = 8, 8, 'a', 'n', 'i', 'm', 'a', 't', 'o', 'r', 0, // 0x0b
	[28 + 11 * 4] = 91, [stringsStart + 91] = 12, 12, 'i', 'n', 't', 'e', 'r', 'p', 'o', 'l', 'a', 't', 'o', 'r', 0, // 0x0c
	[28 + 12 * 4] = 106, [stringsStart + 106] = 6, 6, 'm', 'i', 'p', 'm', 'a', 'p', 0, // 0x0d
	[28 + 13 * 4] = 115, [stringsStart + 115] = 7, 7, 'i', 'n', 't', 'e', 'g', 'e', 'r', 0, // 0x0e
	[28 + 14 * 4] = 125, [stringsStart + 125] = 10, 10, 't', 'r', 'a', 'n', 's', 'i', 't', 'i', 'o', 'n', 0, // 0x0f
	[28 + 15 * 4] = 138, [stringsStart + 138] = 3, 3, 'r', 'a', 'w', 0, // 0x10
	[28 + 16 * 4] = 144, [stringsStart + 144] = 4, 4, 'b', 'o', 'o', 'l', 0, // 0x11
	[28 + 17 * 4] = 151, [stringsStart + 151] = 7, 7, 'p', 'l', 'u', 'r', 'a', 'l', 's', 0, // 0x12
	[28 + 18 * 4] = 161, [stringsStart + 161] = 4, 4, 'm', 'e', 'n', 'u', 0, // 0x13
	[28 + 19 * 4] = 168, [stringsStart + 168] = 4, 4, 'f', 'o', 'n', 't', 0, // 0x14
	#undef stringsStart
};
static_assert(sizeof(arsc_types) < 0x10000, "arsc_types");
static_assert(sizeof(arsc_types) % 4 == 0, "arsc_types");

void arsc_begin_file(struct arsc_file *this, const char *package_name) {
	this->fp = tmpfile();
	if (!this->fp) {
		perror("tmpfile");
		exit(EXIT_FAILURE);
	}
	memset(this->package_name, 0, sizeof(this->package_name));
	utf8_to_utf16(this->package_name, package_name);
	arsc_begin_string_pool(&this->string_pool);
	arsc_begin_string_pool(&this->keys);
	this->spec_type_id = 0;
	this->current_map_size = SIZE_MAX;
	this->config_start = 0;
}

void arsc_end_file_fp(struct arsc_file *this, FILE *fp) {
	const long start = ftell(fp);
	write16(fp, 0x0002); // RES_TABLE_TYPE
	write16(fp, 12); // headerSize
	write32(fp, 0); // size (to be calculated)
	write32(fp, 1); // packageCount

	arsc_end_string_pool(&this->string_pool, fp);

	const long package_start = ftell(fp);
	write16(fp, 0x0200); // RES_TABLE_PACKAGE_TYPE
	write16(fp, 284); // headerSize
	write32(fp, 0); // size (to be calculated)
	write32(fp, 0x7f); // id
	for (size_t i = 0; i < 128; i++) write16(fp, this->package_name[i]);
	write32(fp, 284); // typeStrings
	write32(fp, arsc_type_count); // lastPublicType
	write32(fp, 284 + sizeof(arsc_types)); // keyStrings
	write32(fp, this->keys.count); // lastPublicKey

	fwrite(arsc_types, 1, sizeof(arsc_types), fp);
	arsc_end_string_pool(&this->keys, fp);

	rewind(this->fp);
	copy_file_fp(fp, this->fp);

	long size = ftell(fp) - start;
	fseek(fp, start + 4, SEEK_SET);
	write32(fp, size);
	size += start - package_start;
	fseek(fp, package_start + 4, SEEK_SET);
	write32(fp, size);

	fclose(this->fp);
}

void arsc_end_file(struct arsc_file *this, const char *filename) {
	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	arsc_end_file_fp(this, fp);
	fclose(fp);
}

void arsc_begin_type(struct arsc_file *this, uint8_t type, size_t entry_count) {
	// Type IDs start	at 1; 0 is invalid.
	assert(type && type <= arsc_type_count);
	assert(!this->spec_type_id);
	write16(this->fp, 0x0202); // RES_TABLE_TYPE_SPEC_TYPE
	write16(this->fp, 16); // headerSize
	write32(this->fp, entry_count * 4 + 16); // size
	write32(this->fp, type); // id, res0, res1
	write32(this->fp, entry_count); // entryCount
	for (size_t i = 0; i < entry_count; i++) write32(this->fp, 0);
	this->spec_type_id = type;
	this->spec_entry_count = entry_count;
}

void arsc_end_type(struct arsc_file *this) {
	assert(this->spec_type_id);
	this->spec_type_id = 0;
}

void arsc_begin_configuration(struct arsc_file *this,
	uint16_t mcc, uint16_t mnc,
	const char *bcp47,
	uint16_t screen_layout,
	uint16_t smallest_width, uint16_t width, uint16_t height, uint8_t color,
	uint8_t orientation, uint8_t ui_mode, uint16_t density, uint8_t touchscreen,
	uint8_t keyboard, uint8_t navigation, uint8_t input_flags,
	uint16_t sdk
) {
	assert(this->spec_type_id);
	assert(!this->config_start);

	write16(this->fp, 0x0201); // RES_TABLE_TYPE_TYPE
	write16(this->fp, 20 + 56); // headerSize
	write32(this->fp, 0); // size (to be calculated)

	write32(this->fp, this->spec_type_id); // id, res0, res1
	write32(this->fp, this->spec_entry_count); // entryCount
	write32(this->fp, this->spec_entry_count * 4 + 20 + 56); // entriesStart

	// sizeof(ResTable_config) has once changed in 2014, but has remained constant since then.
	// https://developer.android.com/guide/topics/resources/providing-resources
	// https://android.googlesource.com/platform/frameworks/base.git/+/master/tools/aapt/AaptConfig.cpp
	this->config_start = ftell(this->fp);
	write32(this->fp, 56); // size
	write16(this->fp, mcc); // mcc (mobile country code (from SIM))
	write16(this->fp, mnc); // mnc (mobile network code (from SIM))

	// The following logic is mostly copied from AaptLocaleValue::initFromDirName, to which no significant changes have been made since 2014.
	char locale_script[4] = {0}, locale_variant[8] = {0}, locale_numbering_system[8] = {0};
	if (bcp47 && *bcp47) {
		// Partition bcp47 at '-' like strtok.
		char language[strlen(bcp47) + 1];
		strcpy(language, bcp47);
		char *subtags[6] = {language};
		size_t subtag_count = 1;
		for (char *p = language; (p = strchr(p, '-')); *p++ = 0) {
			if (subtag_count >= 6) {
				fprintf(stderr, "%s: BCP 47 tag with too many parts (%s)\n", __func__, bcp47);
				exit(EXIT_FAILURE);
			}
			subtags[subtag_count++] = p + 1;
		}
		// https://github.com/unicode-org/cldr/blob/master/common/bcp47/number.xml
		if (strcmp(subtags[subtag_count - 2], "nu") == 0) {
			strcpy(locale_numbering_system, subtags[subtag_count - 1]);
			subtag_count -= 2;
		}
		// The language is always the first subtag.
		if (subtags[0][2]) {
			write16be(this->fp,
				0x8000 | ((subtags[0][0] & 0x1f) - 1)
				| (((subtags[0][1] & 0x1f) - 1) << 5)
				| (((subtags[0][2] & 0x1f) - 1) << 10)
			); // language
		} else {
			assert(subtags[0][1]);
			write8(this->fp, tolower(subtags[0][0])); // language[0]
			write8(this->fp, tolower(subtags[0][1])); // language[1]
		}

		// Rearrange the other information so that
		//   subtags[1] = script
		//   subtags[2] = region
		//   subtags[3] = variant
		if (subtag_count == 2) {
			// The second tag can either be a region, a variant or a script.
			switch (strlen(subtags[1])) {
			case 2:
			case 3:
				subtags[2] = subtags[1];
				subtags[1] = NULL;
				break;
			case 4:
				if (isalpha(subtags[1][0]) && isalpha(subtags[1][1])
					&& isalpha(subtags[1][2]) && isalpha(subtags[1][3])) break;
				// This is not alphabetical, so we fall through to variant.
			case 5:
			case 6:
			case 7:
			case 8:
				subtags[3] = subtags[1];
				subtags[1] = NULL;
				break;
			default:
				fprintf(stderr, "%s: invalid BCP 47 tag %s\n", __func__, bcp47);
				exit(EXIT_FAILURE);
			}
		} else if (subtag_count == 3) {
			// The third tag can either be a region code (if the second tag was a script), else a variant code.
			if (strlen(subtags[2]) >= 4) {
				subtags[3] = subtags[2];
				subtags[2] = NULL;
				// The second subtag can either be a script or a region code.
				// If its size is 4, it's a script code, else it's a region code.
				if (strlen(subtags[1]) != 4) {
					subtags[2] = subtags[1];
					subtags[1] = NULL;
				}
			}
		}
		if (!subtags[2]) {
			write16(this->fp, 0); // country
		} else if (subtags[2][2]) {
			assert(isdigit(subtags[2][0]) && isdigit(subtags[2][1]) && isdigit(subtags[2][2]));
			write16be(this->fp,
				0x8000 | (subtags[2][0] & 0x0f)
				| ((subtags[2][1] & 0x0f) << 5) | ((subtags[2][2] & 0x0f) << 10)
			); // country
		} else {
			assert(subtags[2][1]);
			write8(this->fp, toupper(subtags[2][0])); // country[0]
			write8(this->fp, toupper(subtags[2][1])); // country[1]
		}
		if (subtags[1]) {
			locale_script[0] = toupper(subtags[1][0]);
			locale_script[1] = tolower(subtags[1][1]);
			locale_script[2] = tolower(subtags[1][2]);
			locale_script[3] = tolower(subtags[1][3]);
		}
		if (subtags[3]) strncpy(locale_variant, subtags[3], 8);
	} else {
		write32(this->fp, 0); // language, country
	}

	write8(this->fp, orientation); // orientation
	write8(this->fp, touchscreen); // touchscreen
	write16(this->fp, density); // density
	if (density == 0xfffe) if (sdk < 21) sdk = 21;
	write8(this->fp, keyboard); // keyboard
	write8(this->fp, navigation); // navigation
	write16(this->fp, input_flags); // inputFlags, inputPad0
	write32(this->fp, 0); // screenWidth, screenHeight
	write32(this->fp, sdk); // sdkVersion, minorVersion
	write8(this->fp, screen_layout); // screenLayout
	write8(this->fp, ui_mode); // uiMode
	write16(this->fp, smallest_width); // smallestScreenWidthDp
	write16(this->fp, width); // screenWidthDp
	write16(this->fp, height); // screenHeightDp
	fwrite(locale_script, 1, 4, this->fp); // localeScript
	fwrite(locale_variant, 1, 8, this->fp); // localeVariant
	write8(this->fp, screen_layout >> 8); // screenLayout2
	write8(this->fp, color); // colorMode
	write16(this->fp, 0); // screenConfigPad2
	// This bool field accounts for 4 bytes due to structure padding.
	write32(this->fp, false); // localeScriptWasComputed
	// I'm not sure where the following member is exactly placed, so I suppress it for now. It's added in Android Pie.
	0 && fwrite(locale_numbering_system, 1, 8, this->fp); // localeNumberingSystem
	assert(ftell(this->fp) - this->config_start == 56);

	for (size_t i = 0; i < this->spec_entry_count; i++) write32(this->fp, -1);
}

void arsc_end_configuration(struct arsc_file *this) {
	assert(this->config_start);
	if ((size_t) ftell(this->fp) == this->config_start + this->spec_entry_count * 4 + 56) {
		// The Android platform dislikes configurations with no entries, saying “ResTable_type entriesStart at 0x… extends beyond chunk end 0x….”
		// Setting entriesStart to 0 in this case would result in [INSTALL_PARSE_FAILED_NOT_APK: Failed to parse ….apk] on one of my devices.
		write32(this->fp, 0);
	}
	long size = ftell(this->fp) - this->config_start + 20;
	fseek(this->fp, this->config_start - 16, SEEK_SET);
	write32(this->fp, size); // size
	fseek(this->fp, 0, SEEK_END);
	this->config_start = 0;
}

static void arsc_index_entry(struct arsc_file *this, size_t index) {
	assert(this->config_start);
	assert(index < this->spec_entry_count);
	const long offset = ftell(this->fp) - this->spec_entry_count * 4 - this->config_start - 56;
	fseek(this->fp, this->config_start + index * 4 + 56, SEEK_SET);
	write32(this->fp, offset);
	fseek(this->fp, 0, SEEK_END);
}

// Simple entry
void arsc_entry(struct arsc_file *this, size_t index) {
	arsc_index_entry(this, index);
	// ResTable_entry
	write16(this->fp, 8); // size
	write16(this->fp, 0); // flags
	this->current_map_size = SIZE_MAX;
}

// Complex entry
void arsc_begin_entry(struct arsc_file *this, size_t index, const char *name) {
	arsc_index_entry(this, index);
	// ResTable_map_entry : public ResTable_entry
	write16(this->fp, 16); // size
	write16(this->fp, 0x0001); // FLAG_COMPLEX
	write32(this->fp, arsc_intern(&this->keys, name)); // key
	write32(this->fp, 0); // parent
	write32(this->fp, 0); // count (to be filled in)
	this->current_map_size = 0;
}

void arsc_end_entry(struct arsc_file *this) {
	assert(this->config_start);
	fseek(this->fp, -(long) this->current_map_size * 12 - 4, SEEK_CUR);
	write32(this->fp, this->current_map_size); // count
	fseek(this->fp, 0, SEEK_END);
	this->current_map_size = SIZE_MAX;
}

void arsc_set_data(struct arsc_file *this, const char *name, uint8_t type, uint32_t data) {
	if (this->current_map_size == SIZE_MAX) {
		write32(this->fp, arsc_intern(&this->keys, name)); // key in ResTable_entry
	} else {
		this->current_map_size++;
		if (name) {
			static const char names[][6] = {
				"type", "min", "max", "l10n",
				"other", "zero", "one", "two", "few", "many",
			};
			for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
				if (strcmp(names[i], name) == 0) {
					write32(this->fp, 0x01000000 | i); // name in ResTable_map
					goto write_data;
				}
			}
			// I don't know which pool, if any, the name field in ResTable_map refers to.
			abort();
		} else {
			write32(this->fp, 0x02000000 | this->current_map_size - 1); // name in ResTable_map
		}
	}
write_data:
	// Res_value
	write16(this->fp, 8); // size
	write8(this->fp, 0); // res0
	write8(this->fp, type); // dataType
	write32(this->fp, data); // data
}

void arsc_set_int32(struct arsc_file *this, const char *name, int32_t value) {
	arsc_set_data(this, name, 0x10, value); // TYPE_INT_DEC
}

void arsc_set_string(struct arsc_file *this, const char *name, const char *value) {
	arsc_set_data(this, name, 0x03, arsc_intern(&this->string_pool, value)); // TYPE_STRING
}



struct axml_file {
	FILE *contents;
	size_t attr_count;
	bool inside_header;
	struct arsc_string_pool string_pool;
	struct {
		long pos;
		size_t ns;
		size_t name;
		size_t attribute_count;
	} stack[16];
	size_t stack_top;
};

void axml_begin_file(struct axml_file *this) {
	this->contents = tmpfile();
	if (!this->contents) {
		perror("tmpfile");
		exit(EXIT_FAILURE);
	}
	// The resource map is not optional at all because some read attributes by ID and some by name.
	// The first empty entry in a string pool is mandatory in this implementation.
	// Its attribute ID is set arbitrarily as long as there is no conflict.
	this->attr_count = 1;
	arsc_begin_string_pool(&this->string_pool);
	this->inside_header = true;
	this->stack_top = 0;
	write32(this->contents, 0x0101fffe);
}

static void axml_end_header(struct axml_file *this) {
	if (this->inside_header) {
		this->inside_header = false;
		// Files converted from textual XML are naturally with RES_XML_START_NAMESPACE_TYPE and RES_XML_END_NAMESPACE_TYPE blocks, but they are unused in PackageParser, so we don't need them.
	}
}

void axml_end_file_fp(struct axml_file *this, FILE *fp) {
	// A valid XML document must have a root tag.
	assert(!this->inside_header);
	assert(!this->stack_top);

	const long start = ftell(fp);
	write16(fp, 0x0003); // RES_XML_TYPE
	write16(fp, 8); // headerSize
	write32(fp, 0); // size (to be calculated)

	arsc_end_string_pool(&this->string_pool, fp);

	write16(fp, 0x0180); // RES_XML_RESOURCE_MAP_TYPE
	write16(fp, 8); // headerSize
	write32(fp, this->attr_count * 4 + 8); // size
	rewind(this->contents);
	copy_file_fp(fp, this->contents);

	long size = ftell(fp) - start;
	fseek(fp, start + 4, SEEK_SET);
	write32(fp, size);

	fclose(this->contents);
}

void axml_end_file(struct axml_file *this, const char *filename) {
	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	axml_end_file_fp(this, fp);
	fclose(fp);
}

void axml_define_attr(struct axml_file *this, uint32_t id, const char *name) {
	assert(this->inside_header);
	assert(name && *name);
	write32(this->contents, id);
	arsc_intern(&this->string_pool, name);
	this->attr_count++;
	assert(this->string_pool.count == this->attr_count);
}

void axml_end_begin_element(struct axml_file *this) {
	if (!this->stack_top) return;
	size_t n = this->stack[this->stack_top - 1].attribute_count;
	if (!n) return;
	fseek(this->contents, this->stack[this->stack_top - 1].pos + 4, SEEK_SET);
	write32(this->contents, n * 20 + 36);
	fseek(this->contents, this->stack[this->stack_top - 1].pos + 28, SEEK_SET);
	write16(this->contents, n);
	fseek(this->contents, 0, SEEK_END);
}

void axml_begin_element(struct axml_file *this, const char *ns, const char *name) {
	axml_end_header(this);
	axml_end_begin_element(this);
	if (this->stack_top >= 16) fprintf(stderr, "%s\n", __func__), exit(EXIT_FAILURE);
	this->stack[this->stack_top].pos = ftell(this->contents);
	this->stack[this->stack_top].ns = arsc_intern(&this->string_pool, ns);
	this->stack[this->stack_top].name = arsc_intern(&this->string_pool, name);
	this->stack[this->stack_top].attribute_count = 0;
	this->stack_top++;
	write16(this->contents, 0x0102); // RES_XML_START_ELEMENT_TYPE
	write16(this->contents, 16); // headerSize
	write32(this->contents, 36); // size
	write32(this->contents, 0); // lineNumber
	write32(this->contents, -1); // comment
	write32(this->contents, this->stack[this->stack_top - 1].ns); // ns
	write32(this->contents, this->stack[this->stack_top - 1].name); // name
	write16(this->contents, 20); // attributeStart
	write16(this->contents, 20); // attributeSize
	write16(this->contents, 0); // attributeCount
	write16(this->contents, 0); // idIndex
	write16(this->contents, 0); // classIndex
	write16(this->contents, 0); // styleIndex
}

void axml_end_element(struct axml_file *this) {
	assert(!this->inside_header);
	axml_end_begin_element(this);
	this->stack_top--;
	write16(this->contents, 0x0103); // RES_XML_END_ELEMENT_TYPE
	write16(this->contents, 16); // headerSize
	write32(this->contents, 24); // size
	write32(this->contents, 0); // lineNumber
	write32(this->contents, -1); // comment
	write32(this->contents, this->stack[this->stack_top].ns); // ns
	write32(this->contents, this->stack[this->stack_top].name); // name
}

void axml_set_attribute(struct axml_file *this, const char *ns, const char *name, const char *raw_value, uint8_t type, uint32_t data) {
	assert(!this->inside_header);
	assert(this->stack_top);
	axml_end_begin_element(this);

	this->stack[this->stack_top - 1].attribute_count++;
	// The ns field points to the URI (instead of the name) of the desired namespace.
	// aapt always shows the name whether the ns field points to the name or the URI.
	// If the field is set incorrectly in AndroidManifest.xml, one will be greeted with the error “No value supplied for <android:name>” when installing the APK.
	write32(this->contents, arsc_intern(&this->string_pool, ns)); // ns
	write32(this->contents, arsc_intern(&this->string_pool, name)); // name
	write32(this->contents, arsc_intern(&this->string_pool, raw_value)); // rawValue
	write16(this->contents, 8); // typedValue.size
	write8(this->contents, 0); // typedValue.res0
	write8(this->contents, type); // typedValue.type
	write32(this->contents, data); // typedValue.data
	long offset = 0;
	if (strcmp(name, "id") == 0) {
		offset = 30;
	} else if (strcmp(name, "class") == 0) {
		offset = 32;
	} else if (strcmp(name, "style") == 0) {
		offset = 34;
	}
	if (offset) {
		fseek(this->contents, this->stack[this->stack_top - 1].pos + offset, SEEK_SET);
		write16(this->contents, this->stack[this->stack_top - 1].attribute_count);
		fseek(this->contents, 0, SEEK_END);
	}
}

void axml_set_int32(struct axml_file *this, const char *ns, const char *name, int32_t data) {
	axml_set_attribute(this, ns, name, NULL, 0x10, data); // TYPE_INT_DEC
}

void axml_set_string(struct axml_file *this, const char *ns, const char *name, const char *data) {
	// `aapt dump xmltree` doesn't even care about the typed data if the type is TYPE_STRING.
	// It only looks for the raw value, outputting it twice.
	axml_set_attribute(this, ns, name, data, 0x03, arsc_intern(&this->string_pool, data)); // TYPE_STRING
}

void axml_set_bool(struct axml_file *this, const char *ns, const char *name, bool data) {
	// In the case of TYPE_INT_BOOLEAN, ResourceTypes.h says that “The 'data' is either 0 or 1, for input "false" or "true" respectively.”
	// Nevertheless, aapt packages true as 0xffffffff.
	axml_set_attribute(this, ns, name, NULL, 0x12, data ? 0xffffffff : 0); // TYPE_INT_BOOLEAN
}

void axml_set_reference(struct axml_file *this, const char *ns, const char *name, uint32_t data) {
	axml_set_attribute(this, ns, name, NULL, 0x01, data); // TYPE_REFERENCE
}

int main(int argc, char **argv) {
	struct axml_file m;
	axml_begin_file(&m);
	axml_define_attr(&m, 0x0101021b, "versionCode");
	axml_define_attr(&m, 0x0101021c, "versionName");
	axml_define_attr(&m, 0x0101020c, "minSdkVersion");
	axml_define_attr(&m, 0x01010270, "targetSdkVersion");
	axml_define_attr(&m, 0x01010003, "name");
	axml_define_attr(&m, 0x01010001, "label");
	axml_define_attr(&m, 0x01010002, "icon");
	axml_define_attr(&m, 0x01010018, "authorities");
	axml_define_attr(&m, 0x0101001b, "grantUriPermissions");
	axml_define_attr(&m, 0x01010010, "exported");

	#define ANDROID_RESOURCES "http://schemas.android.com/apk/res/android"
	axml_begin_element(&m, NULL, "manifest");
		axml_set_int32(&m, ANDROID_RESOURCES, "versionCode", 1);
		axml_set_string(&m, ANDROID_RESOURCES, "versionName", "哼，哼，啊啊啊啊啊");
		axml_set_string(&m, NULL, "package", "net.hanshq.hello");
		axml_begin_element(&m, NULL, "uses-sdk");
			axml_set_int32(&m, ANDROID_RESOURCES, "minSdkVersion", 10);
			axml_set_int32(&m, ANDROID_RESOURCES, "targetSdkVersion", 29);
		axml_end_element(&m);
		axml_begin_element(&m, NULL, "uses-permission");
			axml_set_string(&m, ANDROID_RESOURCES, "name", "android.permission.REQUEST_INSTALL_PACKAGES");
		axml_end_element(&m);
		axml_begin_element(&m, NULL, "application");
			axml_set_string(&m, ANDROID_RESOURCES, "label", "我的第一个 Java 应用程序");
			axml_set_reference(&m, ANDROID_RESOURCES, "icon", 0x7f080000);
			axml_begin_element(&m, NULL, "activity");
				axml_set_string(&m, ANDROID_RESOURCES, "name", ".MainActivity");
				axml_set_bool(&m, ANDROID_RESOURCES, "exported", true);
				axml_begin_element(&m, NULL, "intent-filter");
					axml_begin_element(&m, NULL, "action");
						axml_set_string(&m, ANDROID_RESOURCES, "name", "android.intent.action.MAIN");
					axml_end_element(&m);
					axml_begin_element(&m, NULL, "category");
						axml_set_string(&m, ANDROID_RESOURCES, "name", "android.intent.category.LAUNCHER");
					axml_end_element(&m);
				axml_end_element(&m);
			axml_end_element(&m);
			axml_begin_element(&m, NULL, "provider");
				axml_set_string(&m, ANDROID_RESOURCES, "name", ".FileProvider");
				axml_set_bool(&m, ANDROID_RESOURCES, "exported", false);
				axml_set_string(&m, ANDROID_RESOURCES, "authorities", "net.hanshq.hello.FileProvider");
				axml_set_bool(&m, ANDROID_RESOURCES, "grantUriPermissions", true);
			axml_end_element(&m);
		axml_end_element(&m);
		axml_begin_element(&m, NULL, "uses-permission");
			axml_set_string(&m, ANDROID_RESOURCES, "name", "android.permission.REQUEST_INSTALL_PACKAGES");
		axml_end_element(&m);
	axml_end_element(&m);

	axml_end_file(&m, "AndroidManifest.xml");

	struct arsc_file r;
	arsc_begin_file(&r, "net.hanshq.hello");

	arsc_begin_type(&r, 0x04, 1); // string
		arsc_begin_configuration(&r, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			arsc_entry(&r, 0);
			arsc_set_string(&r, "abd", "Grass.");
		arsc_end_configuration(&r);
		arsc_begin_configuration(&r, 0, 0, "zh-CN", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			arsc_entry(&r, 0);
			arsc_set_string(&r, "abd", "草。");
		arsc_end_configuration(&r);
		arsc_begin_configuration(&r, 0, 0, "ja-JP", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			arsc_entry(&r, 0);
			arsc_set_string(&r, "abd", "くさ。");
		arsc_end_configuration(&r);
	arsc_end_type(&r);
	arsc_begin_type(&r, 0x07, 1); // array
		arsc_begin_configuration(&r, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			arsc_begin_entry(&r, 0, "abe");
				arsc_set_int32(&r, NULL, 114);
				arsc_set_int32(&r, NULL, 514);
				arsc_set_int32(&r, NULL, -1919);
				arsc_set_int32(&r, NULL, 810);
			arsc_end_entry(&r);
		arsc_end_configuration(&r);
	arsc_end_type(&r);
	arsc_begin_type(&r, 0x08, 1); // drawable
		arsc_begin_configuration(&r, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			arsc_entry(&r, 0);
			arsc_set_string(&r, "icon", "a.png");
		arsc_end_configuration(&r);
	arsc_end_type(&r);
	arsc_begin_type(&r, 0x12, 1); // plurals
		arsc_begin_configuration(&r, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			arsc_begin_entry(&r, 0, "pp");
				arsc_set_string(&r, "one", "one's string");
				arsc_set_string(&r, "other", "other's string");
			arsc_end_entry(&r);
		arsc_end_configuration(&r);
	arsc_end_type(&r);

	arsc_end_file(&r, "resources.arsc");
}
