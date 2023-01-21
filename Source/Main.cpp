#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Usage(const char *path) {
	const char *program = path + strlen(path) - 1;

	for (; program != path; --program) {
		if (*program == '/' || *program == '\\') {
			program += 1;
			break;
		}
	}

	fprintf(stderr, "Usage: %s audio_files\n", program);
	exit(0);
}

int main(int argc, char *argv[]) {
	if (argc <= 1){
		Usage(argv[0]);
	}



	return 0;
}
