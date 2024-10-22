#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Función para mostrar un error genérico
void showError() {
    const char *error_message = "An error has occurred\n";
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

        // Parsear la línea en tokens
        char *token = strtok(line, " ");
        
        // Manejar el comando 'exit'
        if (token != NULL && strcmp(token, "exit") == 0) {
            if (strtok(NULL, " ") != NULL) {  // 'exit' no debe tener argumentos
                showError();
            } else {
                free(line);
                exit(0);  // Salir si 'exit' se usa sin argumentos
            }
        }

        // Manejar el comando 'cd'
        else if (token != NULL && strcmp(token, "cd") == 0) {
            char *dir = strtok(NULL, " ");
            if (dir == NULL || strtok(NULL, " ") != NULL) {  // 'cd' requiere exactamente un argumento
                showError();
            } else if (chdir(dir) != 0) {  // Intentar cambiar de directorio
                showError();
            }
        }

        // Si es otro comando, aquí iría la lógica de ejecutables externos
        
        free(line);  // Liberar la memoria después de cada línea
    }

    return 0;
}
