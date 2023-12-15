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
        free(temp);
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


static int
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
    //FILE * file_ptr = fopen("../tt.txt", "a");

    int first = 1;
    while (e != NULL) {
        if (e->next != NULL && e->next->type == EXPR_TYPE_PIPE) {
            if (pipe(curr_fd) == -1) {
                perror("pipe");
                waitForChildProcesses(childList);
                exit(EXIT_FAILURE);
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
                waitForChildProcesses(childList);
                exit(exitCode);
            }
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
                    waitForChildProcesses(childList);
                    exit(EXIT_FAILURE);
                case 0:
                    if (prev_fd[0] != -1) {
                        // printf("exe %s dup2 %d %d\n", e->cmd.exe, prev_fd[0], STDIN_FILENO);
                        close(prev_fd[1]);
                        if (dup2(prev_fd[0], STDIN_FILENO) == -1) {
                            perror("dup2");
                            waitForChildProcesses(childList);
                            exit(EXIT_FAILURE);
                        }
                        close(prev_fd[0]);
                    }
                    if (curr_fd[1] != -1) {
                        // printf("exe %s dup2 %d %d\n", e->cmd.exe, curr_fd[1], STDOUT_FILENO);
                        close(curr_fd[0]);
                        if (dup2(curr_fd[1], STDOUT_FILENO) == -1) {
                            perror("dup2");
                            waitForChildProcesses(childList);
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
                            waitForChildProcesses(childList);
                            exit(EXIT_FAILURE);
                        }
                        if (out_fd != STDOUT_FILENO) {
                            if (dup2(out_fd, STDOUT_FILENO) == -1) {
                                perror("dup2");
                                waitForChildProcesses(childList);
                                exit(EXIT_FAILURE);
                            }
                            close(out_fd);
                        }
                    }


                    execvp(e->cmd.exe, args);

                    waitForChildProcesses(childList);
                    exit(atoi(e->cmd.args[0]));  // Exit with a failure status
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
    int lastExitStatus = waitForChildProcesses(childList);
    return lastExitStatus;
}

int
main(void) {
//#if CHECK_LEAKS == 1
//    heaph_get_alloc_count();
//#endif
    const size_t buf_size = 4096;
    char buf[buf_size];
    int rc;
    struct parser* p = parser_new();
    // printf("> ");
    // fflush(stdout);
    int lastExitStatus;
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
            lastExitStatus = my_execute_command_line(line);
            command_line_delete(line);
        }
        // printf("> ");
        // fflush(stdout);
    }
    parser_delete(p);
//#if CHECK_LEAKS == 1
//    heaph_get_alloc_count();
//#endif
    return lastExitStatus;
}
