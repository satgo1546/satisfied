// https://justanapplication.wordpress.com/android-internals/

// https://android.googlesource.com/platform/frameworks/base/+/master/libs/androidfw/include/androidfw/ResourceTypes.h
// https://android.googlesource.com/platform/frameworks/base/+/master/libs/androidfw/ResourceTypes.cpp
// The above files are placed accordingly as of Android S.
// They have long existed before Android 1.6, but the directory structure has changed much and may still change in the future.

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bits.c"

size_t utf8_utf16_length(const char *s) {
	size_t r = 0;
	while (*s) {
		r += ((*s & 0xc0) != 0x80) + ((unsigned char) *s >= 0xf0);
		s++;
	}
	return r;
}

struct axml_file {
	FILE *fp;
	FILE *contents;
	size_t attr_count;
	char *string_pool;
	const char *string_pool_end;
	size_t string_count;
	bool inside_header;
	struct {
		long pos;
		size_t ns;
		size_t name;
		size_t attribute_count;
	} stack[16];
	size_t stack_top;
};

size_t axml_intern(struct axml_file *this, const char *s) {
	// getAttributeNamespaceID returns -1 if there is no namespace.
	if (!s) return SIZE_MAX;
	if (!*s) return 0;
	char *p;
	size_t n = 1;
	for (p = this->string_pool; *p; p += strlen(p) + 1) {
		if (strcmp(s, p) == 0) return n;
		n++;
	}
	if (p + strlen(s) + 1 >= this->string_pool_end) fprintf(stderr, "%s\n", __func__), exit(EXIT_FAILURE);
	strcpy(p, s);
	this->string_count = n + 1;
	return n;
}

// UTF8_FLAG gets introduced in Android 2.2.
// utf8_to_utf16_length is used to get the “length” of a string in ResourceTypes.cpp.
// The “length” field is therefore in UTF-16 codes (i.e., codepoints outside BMP count as two) instead of codepoints.

