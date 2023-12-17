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

static void freeChildProcessNode(struct ChildProcess* processNode) {
    if (processNode != NULL) {
        free(processNode);
    }
}

// Function to wait for all child processes to finish
static int waitForChildProcesses(struct ChildProcessList* list) {
    int lastStatus = 0;
    struct ChildProcess* current = list->head;
    while (current != NULL) {
        int status;
        waitpid(current->pid, &status, 0);
        if (WIFEXITED(status)) {
            lastStatus = WEXITSTATUS(status);
        }
        struct ChildProcess* temp = current;
        current = current->next;
        freeChildProcessNode(temp); // Free each ChildProcess node after processing
    }
    list->head = NULL;
    free(list);
    return lastStatus;
}



// handle_cd
static int handle_cd(const struct expr* e) {
    if (e->type != EXPR_TYPE_COMMAND) return 0;
    if (strcmp(e->cmd.exe, "cd") != 0) return 0;
    return 1;
}

static int handle_exit(const struct expr* e) {
    if (strcmp(e->cmd.exe, "exit") != 0) return -1;
    //if (e->cmd.arg_count != 0) return;
    if (e->next != NULL) return -1;
    if (e->cmd.arg_count != 0) {
        return atoi(e->cmd.args[0]);
    } else
        return 0;
}

struct ExecutionResult {
    int exitCode;
    int forceExitCode;
};


struct ExecutionResult forceExit(struct ExecutionResult execResult, struct ChildProcessList* childList, int code) {
    waitForChildProcesses(childList);
    execResult.forceExitCode = code;
    return execResult;
}

struct ExecutionResult gracefulExit(struct ExecutionResult execResult, struct ChildProcessList* childList) {
    int exitCode = waitForChildProcesses(childList);
    execResult.exitCode = exitCode;
    return execResult;
}

static struct ExecutionResult
my_execute_command_line(const struct command_line* line, struct ExecutionResult execResult) {
    assert(line != NULL);
    const struct expr* e = line->head;

    int prev_fd[2] = {-1, -1};
    int curr_fd[2] = {-1, -1};

    struct ChildProcessList* childList = malloc(sizeof(struct ChildProcessList));

    if (childList == NULL) {
        perror("malloc");
        return forceExit(execResult, childList, EXIT_FAILURE);
    }
    childList->head = NULL;
    //FILE * file_ptr = fopen("../tt.txt", "a");

    int first = 1;
    while (e != NULL) {
        if (e->next != NULL && e->next->type == EXPR_TYPE_PIPE) {
            if (pipe(curr_fd) == -1) {
                perror("pipe");
                return forceExit(execResult, childList, EXIT_FAILURE);
            }
            // printf("pipe %d %d\n", curr_fd[0], curr_fd[1]);
        }

        if (handle_cd(e)) {
            if (e->cmd.arg_count == 0) {
                chdir(getenv("HOME"));
            } else {
                chdir(e->cmd.args[0]);
            }
        } else
        if (e->type == EXPR_TYPE_COMMAND) {
            int exitCode = handle_exit(e);
            if (first == 1 && exitCode != -1) {
                return forceExit(execResult, childList, exitCode);
            }

            const pid_t pid = fork();
            switch (pid) {
                case -1:
                    perror("fork");
                    return forceExit(execResult, childList, EXIT_FAILURE);
                case 0:
                    if (prev_fd[0] != -1) {
                        // printf("exe %s dup2 %d %d\n", e->cmd.exe, prev_fd[0], STDIN_FILENO);
                        close(prev_fd[1]);
                        if (dup2(prev_fd[0], STDIN_FILENO) == -1) {
                            perror("dup2");
                            return forceExit(execResult, childList, EXIT_FAILURE);
                        }
                        close(prev_fd[0]);
                    }
                    if (curr_fd[1] != -1) {
                        // printf("exe %s dup2 %d %d\n", e->cmd.exe, curr_fd[1], STDOUT_FILENO);
                        close(curr_fd[0]);
                        if (dup2(curr_fd[1], STDOUT_FILENO) == -1) {
                            perror("dup2");
                            return forceExit(execResult, childList, EXIT_FAILURE);
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
                            return forceExit(execResult, childList, EXIT_FAILURE);
                        }
                        if (out_fd != STDOUT_FILENO) {
                            if (dup2(out_fd, STDOUT_FILENO) == -1) {
                                perror("dup2");
                                return forceExit(execResult, childList, EXIT_FAILURE);
                            }
                            close(out_fd);
                        }
                    }

                    char** args = malloc((e->cmd.arg_count + 2) * sizeof(char *));
                    args[0] = e->cmd.exe;
                    for (uint32_t i = 0; i < e->cmd.arg_count; ++i) {
                        args[i + 1] = e->cmd.args[i];
                    }
                    args[e->cmd.arg_count + 1] = NULL;

                    execvp(e->cmd.exe, args);

                    free(args);

                    return forceExit(execResult, childList, atoi(e->cmd.args[0]));
                default:
                    addChildProcess(childList, pid);
            }
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
//            printf("%s", e->cmd.exe);
//            printf("%s", e->cmd.args[0]);
//            printf("Unknown expr type %d\n", e->type);
            // assert(false);
        }
        e = e->next;
        first = 0;
    }
    if (prev_fd[0] != -1) close(prev_fd[0]);
    if (prev_fd[1] != -1) close(prev_fd[1]);
    return gracefulExit(execResult, childList);
}

int
main(void) {
//    heaph_get_alloc_count();
    const size_t buf_size = 4096;
    char buf[buf_size];
    int rc;
    struct parser* p = parser_new();
    // printf("> ");
    // fflush(stdout);
    struct ExecutionResult execResult = {-1, -1};
    while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
        parser_feed(p, buf, rc);
        struct command_line* line = NULL;
        while (true) {
            const enum parser_error err = parser_pop_next(p, &line);
            if (err == PARSER_ERR_NONE && line == NULL)
                break;
            if (err != PARSER_ERR_NONE) {
                printf("Error: %d\n", (int) err);
                command_line_delete(line);
                continue;
            }
            execResult = my_execute_command_line(line, execResult);
            command_line_delete(line);
            if (execResult.forceExitCode != -1) {
                break;
            }
        }
        // printf("> ");
        // fflush(stdout);
        if (execResult.forceExitCode != -1) {
            break;
        }
    }
    parser_delete(p);

    int exitCode = 0;
    if (execResult.forceExitCode != -1) {
        exitCode = execResult.forceExitCode;
    }
    if (execResult.exitCode != -1) {
        exitCode = execResult.exitCode;
    }
//    heaph_get_alloc_count();
//    free(execResult);
    return exitCode;
}
