#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 10
#define MAX_PATH_LENGTH 1024

char *current_path = NULL;
int last_command_success = 1;
int path_overwrite_attempted = 0;

void showError() {
    write(STDERR_FILENO, "An error has occurred\n", 22);
    last_command_success = 0;
}

void trim_whitespace(char *str) {
    char *end;
    while (*str == ' ') str++;
    end = str + strlen(str) - 1;
    while (end > str && *end == ' ') end--;
    *(end + 1) = '\0';
}

int redirect_output(char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        showError();
        return -1;
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        showError();
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

char* findCommand(char *command) {
    char full_path[MAX_PATH_LENGTH];
    if (current_path != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, command);
        if (access(full_path, X_OK) == 0) {
            return strdup(full_path);
        }
    }
    char *path_env = getenv("PATH");
    if (path_env == NULL) return NULL;
    char *token = strtok(path_env, ":");
    while (token != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", token, command);
        if (access(full_path, X_OK) == 0) {
            return strdup(full_path);
        }
        token = strtok(NULL, ":");
    }
    return NULL;
}

void clearPath() {
    if (current_path != NULL) {
        free(current_path);
        current_path = NULL;
    }
}

void setPath(char *path) {
    clearPath();
    current_path = strdup(path);
    path_overwrite_attempted = 1;
}

void execute_external_command(char **args, char *redirect_file) {
    char *command_path = findCommand(args[0]);
    if (command_path == NULL) {
        showError();
        return;
    }
    if (fork() == 0) {
        if (redirect_file != NULL && redirect_output(redirect_file) < 0) {
            exit(1);
        }
        execv(command_path, args);
        showError();
        exit(1);
    } else {
        int status;
        waitpid(-1, &status, 0);
    }
}

void execute_parallel(char *input) {
    char *command;
    char *saveptr;
    command = strtok_r(input, "&", &saveptr);
    
    while (command != NULL) {
        if (*command == '\0') {
            break;
        }
        if (strlen(command) > 0) {
            char *args[MAX_ARGS];
            int i = 0;
            char *redirect_file = NULL;

            // Dividir comando y redirección
            char *cmd = strtok(command, ">");
            char *file = strtok(NULL, ">");

            // Si hay un archivo de redirección, limpiar los espacios
            if (file != NULL) {
                trim_whitespace(file);
                redirect_file = file;
            }

            // Tokenizar el comando en argumentos
            char *token = strtok(cmd, " \t\n");
            while (token != NULL && i < MAX_ARGS - 1) {
                args[i++] = token;
                token = strtok(NULL, " \t\n");
            }
            args[i] = NULL;

            // Verificar que haya un comando para ejecutar
            if (args[0] != NULL) {
                pid_t pid = fork();
                if (pid == 0) {
                    // Proceso hijo

                    // Configurar redirección de salida si está especificada
                    if (redirect_file != NULL) {
                        int fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd < 0) {
                            showError();
                            exit(1);
                        }
                        if (dup2(fd, STDOUT_FILENO) < 0) {
                            showError();
                            close(fd);
                            exit(1);
                        }
                        close(fd);
                    }

                    // Ejecutar el comando externo
                    char *command_path = findCommand(args[0]);
                    if (command_path != NULL) {
                        execv(command_path, args);
                        showError();  // Si execv falla
                    } else {
                        showError();
                    }
                    exit(1);
                } else if (pid < 0) {
                    // Error al crear el proceso hijo
                    showError();
                }
            }
        }
        command = strtok_r(NULL, "&", &saveptr);
    }

    // Esperar a que todos los procesos hijos finalicen
    while (wait(NULL) > 0);
}



int main(int argc, char *argv[]) {
    FILE *input = stdin;
    if (argc > 2) {
        showError();
        exit(1);
    }
    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            showError();
            exit(1);
        }
    }
    while (1) {
        if (input == stdin) {
            printf("wish> ");
            fflush(stdout);
        }
        char *line = NULL;
        size_t len = 0;
        if (getline(&line, &len, input) == -1) {
            free(line);
            exit(0);
        }
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0 || strspn(line, " \t") == strlen(line)) {
            free(line);
            continue;
        }
        if (strcmp(line, "&") == 0 || strspn(line, " \t&") == strlen(line)) {
            free(line);
            continue;
        }
        if (strstr(line, "&") != NULL) {
            execute_parallel(line);
            free(line);
            continue;
        }
        char *args[MAX_ARGS];
        int arg_count = 0;
        char *output_file = NULL;
        int redirection_count = 0;
        int redirection_flag = 0;
        int has_command = 0;
        char *token = strtok(line, " ");
        if (strcmp(token, ">") == 0) {
            showError();
            free(line);
            continue;
        }
        while (token != NULL && arg_count < MAX_ARGS - 1) {
            if (strcmp(token, ">") == 0) {
                redirection_count++;
                if (redirection_count > 1) {
                    showError();
                    redirection_flag = 1;
                    break;
                }
                token = strtok(NULL, " ");
                if (token == NULL || strchr(token, ' ') != NULL) { 
                    showError();
                    redirection_flag = 1;
                    break;
                }
                output_file = token;
                token = strtok(NULL, " ");
                if (token != NULL) {
                    showError();
                    redirection_flag = 1;
                    break;
                }
            } else {
                args[arg_count++] = token;
                has_command = 1;
            }
            token = strtok(NULL, " ");
        }
        if (arg_count > 0 && strcmp(args[arg_count - 1], "&") == 0) {
            args[--arg_count] = NULL;
        }
        args[arg_count] = NULL;
        if (redirection_flag || (!has_command && output_file != NULL)) {
            free(line);
            continue;
        }
        if (args[0] != NULL && strcmp(args[0], "exit") == 0) {
            if (arg_count != 1) {
                showError();
            } else {
                free(line);
                free(current_path);
                exit(0);
            }
        } else if (args[0] != NULL && strcmp(args[0], "path") == 0) {
            if (arg_count == 1) {
                clearPath();
            } else if (arg_count == 2) {
                setPath(args[1]);
            } else {
                showError();
            }
        } else if (args[0] != NULL && strcmp(args[0], "cd") == 0) {
            if (arg_count != 2) {
                showError();
            } else if (chdir(args[1]) != 0) {
                showError();
            } else {
                last_command_success = 1;
            }
        } else if (args[0] != NULL) {
            if (path_overwrite_attempted && strcmp(args[0], "ls") == 0) {
                showError();
                free(line);
                continue;
            }
            pid_t pids[MAX_ARGS];
            int pid_count = 0;
            int start_idx = 0;
            while (start_idx < arg_count) {
                int end_idx = start_idx;
                while (end_idx < arg_count && strcmp(args[end_idx], "&") != 0) {
                    end_idx++;
                }
                pid_t pid = fork();
                if (pid == 0) {
                    if (output_file != NULL) {
                        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd < 0) {
                            showError();
                            exit(1);
                        }
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                    }
                    char *command_path = findCommand(args[start_idx]);
                    if (command_path != NULL) {
                        execv(command_path, &args[start_idx]);
                        showError();
                    } else {
                        showError();
                    }
                    exit(1);
                } else if (pid < 0) {
                    showError();
                } else {
                    pids[pid_count++] = pid;
                }
                start_idx = end_idx + 1;
            }
            for (int i = 0; i < pid_count; i++) {
                waitpid(pids[i], NULL, 0);
            }
        }
        free(line);
    }
    free(current_path);
    return 0;
}
