// https://android.googlesource.com/platform/frameworks/base/+/refs/heads/master/core/res/res/values/public.xml
// Standard attribute IDs defined in android.R.attr are dense starting with 0x01010000.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void check(int condition, const char *message) {
	if (!condition) {
		fprintf(stderr, "Error: %s\n", message);
		exit(EXIT_FAILURE);
	}
}

char names[3000][128];
unsigned max_id;

int main() {
	char ch;
	unsigned id = 0;
	while (scanf(" <%c", &ch) == 1) {
		if (ch == 'p' && scanf("ubli%*s %c", &ch) == 1) {
			if (ch == 't') check(scanf("ype = \"%*[^\"]\" %c", &ch) == 1, "\"type=\" expected");
			if (ch == 'n') {
				char name[128];
				scanf("ame = \"%127[^\"]%c", name, &ch);
				check(ch == '"', "name too long");
				if (scanf(" id = \"%x", &id) != 1) id++;
				if ((id & 0xffff0000u) == 0x01010000u) {
					printf("[%#010x] = %s,\n", id, name);
					strcpy(names[id & 0xffff], name);
					if (id > max_id) max_id = id;
				}
			} else if (ch == 'f') {
				check(scanf("irst-id = \"%x", &id) == 1, "group without first-id");
				id--;
			}
		} else if (strchr("/?re", ch)) {
			// Skip end tags, processing instructions, <resources> and <eat-comment>.
		} else if (ch == '!') {
			// Skip a comment node.
			char a = 0, b = 0, c = 0;
			while (a != '-' || b != '-' || c != '>') {
				a = b;
				b = c;
				c = getchar();
			}
			continue;
		} else {
			fprintf(stderr, "Error: unexpected character '%c' (%d) after '<'\n", ch, ch);
			break;
		}
		scanf("%*[^>]>");
	}
	check(feof(stdin), "unexpected input");
	FILE *f = fopen("public.stringpool", "wb");
	for (unsigned i = 0; i <= (max_id & 0xffff); i++) {
		fputs(names[i], f);
		fputc(0, f);
	}
}
