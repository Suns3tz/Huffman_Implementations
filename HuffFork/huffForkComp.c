/*
 * huffForkComp.c
 * Comprime un directorio en paralelo con fork() + pipes + archivos temporales.
 * Autora: Cristina Urbina C.
 *
 * Estrategia:
 *   Fase 1: crea un pipe por cada archivo.
 *           Cada hijo calcula la tabla de frecuencias de su archivo
 *           y la envía al padre por el pipe.
 *           Suma todas las frecuencias en el padre.
 *   Fase 2: construye el árbol de Huffman global.
 *           Crea un fork por archivo.
 *           Cada hijo comprime su archivo a un archivo temporal
 *           (tmp_<pid>_<indice>.bin) usando el árbol global heredado
 *           por fork (no necesita IPC para el árbol).
 *           Espera (waitpid) a cada hijo, obtiene el tamaño comprimido
 *           de cada temporal con stat y escribe la metadata completa
 *           (incluyendo el tamaño comprimido) en el .huff final.
 *           Finalmente concatena los temporales en el .huff.
 *   Reporte: escribe results/compresor.txt con las estadísticas.
 */

#include "huffman.h"

extern char *HuffmanCodesArray[256];

/* Devuelve el tiempo monotónico actual en segundos */
static double tiempoActual(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Escribe results/compresor.txt con las estadísticas del compresor */
static void escribirEstadisticasCompresor(double tiempoTotal, uint64_t tamanoTotalOriginal, uint64_t tamanoComprimido, double radioCompresion) {
    mkdir("results", 0755);
    FILE *f = fopen("results/compresor.txt", "w");
    if (!f) {
        perror("fopen results/compresor.txt");
        return;
    }

    fprintf(f, "tiempo_total_compresor=%.4f\n", tiempoTotal);
    fprintf(f, "tamano_total_original_bytes=%lu\n", (unsigned long)tamanoTotalOriginal);
    fprintf(f, "tamano_archivo_comprimido_bytes=%lu\n", (unsigned long)tamanoComprimido);
    fprintf(f, "radio_compresion=%.2f\n", radioCompresion);

    fclose(f);
}

/* ---------- Fase 1: cuenta frecuencias con fork + pipe ---------- */
static int contarFrecuenciasGlobal(ListaArchivos *lista, int frecuencias[256]) {
    memset(frecuencias, 0, sizeof(int) * 256);

    for (int i = 0; i < lista->cantidad; i++) {
        int fd[2];
        if (pipe(fd) == -1) {
            perror("pipe");
            return -1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(fd[0]); close(fd[1]);
            return -1;
        }

        if (pid == 0) {
            /* ---- HIJO ---- */
            close(fd[0]); /* Cierra el extremo de lectura */
            int frecLocal[256] = {0};
            contarFrecuenciasArchivo(lista->rutas[i], frecLocal);
            ssize_t escrito = write(fd[1], frecLocal, sizeof(int) * 256);
            (void)escrito;
            close(fd[1]);
            _exit(0);
        }
        else {
            /* ---- PADRE ---- */
            close(fd[1]); /* Cierra el extremo de escritura */
            int frecHijo[256];
            ssize_t total = 0;
            ssize_t leidos;
            while (total < (ssize_t)sizeof(frecHijo)) {
                leidos = read(fd[0], ((char*)frecHijo) + total, sizeof(frecHijo) - total);
                if (leidos <= 0) break;
                total += leidos;
            }
            close(fd[0]);
            waitpid(pid, NULL, 0);

            for (int j = 0; j < 256; j++) {
                frecuencias[j] += frecHijo[j];
            }
        }
    }
    return 0;
}

/* ---------- Fase 2: comprime con fork a temporales ---------- */
static int comprimirArchivosConFork(ListaArchivos *lista, const char *directorioSalidaTmp, char ***temporalesSalida, int *numTemporales) {
    char **temporales = (char**)malloc(sizeof(char*) * lista->cantidad);

    for (int i = 0; i < lista->cantidad; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return -1;
        }

        if (pid == 0) {
            /* ---- HIJO ---- */
            char rutaTmp[1024];
            snprintf(rutaTmp, sizeof(rutaTmp), "%s/tmp_%d_%d.bin",
                     directorioSalidaTmp, getpid(), i);

            FILE *in = fopen(lista->rutas[i], "rb");
            FILE *out = fopen(rutaTmp, "wb");
            if (in && out) {
                comprimirArchivo(in, out);
                fclose(in);
                fclose(out);
                _exit(0);
            }
            _exit(1);
        }
        else {
            /* ---- PADRE ---- */
            char rutaTmp[1024];
            snprintf(rutaTmp, sizeof(rutaTmp), "%s/tmp_%d_%d.bin",
                     directorioSalidaTmp, pid, i);
            temporales[i] = strdup(rutaTmp);
        }
    }

    /* Espera a todos los hijos */
    for (int i = 0; i < lista->cantidad; i++) {
        wait(NULL);
    }

    *temporalesSalida = temporales;
    *numTemporales = lista->cantidad;
    return 0;
}

