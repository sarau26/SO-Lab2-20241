#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    while (1) {
        // Mostrar el prompt
        printf("wish> ");
        fflush(stdout);

        // Leer línea de entrada
        char *line = NULL;
        size_t len = 0;
        getline(&line, &len, stdin);

        // Quitar el salto de línea al final de la entrada
        line[strcspn(line, "\n")] = '\0';

        // Parsear la línea de comandos para detectar 'exit'
        if (strcmp(line, "exit") == 0) {
            free(line); // Liberar memoria antes de salir
            exit(0);    // Terminar el programa
        }

        printf("Comando ingresado: %s\n", line);

        free(line); // Liberar memoria al final de cada ciclo
    }

    return 0;
}
