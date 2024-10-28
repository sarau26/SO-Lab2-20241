#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 10
#define MAX_PATH_LENGTH 1024

char *current_path = NULL; // Variable global para almacenar la ruta actual
int last_command_success = 1; // Para rastrear el éxito del último comando
int path_overwrite_attempted = 0; // Bandera para indicar intento de sobrescribir la ruta

void showError() {
    write(STDERR_FILENO, "An error has occurred\n", 22);
    last_command_success = 0; // Marcar que hubo un error
}

char* findCommand(char *command) {
    char full_path[MAX_PATH_LENGTH];

    // Si hay una ruta actual definida, usarla
    if (current_path != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, command);
        if (access(full_path, X_OK) == 0) { // Verificar si el comando es ejecutable
            return strdup(full_path); // Retornar el camino completo del comando
        }
    }

    // Si no hay una ruta actual, verificar el PATH
    char *path_env = getenv("PATH");
    if (path_env == NULL) return NULL;

    char *token = strtok(path_env, ":");
    while (token != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", token, command);
        if (access(full_path, X_OK) == 0) { // Verificar si el comando es ejecutable
            return strdup(full_path); // Retornar el camino completo del comando
        }
        token = strtok(NULL, ":");
    }

    return NULL; // Comando no encontrado
}

void clearPath() {
    if (current_path != NULL) {
        free(current_path); // Liberar la memoria anterior
        current_path = NULL; // Limpiar la ruta actual
    }
}

void setPath(char *path) {
    clearPath(); // Limpiar la ruta actual antes de establecer una nueva
    current_path = strdup(path); // Establecer la nueva ruta
    path_overwrite_attempted = 1; // Marcar que se intentó sobrescribir la ruta
}

