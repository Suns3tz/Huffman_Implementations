/*
 * huffForkDecomp.c
 * Descomprime un archivo .huff en paralelo con fork().
 * Autora: Cristina Urbina C.
 *
 * Estrategia:
 *   1. Lee el header del .huff y reconstruye el árbol.
 *   2. Calcula el offset de cada archivo usando los tamaños
 *      comprimidos guardados en la metadata.
 *   3. Por cada archivo en el header, crea un fork.
 *      Cada hijo:
 *        - Se posiciona en su offset dentro del .huff.
 *        - Lee exactamente 'tamanoComprimido' bytes.
 *        - Los descomprime usando el árbol heredado por fork.
 *        - Escribe el archivo resultante.
 *        - Calcula su MD5 y lo compara con el guardado.
 *        - Escribe el resultado (OK/FALLO) en un pipe al padre.
 *   4. Recolecta los resultados y muestra estadísticas.
 *   Reporte: escribe results/descompresor.txt con las estadísticas.
 */

#include "huffman.h"

extern char *HuffmanCodesArray[256];

/* Declara la metadata de cada archivo dentro del .huff */
typedef struct {
    char nombre[512];
    uint64_t tamanoOriginal;
    uint64_t tamanoComprimido;
    unsigned char md5[16];
} MetadataArchivo;

/* Devuelve el tiempo monotónico actual en segundos */
static double tiempoActual(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Escribe results/descompresor.txt con las estadísticas del descompresor */
static void escribirEstadisticasDescompresor(double tiempoTotal, int firmasVerificadas, uint32_t totalArchivos, double porcentajeSalud) {
    mkdir("results", 0755);
    FILE *f = fopen("results/descompresor.txt", "w");
    if (!f) {
        perror("fopen results/descompresor.txt");
        return;
    }

    fprintf(f, "porcentaje_salud=%.2f\n", porcentajeSalud);
    fprintf(f, "firmas_verificadas=%d\n", firmasVerificadas);
    fprintf(f, "total_archivos=%u\n", totalArchivos);
    fprintf(f, "tiempo_total_descompresor=%.4f\n", tiempoTotal);

    fclose(f);
}

int descomprimirArchivoFork(const char *archivoHuff, const char *directorioSalida) {
    double tInicio = tiempoActual();

    printf("=====================================================================\n");
    printf("    DESCOMPRESOR HUFFMAN PARALELO (FORK) - Cristina Urbina C.        \n");
    printf("=====================================================================\n");

    FILE *in = fopen(archivoHuff, "rb");
    if (!in) {
        perror("fopen .huff");
        return 1;
    }

    /* Lee el modo */
    uint8_t modo = leerModo(in);
    if (modo != MODO_FORK) {
        fprintf(stderr, "Advertencia: el archivo no fue comprimido con fork (modo=%u)\n", modo);
    }

    /* Lee la tabla de frecuencias y reconstruye el árbol */
    int frecuencias[256];
    leerFrecuencias(in, frecuencias);
    MinHeapNode *root = construirArbolHuffman(frecuencias);
    if (!root) {
        fprintf(stderr, "Error reconstruyendo árbol\n");
        fclose(in);
        return 1;
    }

    uint32_t cantidad = leerCantidadArchivos(in);

    /* Lee la metadata completa e incluye tamaño comprimido */
    MetadataArchivo *metas = (MetadataArchivo*)malloc(sizeof(MetadataArchivo) * cantidad);
    for (uint32_t i = 0; i < cantidad; i++) {
        leerMetadataArchivo(in, metas[i].nombre, &metas[i].tamanoOriginal, &metas[i].tamanoComprimido, metas[i].md5);
    }

    /* Guarda la posición donde inician los datos comprimidos */
    long offsetDatos = ftell(in);
    fclose(in);

    /* Calcula el offset de cada archivo dentro del .huff */
    uint64_t *offsets = (uint64_t*)malloc(sizeof(uint64_t) * cantidad);
    uint64_t acumulado = 0;
    for (uint32_t i = 0; i < cantidad; i++) {
        offsets[i] = acumulado;
        acumulado += metas[i].tamanoComprimido;
    }

    mkdir(directorioSalida, 0755);

    /* Procesa cada archivo con fork */
    int verificados = 0;
    for (uint32_t i = 0; i < cantidad; i++) {
        int fd[2];
        if (pipe(fd) == -1) {
            perror("pipe");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(fd[0]); close(fd[1]);
            break;
        }

        if (pid == 0) {
            /* ---- HIJO ---- */
            close(fd[0]);

            FILE *huff = fopen(archivoHuff, "rb");
            if (!huff) _exit(1);

            /* posiciona en el offset exacto de este archivo */
            fseek(huff, offsetDatos + offsets[i], SEEK_SET);

            char rutaSalida[1024];
            snprintf(rutaSalida, sizeof(rutaSalida), "%s/%s",
                     directorioSalida, metas[i].nombre);

            FILE *out = fopen(rutaSalida, "wb");
            if (!out) {
                fclose(huff);
                _exit(1);
            }

            descomprimirArchivo(huff, out, metas[i].tamanoOriginal, root);
            fclose(out);
            fclose(huff);

            /* Verifica el MD5 */
            unsigned char md5Calc[16];
            calcularMD5(rutaSalida, md5Calc);
            int ok = (memcmp(md5Calc, metas[i].md5, 16) == 0);

            ssize_t escrito = write(fd[1], &ok, sizeof(int));
            (void)escrito;
            close(fd[1]);
            _exit(0);
        }
        else {
            /* ---- PADRE ---- */
            close(fd[1]);
            int ok = 0;
            ssize_t leido = read(fd[0], &ok, sizeof(int));
            (void)leido;
            close(fd[0]);
            waitpid(pid, NULL, 0);

            if (ok) {
                printf("[OK]    %s\n", metas[i].nombre);
                verificados++;
            } else {
                printf("[FALLO] %s\n", metas[i].nombre);
            }
        }
    }

    double tFin = tiempoActual();
    double tiempoTotal = tFin - tInicio;

    double porcentajeSalud = 0.0;
    if (cantidad > 0) {
        porcentajeSalud = 100.0 * verificados / cantidad;
    }

    printf("\nArchivos verificados: %d / %u\n", verificados, cantidad);
    printf("Porcentaje de salud: %.2f%%\n", porcentajeSalud);
    printf("TIEMPO_DESCOMPRESOR=%.6f\n", tiempoTotal);

    /* Escribe el reporte de estadísticas */
    escribirEstadisticasDescompresor(tiempoTotal, verificados, cantidad, porcentajeSalud);

    free(offsets);
    free(metas);
    liberarArbol(root);
    limpiarTablaCodigos();
    return 0;
}