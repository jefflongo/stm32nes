#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "mapper.h"

void nestest() {
	FILE* test = fopen("nestest.txt", "rb");
	if (!load_rom("nestest.nes") || !test) {
		printf("File not found!\n");
		return;
	}
	init();
	char state[100];
	char exp[100];
	char line[100];
	char buffer[10];
	while (fgets(line, sizeof(line), test)) {
		log(state, sizeof(state));
		strcpy(exp, "PC:");
		strncpy(buffer, line, 4);
		buffer[4] = '\0';
		strcat(exp, buffer);
		strcat(exp, " A:");
		strncpy(buffer, line + 50, 2);
		buffer[2] = '\0';
		strcat(exp, buffer);
		strcat(exp, " X:");
		strncpy(buffer, line + 55, 2);
		buffer[2] = '\0';
		strcat(exp, buffer);
		strcat(exp, " Y:"); 
		strncpy(buffer, line + 60, 2);
		buffer[2] = '\0';
		strcat(exp, buffer);
		strcat(exp, " P:");
		strncpy(buffer, line + 65, 2);
		buffer[2] = '\0';
		strcat(exp, buffer);
		strcat(exp, " SP:");
		strncpy(buffer, line + 71, 2);
		buffer[2] = '\0';
		strcat(exp, buffer);
		strcat(exp, " CYC:");
		strcpy(buffer, line + 90);
		int count = 0;
		while (line[90 + count] != '\r') count++;
		buffer[count] = '\0';
		strcat(exp, buffer);
		if (strcmp(state, exp)) {
			printf("FAILED\nExpected %s\nGot      %s\n", exp, state);
			getchar();
		}
		printf("%s\n", state);
		run();
	}
}

int main() {
	nestest();
    return 0;
}