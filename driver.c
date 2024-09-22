#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#define INPUTSIZE 2048

void getInput(char* data[], char input_file[], char output_file[], int* background, int pid);
void execInput(char* data[], char input_file[], char output_file[], int* background, int pid, int *exit_status);

int main() {

	int pid = getpid();
	int exit_status = 0;
	int background = 0;
	char* input[INPUTSIZE];
	char input_file[512] = "";	
	char output_file[512] = "";

	int exit = 0;
	while (!exit) {
		memset(input, '\0', sizeof(input));
		input_file[0] = '\0';
		output_file[0] = '\0';
		background = 0;

		getInput(input, input_file, output_file, &background, pid);

		// prompt user again
		if (!input[0]) {
			continue;
		}

		// prompt user again if blank or commented
		else if (input[0][0] == '\0' || input[0][0] == '#') {
			fflush(stdout);
			continue;
		}

		// first built in command
		else if (!strcmp(input[0], "exit")) {
			exit = 1;
		}

		// second built in command
		else if (!strcmp(input[0], "status")) {
			if (WIFEXITED(exit_status)) {
				printf("Exited with signal: %d\n", WEXITSTATUS(exit_status));
			}
			else {
				printf("Terminated with signal: %d\n", WTERMSIG(exit_status));
			}
		}

		// third built in command
		else if (!strcmp(input[0], "cd")) {
			fflush(stdout);
			if (input[1]) {
				if (chdir(input[1]) == -1) {
					printf("Directory does not exist.\n");
					fflush(stdout);
				}
			}
			else {
				chdir(getenv("HOME"));
			}
		}

		// use exec for anything else
		else {
			fflush(stdout);
			execInput(input, input_file, output_file, &background, pid, &exit_status);
		}

	}

	return 0;
}

void getInput(char* data[], char input_file[], char output_file[], int* background, int pid) {
	char input[INPUTSIZE];
	char space[2] = " ";
	int check_background = 0;

	// get input from the user
	printf(": ");
	fflush(stdout);
	fgets(input, sizeof(input), stdin);

	// remove newline that occurs when ENTER is pressed
	for (int i = 0; i < sizeof(input); i++) {
		if (input[i] == '\n') {
			input[i] = '\0'; // null
		}
	}

	// check for blank input
	if (!strcmp(input, "")) {
		fflush(stdout);
		data[0] = strdup("");
		return;
	}

	// tokenize user input and parse for special characters
	// NOTE: data[] is like a 2D arr
	// ex: data[1] = "test", data[1][1] = "e"
	char* token = strtok(input, space);
	int i = 0;

	while (token) {
		check_background = 0;

		if (i >= 512) {
			printf("There is a max of 512 arguments.\n");
			fflush(stdout);
			break;
		}

		// ampersand
		if (!strcmp(token, "&")) {
			check_background = 1;
		}

		// input file
		else if (!strcmp(token, "<")) {
			token = strtok(NULL, space);
			if (token) {
				strcpy(input_file, token);
			}
		}
		// output file
		else if (!strcmp(token, ">")) {
			token = strtok(NULL, space);
			strcpy(output_file, token);
		}
		// arguments
		else {
			data[i] = strdup(token);
			for (int j = 0; data[i][j]; j++) {
				if (data[i][j] == '$' && data[i][j + 1] == '$') {
					data[i][j] = '\0';
					data[i][j + 1] = '\0';
					char buffer[512];
					snprintf(buffer, sizeof(buffer), "%d", pid);
					strcat(data[i], buffer);
				}
				fflush(stdout);
			}
		}

		token = strtok(NULL, space);
		i++;
	}

	// make sure the ampersand is at the end of the command line
	if (check_background) {
		*background = 1;
	}
}

void execInput(char* data[], char input_file[], char output_file[], int* background, int pid, int *exit_status) {
	int childStatus;
	int input_desc, output_desc;
	int input_copy, output_copy;

	// Fork a new process
	pid_t spawnPid = fork();

	if (spawnPid == -1) {
		perror("fork()\n");
		exit(1);
	}

	else if (spawnPid == 0) {
		// input files
		if (strcmp(input_file, "")) {
			input_desc = open(input_file, O_RDONLY);
			if (input_desc == -1) {
				perror("Input file failed to open().\n");
				exit(1);
			}

			// copy to use for execvp
			input_copy = dup2(input_desc, 1);
			if (input_copy == -1) {
				perror("Unable to copy input file.\n");
				exit(1);
			}
		}

		// output files
		if (strcmp(output_file, "")) {
			output_desc = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
			if (output_desc == -1) {
				perror("Output file failed to open().\n");
				exit(1);
			}

			// copy to use for execvp
			output_copy = dup2(output_desc, 1);
			if (output_copy == -1) {
				perror("Unable to copy output file.\n");
				exit(1);
			}
		}

		// execute the command w/ the args
		if (execvp(data[0], data) < 0) {
			perror("The specified file or directory does not exist.\n");
			fflush(stdout);
			exit(2);
		}

	} else {
		// In the parent process
		// Wait for child's termination
		if (*background) {
			spawnPid = waitpid(spawnPid, exit_status, WNOHANG);
			printf("childPid in background is %d\n", spawnPid);
			fflush(stdout);
			if (WIFEXITED(exit_status)) {
				printf("Exited with signal: %d\n", WEXITSTATUS(exit_status));
			}
			else {
				printf("Terminated with signal: %d\n", WTERMSIG(exit_status));
			}
		}
		else {
			spawnPid = waitpid(spawnPid, exit_status, 0);
		}
	}
}