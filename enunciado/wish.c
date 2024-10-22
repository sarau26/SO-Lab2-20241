#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

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

        // Manejar el comando 'exit'
        if (strcmp(line, "exit") == 0) {
            free(line);
            exit(0);
        }

        // Parsear la línea en tokens (argumentos)
        char *args[MAX_ARGS];
        int arg_count = 0;
        char *token = strtok(line, " ");
        while (token != NULL && arg_count < MAX_ARGS - 1) {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        // Si el comando es 'cd', manejamos el cambio de directorio
        if (strcmp(args[0], "cd") == 0) {
            if (arg_count != 2) {
                showError();
            } else if (chdir(args[1]) != 0) {
                showError();
            }
        }
        // Si el comando no es 'cd', lo ejecutamos como un programa externo
        else {
            pid_t pid = fork();
            if (pid == 0) {  // Proceso hijo
                // Intentar ejecutar el comando
                if (execv(args[0], args) == -1) {
                    // Intentar ejecutar con ruta completa (/bin/ls, por ejemplo)
                    char path[100];
                    snprintf(path, sizeof(path), "/bin/%s", args[0]);
                    execv(path, args);
                    // Si falla, mostrar el mensaje de error y salir
                    perror(args[0]);
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