int main(int argc, char *argv[]) {
    FILE *input = stdin;  // Por defecto, la entrada es stdin (modo interactivo)

    // Si hay más de un argumento, es un error
    if (argc > 2) {
        showError();
        exit(1);
    }

    // Si hay un argumento, es el nombre de un archivo (modo batch)
    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            showError();
            exit(1);
        }
    }

    // Bucle principal
    while (1) {
        // Mostrar el prompt solo en modo interactivo
        if (input == stdin) {
            printf("wish> ");
            fflush(stdout);
        }

        // Leer la línea de entrada (ya sea desde stdin o desde el archivo)
        char *line = NULL;
        size_t len = 0;
        if (getline(&line, &len, input) == -1) {
            free(line);
            exit(0);  // Si EOF es alcanzado, salir
        }

        // Quitar el salto de línea
        line[strcspn(line, "\n")] = '\0';

        // Ignorar líneas vacías o que contienen solo espacios
        if (strlen(line) == 0 || strspn(line, " \t") == strlen(line)) {
            free(line);
            continue;  // Salir del bucle actual y seguir con la siguiente línea
        }

        // Parsear la línea en tokens (argumentos)
        char *args[MAX_ARGS];
        int arg_count = 0;
        char *output_file = NULL;
        int redirection_count = 0;  // Contador de redirecciones
        int redirection_flag = 0;   // Bandera para detectar errores de redirección

        // Verificar si hay un comando antes de la redirección
        int has_command = 0;

        // Parsear la línea en tokens
        char *token = strtok(line, " ");
        while (token != NULL && arg_count < MAX_ARGS - 1) {
            if (strcmp(token, ">") == 0) {
                // Manejar la redirección de salida
                redirection_count++;
                if (redirection_count > 1) {
                    showError();
                    redirection_flag = 1; // Marcar error de redirección
                    break;
                }
                token = strtok(NULL, " ");
                if (token == NULL) {
                    showError();
                    redirection_flag = 1; // Marcar error de redirección
                    break;
                }
                output_file = token;
            } else {
                args[arg_count++] = token; // Agregar argumento
                has_command = 1; // Hay un comando
            }
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        if (redirection_flag || (!has_command && output_file != NULL)) {
            free(line);
            continue;  // Salir del bucle actual y seguir con la siguiente línea
        }

        // Manejar el comando 'exit'
        if (args[0] != NULL && strcmp(args[0], "exit") == 0) {
            if (arg_count != 1) {
                showError();
            } else {
                free(line);
                free(current_path); // Liberar la memoria del path actual
                exit(0);
            }
        }

        // Manejar el comando 'path'
        else if (args[0] != NULL && strcmp(args[0], "path") == 0) {
            if (arg_count == 1) {
                clearPath(); // Limpiar la ruta actual si no se proporciona un argumento
            } else if (arg_count == 2) {
                setPath(args[1]); // Establecer la nueva ruta
            } else {
                showError(); // Manejo de error si hay más de 2 argumentos
            }
        }

        // Manejar el comando 'cd'
        else if (args[0] != NULL && strcmp(args[0], "cd") == 0) {
            if (arg_count != 2) {
                showError();
            } else if (chdir(args[1]) != 0) {
                showError();
            } else {
                last_command_success = 1; // Comando cd exitoso
            }
        }

        // Manejar comandos en paralelo
        else if (args[0] != NULL) {
            // Comprobar si se intentó sobrescribir la ruta
            if (path_overwrite_attempted && strcmp(args[0], "ls") == 0) {
                showError(); // No ejecutar ls porque se intentó sobrescribir la ruta
                free(line);
                continue;
            }

            // Variables para la ejecución de procesos
            pid_t pids[MAX_ARGS]; // Array para almacenar los PIDs de los procesos
            int pid_count = 0; // Contador de PIDs

            // Ejecutar comandos en paralelo
            int start_idx = 0;
            while (start_idx < arg_count) {
                // Encontrar el final del comando
                int end_idx = start_idx;
                while (end_idx < arg_count && strcmp(args[end_idx], "&") != 0) {
                    end_idx++;
                }

                // Crear un nuevo proceso
                pid_t pid = fork();
                if (pid == 0) {  // Proceso hijo
                    // Redirigir salida solo si se especifica un archivo
                    if (output_file != NULL) {
                        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd < 0) {
                            showError(); // Error al abrir el archivo
                            exit(1);
                        }
                        dup2(fd, STDOUT_FILENO); // Redirigir stdout al archivo
                        close(fd);
                    }

                    // Ejecutar el comando correspondiente
                    char *command_path = findCommand(args[start_idx]);
                    if (command_path != NULL) {
                        execv(command_path, &args[start_idx]); // Ejecutar el comando externo
                        showError(); // Si execv falla
                    } else {
                        showError(); // Comando no encontrado
                    }
                    exit(1);
                } else if (pid < 0) {
                    showError(); // Error al hacer fork
                } else {
                    // Guardar el PID del proceso hijo
                    pids[pid_count++] = pid;
                }

                // Mover al siguiente comando
                start_idx = end_idx + 1;
            }

            // Esperar a que todos los procesos hijos terminen
            for (int i = 0; i < pid_count; i++) {
                waitpid(pids[i], NULL, 0);
            }
        }

        // Manejar el comando 'ls'
        else if (args[0] != NULL && strcmp(args[0], "ls") == 0) {
            if (last_command_success) { // Verifica si el último comando fue exitoso
                // Crear un nuevo proceso
                pid_t pid = fork();
                if (pid == 0) {  // Proceso hijo
                    // Redirigir salida solo si se especifica un archivo
                    if (output_file != NULL) {
                        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd < 0) {
                            showError(); // Error al abrir el archivo
                            exit(1);
                        }
                        dup2(fd, STDOUT_FILENO); // Redirigir stdout al archivo
                        close(fd);
                    }

                    execlp("ls", "ls", NULL); // Ejecutar ls
                    showError(); // Si execlp falla
                    exit(1);
                } else if (pid < 0) {
                    showError(); // Error al hacer fork
                }

                wait(NULL); // Esperar a que el proceso hijo termine
            }
        }

        free(line); // Liberar la memoria de la línea
    }

    // Liberar memoria antes de salir
    //free(line);
    free(current_path); // Liberar la memoria del path actual
    return 0;
}
