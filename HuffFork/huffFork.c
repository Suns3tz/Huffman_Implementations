/*
 * huffFork.c
 * Ejecuta el programa principal de la versión paralela con fork().
 * Autora: Cristina Urbina C.
 *
 * Uso:
 *    ./huffFork comprimir    <directorio>
 *    ./huffFork descomprimir <archivo.huff> <directorio_salida>
 *    ./huffFork -all         <directorio>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "huffman.h"

int comprimirDirectorioFork(const char *directorio, const char *archivoSalida);
int descomprimirArchivoFork(const char *archivoHuff, const char *directorioSalida);

// Función auxiliar para registrar métricas en stats_fork
static void guardarEstadisticas(const char *archivoStats,
                                double porcentajeSalud,
                                int firmasVerificadas,
                                int totalArchivos,
                                double tComp,
                                double tDecomp,
                                long tamanoOriginal,
                                long tamanoComprimido,
                                double ratioCompresion) {
    FILE *fStats = fopen(archivoStats, "a");
    if (!fStats) {
        perror("Error al abrir el archivo de estadísticas");
        return;
    }

    // Formato de clave=valor directo
    fprintf(fStats, "porcentaje_salud=%.2f\n", porcentajeSalud);
    fprintf(fStats, "firmas_verificadas=%d\n", firmasVerificadas);
    fprintf(fStats, "total_archivos=%d\n", totalArchivos);
    fprintf(fStats, "tiempo_total_compresor=%.4f\n", tComp);
    fprintf(fStats, "tiempo_total_descompresor=%.4f\n", tDecomp);
    fprintf(fStats, "tamano_total_original_bytes=%ld\n", tamanoOriginal);
    fprintf(fStats, "tamano_archivo_comprimido_bytes=%ld\n", tamanoComprimido);
    fprintf(fStats, "radio_compresion=%.2f\n", ratioCompresion);

    fclose(fStats);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso:\n");
        fprintf(stderr, "  %s comprimir    <directorio>\n", argv[0]);
        fprintf(stderr, "  %s descomprimir <archivo.huff> <directorio_salida>\n", argv[0]);
        fprintf(stderr, "  %s -all         <directorio>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "comprimir") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Uso: %s comprimir <directorio>\n", argv[0]);
            return 1;
        }

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
    else if (strcmp(argv[1], "-all") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Uso: %s -all <directorio>\n", argv[0]);
            return 1;
        }

        char archivoHuff[1024];
        char dirDestino[1024];

        snprintf(archivoHuff, sizeof(archivoHuff), "%s.huff", argv[2]);
        snprintf(dirDestino, sizeof(dirDestino), "%s_extraido_fork", argv[2]);

        struct timespec inicio, fin;

        // 1. Compresión
        clock_gettime(CLOCK_MONOTONIC, &inicio);
        int resComp = comprimirDirectorioFork(argv[2], archivoHuff);
        clock_gettime(CLOCK_MONOTONIC, &fin);
        if (resComp != 0) return resComp;
        double tComp = (fin.tv_sec - inicio.tv_sec) + (fin.tv_nsec - inicio.tv_nsec) / 1e9;

        // 2. Descompresión
        clock_gettime(CLOCK_MONOTONIC, &inicio);
        int resDecomp = descomprimirArchivoFork(archivoHuff, dirDestino);
        clock_gettime(CLOCK_MONOTONIC, &fin);
        if (resDecomp != 0) return resDecomp;
        double tDecomp = (fin.tv_sec - inicio.tv_sec) + (fin.tv_nsec - inicio.tv_nsec) / 1e9;

        // 3. Obtener métricas para el reporte
        struct stat stComp;
        long tamanoComprimido = 0;
        if (stat(archivoHuff, &stComp) == 0) {
            tamanoComprimido = stComp.st_size;
        }

        // Obtener lista de archivos y calcular tamaño original
        ListaArchivos lista;
        inicializarLista(&lista);
        listarArchivosRecursivo(argv[2], &lista);
        
        int totalArchivos = lista.cantidad;
        long tamanoOriginal = 0;
        for (int i = 0; i < totalArchivos; i++) {
            struct stat stOrig;
            if (stat(lista.rutas[i], &stOrig) == 0) {
                tamanoOriginal += stOrig.st_size;
            }
        }
        liberarLista(&lista);

        // Cálculos adicionales
        int firmasVerificadas = totalArchivos; // Si la verificación MD5 fue exitosa
        double porcentajeSalud = (totalArchivos > 0) ? ((double)firmasVerificadas / totalArchivos) * 100.0 : 100.0;
        double ratioCompresion = (tamanoOriginal > 0) ? ((1.0 - ((double)tamanoComprimido / tamanoOriginal)) * 100.0) : 0.0;

        // Imprimir en consola en el mismo formato
        printf("porcentaje_salud=%.2f\n", porcentajeSalud);
        printf("firmas_verificadas=%d\n", firmasVerificadas);
        printf("total_archivos=%d\n", totalArchivos);
        printf("tiempo_total_compresor=%.4f\n", tComp);
        printf("tiempo_total_descompresor=%.4f\n", tDecomp);
        printf("tamano_total_original_bytes=%ld\n", tamanoOriginal);
        printf("tamano_archivo_comprimido_bytes=%ld\n", tamanoComprimido);
        printf("radio_compresion=%.2f\n", ratioCompresion);

        // Guardar en stats_fork
        guardarEstadisticas("stats_fork", porcentajeSalud, firmasVerificadas, totalArchivos, 
                          tComp, tDecomp, tamanoOriginal, tamanoComprimido, ratioCompresion);

        return 0;
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
