#include <stdio.h>

int main() {
  char s[20];
	scanf("%s", s);
	for (char* ptr = s; *ptr; ++ptr) {
    printf("%02x ", *ptr);
	}
	return 0;
}