/* ---------- Función principal de compresión ---------- */
int comprimirDirectorioFork(const char *directorio, const char *archivoSalida) {
    double tInicio = tiempoActual();

    printf("=====================================================================\n");
    printf("     COMPRESOR HUFFMAN PARALELO (FORK) - Cristina Urbina C.          \n");
    printf("=====================================================================\n");

    /* Lista los archivos del directorio */
    ListaArchivos lista;
    inicializarLista(&lista);
    listarArchivosRecursivo(directorio, &lista);

    if (lista.cantidad == 0) {
        fprintf(stderr, "No se encontraron archivos en: %s\n", directorio);
        liberarLista(&lista);
        return 1;
    }
    printf("[1/5] Archivos encontrados: %d\n", lista.cantidad);

    /* Cuenta las frecuencias con fork + pipe */
    int frecuencias[256];
    if (contarFrecuenciasGlobal(&lista, frecuencias) != 0) {
        liberarLista(&lista);
        return 1;
    }
    printf("[2/5] Frecuencias globales calculadas con fork + pipe\n");

    /* Construye el árbol y la tabla de códigos */
    MinHeapNode *root = construirArbolHuffman(frecuencias);
    if (root == NULL) {
        fprintf(stderr, "Error construyendo árbol de Huffman\n");
        liberarLista(&lista);
        return 1;
    }
    int arr[256], top = 0;
    generarTablaCodigos(root, arr, top);
    printf("[3/5] Árbol de Huffman construido\n");

    /* Crea el directorio temporal */
    char dirTmp[] = "/tmp/huffForkXXXXXX";
    if (mkdtemp(dirTmp) == NULL) {
        perror("mkdtemp");
        liberarArbol(root);
        liberarLista(&lista);
        return 1;
    }

    char **temporales = NULL;
    int numTemporales = 0;
    if (comprimirArchivosConFork(&lista, dirTmp, &temporales, &numTemporales) != 0) {
        liberarArbol(root);
        liberarLista(&lista);
        return 1;
    }
    printf("[4/5] Archivos comprimidos en paralelo (fork)\n");

    /* Obtén el tamaño comprimido de cada temporal */
    uint64_t *tamanosComprimidos = (uint64_t*)malloc(sizeof(uint64_t) * numTemporales);
    for (int i = 0; i < numTemporales; i++) {
        struct stat st;
        if (stat(temporales[i], &st) == 0) {
            tamanosComprimidos[i] = (uint64_t)st.st_size;
        } else {
            tamanosComprimidos[i] = 0;
        }
    }

    /* Escribe el .huff final */
    FILE *out = fopen(archivoSalida, "wb");
    if (!out) {
        perror("fopen salida");
        free(tamanosComprimidos);
        liberarArbol(root);
        liberarLista(&lista);
        return 1;
    }

    escribirModo(out, MODO_FORK);
    escribirFrecuencias(out, frecuencias);
    escribirCantidadArchivos(out, (uint32_t)lista.cantidad);

    /* Escribe la metadata por archivo (incluye tamaño comprimido)
     * y acumula el tamaño original total */
    uint64_t tamanoTotalOriginal = 0;
    for (int i = 0; i < lista.cantidad; i++) {
        struct stat st;
        stat(lista.rutas[i], &st);
        uint64_t tamanoOriginal = (uint64_t)st.st_size;
        tamanoTotalOriginal += tamanoOriginal;

        unsigned char md5[16];
        calcularMD5(lista.rutas[i], md5);

        /* Usa solo el nombre base */
        const char *nombreBase = strrchr(lista.rutas[i], '/');
        nombreBase = nombreBase ? nombreBase + 1 : lista.rutas[i];

        escribirMetadataArchivo(out, nombreBase, tamanoOriginal, tamanosComprimidos[i], md5);
    }

    /* Concatena los datos comprimidos */
    for (int i = 0; i < numTemporales; i++) {
        FILE *tmp = fopen(temporales[i], "rb");
        if (!tmp) continue;

        unsigned char buffer[4096];
        size_t n;

        while ((n = fread(buffer, 1, sizeof(buffer), tmp)) > 0) {
            fwrite(buffer, 1, n, out);
        }
        fclose(tmp);
        remove(temporales[i]);
        free(temporales[i]);
    }
    free(temporales);
    free(tamanosComprimidos);
    rmdir(dirTmp);

    fclose(out);

    double tFin = tiempoActual();
    double tiempoTotal = tFin - tInicio;

    /* Obtén el tamaño del .huff final */
    struct stat stHuff;
    uint64_t tamanoComprimidoTotal = 0;
    if (stat(archivoSalida, &stHuff) == 0) {
        tamanoComprimidoTotal = (uint64_t)stHuff.st_size;
    }

    /* Calcula el radio de compresión */
    double radioCompresion = 0.0;
    if (tamanoTotalOriginal > 0) {
        radioCompresion = 100.0 * (1.0 - ((double)tamanoComprimidoTotal / (double)tamanoTotalOriginal));
    }

    printf("[5/5] Archivo final escrito: %s\n", archivoSalida);
    printf("TIEMPO_COMPRESOR=%.6f\n", tiempoTotal);

    /* Escribe el reporte de estadísticas */
    escribirEstadisticasCompresor(tiempoTotal, tamanoTotalOriginal, tamanoComprimidoTotal, radioCompresion);

    liberarArbol(root);
    limpiarTablaCodigos();
    liberarLista(&lista);
    return 0;
}