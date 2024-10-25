#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 10

void showError() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
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

        // Ignorar líneas vacías
        if (strlen(line) == 0) {
            free(line);
            continue;  // Salir del bucle actual y seguir con la siguiente línea
        }

        // Parsear la línea en tokens (argumentos)
        char *args[MAX_ARGS];
        int arg_count = 0;
        char *token = strtok(line, " ");
        char *output_file = NULL;
        int redirection_count = 0;  // Contador de redirecciones
        int redirection_flag = 0;   // Bandera para detectar errores de redirección
        
        // Verificar si hay un comando antes de la redirección
        int has_command = 0;

        while (token != NULL && arg_count < MAX_ARGS - 1) {
            if (strcmp(token, ">") == 0) {
                // Contar el número de redirecciones
                redirection_count++;
                if (redirection_count > 1) {
                    // Si hay más de un '>', mostrar error y salir del bucle
                    showError();
                    redirection_flag = 1;
                    break;
                }
                // Redirección de salida
                token = strtok(NULL, " ");
                if (token == NULL) {
                    // Si no hay archivo especificado después de '>', mostrar error
                    showError();
                    redirection_flag = 1;
                    break;
                }
                output_file = token;
                // Comprobar si hay múltiples archivos después de la redirección
                token = strtok(NULL, " ");
                if (token != NULL) {
                    // Si hay más de un archivo especificado, mostrar error
                    showError();
                    redirection_flag = 1;
                    break;
                }
            } else if (strcmp(token, "&") == 0) {
                // Ignorar el símbolo de ampersand solo, ya que no hay comando
                // Sin embargo, esto se debe manejar más adelante
                token = strtok(NULL, " ");
                continue;  // Continuar al siguiente token
            } else {
                args[arg_count++] = token;
                has_command = 1; // Hay un comando antes de la redirección
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
            if (arg_count != 1) {  // Si 'exit' tiene más de un argumento, mostrar error
                showError();
            } else {
                free(line);
                exit(0);
            }
        }

        // Si el comando es 'cd', manejamos el cambio de directorio
        else if (args[0] != NULL && strcmp(args[0], "cd") == 0) {
            if (arg_count != 2) {
                showError();
            } else if (chdir(args[1]) != 0) {
                showError();
            }
        }
        // Si el comando no es 'cd' y hay un comando, lo ejecutamos como un programa externo
        else if (args[0] != NULL) {
            pid_t pid = fork();
            if (pid == 0) {  // Proceso hijo
                // Si hay redirección de salida, cambiar el descriptor de archivo
                if (output_file != NULL) {
                    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) {
                        showError();
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);  // Redirigir también la salida de error
                    close(fd);
                }

                // Intentar ejecutar el comando
                if (execv(args[0], args) == -1) {
                    // Intentar ejecutar con ruta completa (/bin/ls, por ejemplo)
                    char path[100];
                    snprintf(path, sizeof(path), "/bin/%s", args[0]);
                    execv(path, args);
                    // Si falla, mostrar el mensaje de error y salir
                    showError();
                    exit(1);
                }
            } else if (pid > 0) {  // Proceso padre
                // Esperar a que el hijo termine
                wait(NULL);
            } else {
                // Error al hacer fork
                showError();
            }
        }

        free(line);  // Liberar la memoria después de cada línea
    }

    return 0;
}
