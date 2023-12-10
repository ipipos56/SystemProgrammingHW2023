#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

// Structure to represent a child process
struct ChildProcess {
	pid_t pid;
	struct ChildProcess* next;
};

// Linked list to store child processes
struct ChildProcessList {
	struct ChildProcess* head;
};

// Function to add a child process to the linked list
static void addChildProcess(struct ChildProcessList* list, pid_t pid) {
	struct ChildProcess* newProcess = malloc(sizeof(struct ChildProcess));
	if (newProcess == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	newProcess->pid = pid;
	newProcess->next = list->head;
	list->head = newProcess;
}

// Function to wait for all child processes to finish
static void waitForChildProcesses(struct ChildProcessList* list) {
	struct ChildProcess* current = list->head;
	while (current != NULL) {
		int status;
		waitpid(current->pid, &status, 0);
		struct ChildProcess* temp = current;
		current = current->next;
		free(temp);
	}
	list->head = NULL;  // Reset the list
	free(list);
}


// handle_cd
static bool handle_cd(const struct expr* e) {
	if (strcmp(e->cmd.exe, "cd") != 0) return false;
	if (e->cmd.arg_count == 0) {
		chdir(getenv("HOME"));
	} else {
		chdir(e->cmd.args[0]);
	}
	return true;
}

static void handle_exit(const struct expr* e) {
	if (strcmp(e->cmd.exe, "exit") != 0) return;
	if (e->cmd.arg_count != 0) return;
	if (e->next != NULL) return;
	exit(0);
}

static void
my_execute_command_line(const struct command_line* line) {
	assert(line != NULL);
	const struct expr* e = line->head;

	int prev_fd[2] = {-1, -1};
	int curr_fd[2] = {-1, -1};

	struct ChildProcessList* childList = malloc(sizeof(struct ChildProcessList));
	if (childList == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	childList->head = NULL;

	while (e != NULL) {
		if (e->next != NULL && e->next->type == EXPR_TYPE_PIPE) {
			if (pipe(curr_fd) == -1) {
				perror("pipe");
				exit(EXIT_FAILURE);
			}
			// printf("pipe %d %d\n", curr_fd[0], curr_fd[1]);
		}
		if (e->type == EXPR_TYPE_COMMAND && !handle_cd(e)) {
			handle_exit(e);
			char** args = malloc((e->cmd.arg_count + 2) * sizeof(char *));
			args[0] = e->cmd.exe;
			for (uint32_t i = 0; i < e->cmd.arg_count; ++i) {
				args[i + 1] = e->cmd.args[i];
			}
			args[e->cmd.arg_count + 1] = NULL;

			const pid_t pid = fork();
			switch (pid) {
				case -1:
					perror("fork");
					free(childList);
					exit(EXIT_FAILURE);
				case 0:
					if (prev_fd[0] != -1) {
						// printf("exe %s dup2 %d %d\n", e->cmd.exe, prev_fd[0], STDIN_FILENO);
						close(prev_fd[1]);
						if (dup2(prev_fd[0], STDIN_FILENO) == -1) {
							perror("dup2");
							exit(EXIT_FAILURE);
						}
						close(prev_fd[0]);
					}
					if (curr_fd[1] != -1) {
						// printf("exe %s dup2 %d %d\n", e->cmd.exe, curr_fd[1], STDOUT_FILENO);
						close(curr_fd[0]);
						if (dup2(curr_fd[1], STDOUT_FILENO) == -1) {
							perror("dup2");
							exit(EXIT_FAILURE);
						}
						close(curr_fd[1]);
					} else {
						int out_fd = STDOUT_FILENO;
						switch (line->out_type) {
							case OUTPUT_TYPE_FILE_NEW:
								out_fd = open(line->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
							break;
							case OUTPUT_TYPE_FILE_APPEND:
								out_fd = open(line->out_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
							break;
							default:
								break;
						}
						if (out_fd == -1) {
							perror("open");
							exit(EXIT_FAILURE);
						}
						if (out_fd != STDOUT_FILENO) {
							if (dup2(out_fd, STDOUT_FILENO) == -1) {
								perror("dup2");
								exit(EXIT_FAILURE);
							}
							close(out_fd);
						}
					}
					execvp(e->cmd.exe, args);
					perror("execvp");
					exit(EXIT_FAILURE);
				default:
					addChildProcess(childList, pid);
			}
			free(args);
		} else if (e->type == EXPR_TYPE_PIPE) {
			if (prev_fd[0] != -1) close(prev_fd[0]);
			if (prev_fd[1] != -1) close(prev_fd[1]);
			prev_fd[0] = curr_fd[0];
			prev_fd[1] = curr_fd[1];
			curr_fd[0] = -1;
			curr_fd[1] = -1;
		} else if (e->type == EXPR_TYPE_AND) {
			// printf("\tAND\n");
		} else if (e->type == EXPR_TYPE_OR) {
			// printf("\tOR\n");
		} else {
			printf("Unknown expr type %d\n", e->type);
			// assert(false);
		}
		e = e->next;
	}
	if (prev_fd[0] != -1) close(prev_fd[0]);
	if (prev_fd[1] != -1) close(prev_fd[1]);
	waitForChildProcesses(childList);
}

int
main(void) {
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
	struct parser* p = parser_new();
	// printf("> ");
	// fflush(stdout);
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
		parser_feed(p, buf, rc);
		struct command_line* line = NULL;
		while (true) {
			const enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			if (err != PARSER_ERR_NONE) {
				printf("Error: %d\n", (int) err);
				continue;
			}
			my_execute_command_line(line);
			command_line_delete(line);
		}
		// printf("> ");
		// fflush(stdout);
	}
	parser_delete(p);
	return 0;
}
