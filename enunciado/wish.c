#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    FILE *input = stdin;  // Por defecto, la entrada es stdin (modo interactivo)
    
    // Si hay más de un argumento, es un error
    if (argc > 2) {
        fprintf(stderr, "wish: too many arguments\n");
        exit(1);
    }
    
    // Si hay un argumento, es el nombre de un archivo (modo batch)
    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            fprintf(stderr, "wish: cannot open file\n");
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

        // Aquí procesarías los otros comandos y ejecutables externos
        
        free(line);  // Liberar la memoria después de cada línea
    }

    return 0;
}