void axml_begin_file(struct axml_file *this, const char *filename) {
	this->fp = fopen(filename, "wb");
	if (!this->fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	this->contents = tmpfile();
	if (!this->contents) {
		perror("tmpfile");
		exit(EXIT_FAILURE);
	}
	// This is a hack to ease string-pooling.
	// The first string/attribute is always the empty string.
	// Its ID is set arbitrarily as long as there is no conflict.
	this->attr_count = 1;
	this->string_pool = calloc(4096, 1);
	this->string_pool_end = this->string_pool + 4096;
	this->string_count = 1;
	this->inside_header = true;
	this->stack_top = 0;
	write32(this->contents, 0x0101fffe);
}

void axml_end_header(struct axml_file *this) {
	if (this->inside_header) {
		this->inside_header = false;
	}
}

void axml_end_file(struct axml_file *this) {
	// A valid XML document must have a root tag.
	assert(!this->inside_header);
	assert(!this->stack_top);

	const long file_start = ftell(this->fp);
	write16(this->fp, 0x0003); // RES_XML_TYPE
	write16(this->fp, 8); // headerSize
	write32(this->fp, 0); // size (to be calculated)

	long start = ftell(this->fp);
	write16(this->fp, 0x0001); // RES_STRING_POOL_TYPE
	write16(this->fp, 28); // headerSize
	write32(this->fp, 0); // size (to be calculated)
	write32(this->fp, this->string_count); // stringCount
	write32(this->fp, 0); // styleCount
	write32(this->fp, 1 << 8); // UTF8_FLAG
	write32(this->fp, this->string_count * 4 + 28); // stringsStart
	write32(this->fp, 0); // stylesStart
	for (size_t i = 0; i < this->string_count; i++) write32(this->fp, 0);

	long pos[this->string_count];
	pos[0] = ftell(this->fp);
	fputc(0, this->fp);
	fputc(0, this->fp);
	fputc(0, this->fp);
	const char *p = this->string_pool;
	for (size_t i = 1; i < this->string_count; i++) {
		pos[i] = ftell(this->fp);
		size_t len = strlen(p);
		writelen7or15(this->fp, utf8_utf16_length(p));
		writelen7or15(this->fp, len);
		fwrite(p, 1, len + 1, this->fp);
		p += len + 1;
	}
	write_padding_to(this->fp, 4);

	long size = ftell(this->fp) - start;
	fseek(this->fp, start + 4, SEEK_SET);
	write32(this->fp, size);
	fseek(this->fp, start + 28, SEEK_SET);
	for (size_t i = 0; i < this->string_count; i++) {
		write32(this->fp, pos[i] - pos[0]);
	}
	fseek(this->fp, 0, SEEK_END);

	write16(this->fp, 0x0180); // RES_XML_RESOURCE_MAP_TYPE
	write16(this->fp, 8); // headerSize
	write32(this->fp, this->attr_count * 4 + 8); // size
	rewind(this->contents);
	copy_fp(this->fp, this->contents);

	size = ftell(this->fp) - file_start;
	fseek(this->fp, file_start + 4, SEEK_SET);
	write32(this->fp, size);

	fclose(this->fp);
	fclose(this->contents);
}

void axml_define_attr(struct axml_file *this, uint32_t id, const char *name) {
	assert(this->inside_header);
	assert(name && *name);
	write32(this->contents, id);
	axml_intern(this, name);
	this->attr_count++;
	assert(this->string_count == this->attr_count);
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
	this->stack[this->stack_top].ns = axml_intern(this, ns);
	this->stack[this->stack_top].name = axml_intern(this, name);
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
	#define ANDROID_RESOURCES "http://schemas.android.com/apk/res/android"
	write32(this->contents, axml_intern(this, ns && strcmp(ns, "android") == 0 ? ANDROID_RESOURCES : NULL)); // ns
	write32(this->contents, axml_intern(this, name)); // name
	write32(this->contents, axml_intern(this, raw_value)); // rawValue
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
	axml_set_attribute(this, ns, name, data, 0x03, axml_intern(this, data)); // TYPE_STRING
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
	axml_begin_file(&m, "mani.bin");
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

	axml_begin_element(&m, NULL, "manifest");
		axml_set_int32(&m, "android", "versionCode", 1);
		axml_set_string(&m, "android", "versionName", "哼，哼，啊啊啊啊啊");
		axml_set_string(&m, NULL, "package", "com.example.hello");
		axml_begin_element(&m, NULL, "uses-sdk");
			axml_set_int32(&m, "android", "minSdkVersion", 10);
			axml_set_int32(&m, "android", "targetSdkVersion", 29);
		axml_end_element(&m);
		axml_begin_element(&m, NULL, "uses-permission");
			axml_set_string(&m, "android", "name", "android.permission.REQUEST_INSTALL_PACKAGES");
		axml_end_element(&m);
		axml_begin_element(&m, NULL, "application");
			axml_set_string(&m, "android", "label", "我的第一个 Java 应用程序");
			axml_set_reference(&m, "android", "icon", 0x7f020000);
			axml_begin_element(&m, NULL, "activity");
				axml_set_string(&m, "android", "name", ".MainActivity");
				axml_begin_element(&m, NULL, "intent-filter");
					axml_begin_element(&m, NULL, "action");
						axml_set_string(&m, "android", "name", "android.intent.action.MAIN");
					axml_end_element(&m);
					axml_begin_element(&m, NULL, "category");
						axml_set_string(&m, "android", "name", "android.intent.category.LAUNCHER");
					axml_end_element(&m);
				axml_end_element(&m);
			axml_end_element(&m);
			axml_begin_element(&m, NULL, "provider");
				axml_set_string(&m, "android", "name", ".FileProvider");
				axml_set_bool(&m, "android", "exported", false);
				axml_set_string(&m, "android", "authorities", "com.example.hello.FileProvider");
				axml_set_bool(&m, "android", "grantUriPermissions", true);
			axml_end_element(&m);
		axml_end_element(&m);
		axml_begin_element(&m, NULL, "uses-permission");
			axml_set_string(&m, "android", "name", "android.permission.REQUEST_INSTALL_PACKAGES");
		axml_end_element(&m);
	axml_end_element(&m);

	axml_end_file(&m);
}
