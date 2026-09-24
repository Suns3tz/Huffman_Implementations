/*
 * huffFork.c
 * Ejecuta el programa principal de la versión paralela con fork().
 * Autora: Cristina Urbina C.
 *
 * Uso:
 *   ./huffFork comprimir    <directorio>
 *   ./huffFork descomprimir <archivo.huff> <directorio_salida>
 */

#include "huffman.h"

int comprimirDirectorioFork(const char *directorio, const char *archivoSalida);
int descomprimirArchivoFork(const char *archivoHuff, const char *directorioSalida);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso:\n");
        fprintf(stderr, "  %s comprimir    <directorio>\n", argv[0]);
        fprintf(stderr, "  %s descomprimir <archivo.huff> <directorio_salida>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "comprimir") == 0) {

        if (argc != 3) {
            fprintf(stderr, "Uso: %s comprimir <directorio>\n", argv[0]);
            return 1;
        }

        /* Construye el nombre del .huff de salida a partir del directorio */
        char archivoSalida[1024];
        snprintf(archivoSalida, sizeof(archivoSalida), "%s.huff", argv[2]);
        return comprimirDirectorioFork(argv[2], archivoSalida);
    }
    else if (strcmp(argv[1], "descomprimir") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Uso: %s descomprimir <archivo.huff> <directorio_salida>\n", argv[0]);
            return 1;
        }
        return descomprimirArchivoFork(argv[2], argv[3]);
    }
    else {
        fprintf(stderr, "Modo desconocido: %s\n", argv[1]);
        return 1;
    }
}

/* ---------- Implementaciones auxiliares compartidas ---------- */

void inicializarLista(ListaArchivos *lista) {
    lista->cantidad = 0;
    lista->capacidad = 16;
    lista->rutas = (char**)malloc(sizeof(char*) * lista->capacidad);
}

void agregarRuta(ListaArchivos *lista, const char *ruta) {
    if (lista->cantidad >= lista->capacidad) {
        lista->capacidad *= 2;
        lista->rutas = (char**)realloc(lista->rutas, sizeof(char*) * lista->capacidad);
    }
    
    lista->rutas[lista->cantidad] = strdup(ruta);
    lista->cantidad++;
}

void liberarLista(ListaArchivos *lista) {
    for (int i = 0; i < lista->cantidad; i++) {
        free(lista->rutas[i]);
    }
    free(lista->rutas);
}

void listarArchivosRecursivo(const char *path, ListaArchivos *lista) {
    struct stat st;
    if (stat(path, &st) != 0) return;

    if (S_ISREG(st.st_mode)) {
        
        /* Ignora archivos .huff para no recomprimir */
        if (strstr(path, ".huff") == NULL) {
            agregarRuta(lista, path);
        }
    }
    else if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (dir == NULL) return;

        struct dirent *entry;
        char subPath[1024];

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            snprintf(subPath, sizeof(subPath), "%s/%s", path, entry->d_name);
            listarArchivosRecursivo(subPath, lista);
        }

        closedir(dir);
    }
}

void contarFrecuenciasArchivo(const char *ruta, int frecuencias[256]) {
    FILE *f = fopen(ruta, "rb");
    if (!f) return;

    unsigned char buffer[4096];
    size_t leidos;

    while ((leidos = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        for (size_t i = 0; i < leidos; i++) {
            frecuencias[buffer[i]]++;
        }
    }

    fclose(f);
}

void calcularMD5(const char *rutaArchivo, unsigned char salida[16]) {

    FILE *archivo = fopen(rutaArchivo, "rb");

    if (!archivo) {
        memset(salida, 0, 16);
        return;
    }

    MD5_CTX ctx;
    unsigned char buffer[4096];
    size_t bytes;
    MD5_Init(&ctx);

    while ((bytes = fread(buffer, 1, sizeof(buffer), archivo)) > 0) {
        MD5_Update(&ctx, buffer, bytes);
    }

    MD5_Final(salida, &ctx);
    fclose(archivo);
}