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
	struct arsc_string_pool string_pool;
};

void arsc_begin_file(struct arsc_file *this) {
	this->fp = tmpfile();
	if (!this->fp) {
		perror("tmpfile");
		exit(EXIT_FAILURE);
	}
	arsc_begin_string_pool(&this->string_pool);
}

void arsc_end_file(struct arsc_file *this, const char *filename) {
	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	const long start = ftell(fp);
	write16(fp, 0x0002); // RES_TABLE_TYPE
	write16(fp, 12); // headerSize
	write32(fp, 0); // size (to be calculated)
	write32(fp, 1); // packageCount

	arsc_end_string_pool(&this->string_pool, fp);

	/*write16(fp, 0x0180); // RES_XML_RESOURCE_MAP_TYPE
	write16(fp, 8); // headerSize
	write32(fp, this->attr_count * 4 + 8); // size
	rewind(this->contents);
	copy_fp(fp, this->contents);*/

	long size = ftell(fp) - start;
	fseek(fp, start + 4, SEEK_SET);
	write32(fp, size);

	fclose(fp);
	fclose(this->fp);
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

void axml_end_file(struct axml_file *this, const char *filename) {
	// A valid XML document must have a root tag.
	assert(!this->inside_header);
	assert(!this->stack_top);

	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	const long start = ftell(fp);
	write16(fp, 0x0003); // RES_XML_TYPE
	write16(fp, 8); // headerSize
	write32(fp, 0); // size (to be calculated)

	arsc_end_string_pool(&this->string_pool, fp);

	write16(fp, 0x0180); // RES_XML_RESOURCE_MAP_TYPE
	write16(fp, 8); // headerSize
	write32(fp, this->attr_count * 4 + 8); // size
	rewind(this->contents);
	copy_fp(fp, this->contents);

	long size = ftell(fp) - start;
	fseek(fp, start + 4, SEEK_SET);
	write32(fp, size);

	fclose(fp);
	fclose(this->contents);
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
			axml_set_reference(&m, ANDROID_RESOURCES, "icon", 0x7f020000);
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

	axml_end_file(&m, "mani.bin");
}
