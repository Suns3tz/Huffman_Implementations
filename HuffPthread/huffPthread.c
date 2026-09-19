#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "huffman_pthread.h"

static double obtenerTiempoSegundos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void imprimirFirmaMD5(const unsigned char *md5, char *outStr) {
    for (int i = 0; i < 16; i++) {
        sprintf(&outStr[i * 2], "%02x", md5[i]);
    }
    outStr[32] = '\0';
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Uso: %s <directorio_o_archivo> [num_hilos] [modo: -c | -d | -all]\n", argv[0]);
        printf("  num_hilos: Opcional (por defecto: núcleos del CPU detectados)\n");
        printf("  modo:      -c: Solo comprimir | -d: Solo descomprimir | -all: Ciclo completo (por defecto)\n");
        return 1;
    }

    const char *ruta = argv[1];
    int numHilos = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (numHilos <= 0) numHilos = 4;

    const char *modo = "-all";

    if (argc >= 3) {
        int parsedHilos = atoi(argv[2]);
        if (parsedHilos > 0) {
            numHilos = parsedHilos;
        } else if (argv[2][0] == '-') {
            modo = argv[2];
        }
    }
    if (argc >= 4) {
        modo = argv[3];
    }

    printf("====================================================================================================\n");
    printf("                  COMPRESOR / DESCOMPRESOR HUFFMAN CONCURRENTE (PTHREADS)                          \n");
    printf("====================================================================================================\n");
    printf("Ruta objetivo      : %s\n", ruta);
    printf("Hilos concurrentes : %d\n", numHilos);
    printf("Modo de ejecución  : %s\n", modo);
    printf("====================================================================================================\n\n");

    SharedContext ctx;
    if (inicializarContexto(&ctx, numHilos) != 0) {
        return 1;
    }

    // Escanear ruta y construir cola de trabajo en memoria compartida
    escanearRutaRecursiva(ruta, &ctx);

    if (ctx.taskCount == 0) {
        fprintf(stderr, "Error: No se encontraron archivos válidos en la ruta especificada.\n");
        liberarContexto(&ctx);
        return 1;
    }

    printf("[INFO] Archivos detectados en la cola de trabajo: %d\n\n", ctx.taskCount);

    double tInicioTotal = obtenerTiempoSegundos();
    double tFrecuencias = 0.0, tArbol = 0.0, tCompresion = 0.0, tDescompresion = 0.0;

    int ejecutarCompresion = (strcmp(modo, "-c") == 0 || strcmp(modo, "-all") == 0);
    int ejecutarDescompresion = (strcmp(modo, "-d") == 0 || strcmp(modo, "-all") == 0);

    if (ejecutarCompresion) {
        // Fase 1: Conteo de frecuencias concurrente
        printf("[1/3] Calculando frecuencias concurrentemente con %d hilos...\n", numHilos);
        double t0 = obtenerTiempoSegundos();
        calcularFrecuenciasParalelo(&ctx);
        tFrecuencias = obtenerTiempoSegundos() - t0;
        printf("      -> Conteo finalizado en %.4f s\n\n", tFrecuencias);

        // Fase 2: Construcción del árbol y códigos en RAM (memoria compartida)
        printf("[2/3] Construyendo Árbol de Huffman unificado y tabla de códigos...\n");
        double t1 = obtenerTiempoSegundos();
        ctx.root = buildHuffmanTree(ctx.ASCIIcount);
        if (!ctx.root) {
            fprintf(stderr, "Error: No se pudo generar el árbol de Huffman (datos vacíos).\n");
            liberarContexto(&ctx);
            return 1;
        }
        int bufferRuta[256];
        generarTablaCodigos(ctx.root, bufferRuta, 0, ctx.HuffmanCodesArray);
        tArbol = obtenerTiempoSegundos() - t1;
        printf("      -> Árbol generado en %.6f s\n\n", tArbol);

        // Fase 3: Compresión concurrente
        printf("[3/3] Comprimiendo archivos concurrentemente con %d hilos...\n\n", numHilos);
        printf("----------------------------------------------------------------------------------------------------\n");
        printf("| %-25s | %-32s | %-10s | %-10s | %-7s |\n", "Archivo Original", "MD5 Signature (16 bytes)", "Tam. Orig.", "Tam. Huff", "Ratio");
        printf("----------------------------------------------------------------------------------------------------\n");

        double t2 = obtenerTiempoSegundos();
        comprimirParalelo(&ctx);
        tCompresion = obtenerTiempoSegundos() - t2;

        for (int i = 0; i < ctx.taskCount; i++) {
            FileTask *t = &ctx.tasks[i];
            if (t->isHuff) continue;

            double ratio = 0.0;
            if (t->originalSize > 0) {
                ratio = 100.0 * (1.0 - ((double)t->compressedSize / t->originalSize));
            }

            char md5Str[33];
            imprimirFirmaMD5(t->md5Original, md5Str);

            // Mostrar solo nombre base si la ruta es muy larga
            const char *displayPath = t->originalPath;
            if (strlen(displayPath) > 25) {
                displayPath = t->originalPath + strlen(t->originalPath) - 25;
            }

            printf("| %-25s | %-32s | %10ld | %10ld | %6.2f%% |\n",
                   displayPath, md5Str, t->originalSize, t->compressedSize, ratio);
        }
        printf("----------------------------------------------------------------------------------------------------\n");
        printf("      -> Compresión finalizada en %.4f s\n\n", tCompresion);
    }

    if (ejecutarDescompresion) {
        // Si no se ejecutó compresión previamente en este proceso pero se requiere descompresión,
        // necesitamos el árbol de Huffman (generado a partir de los archivos de origen si están disponibles)
        if (!ctx.root) {
            printf("[INFO] Generando árbol de Huffman previo a la descompresión...\n");
            calcularFrecuenciasParalelo(&ctx);
            ctx.root = buildHuffmanTree(ctx.ASCIIcount);
            int bufferRuta[256];
            generarTablaCodigos(ctx.root, bufferRuta, 0, ctx.HuffmanCodesArray);
        }

        printf("[Fase Descompresión] Descomprimiendo y verificando integridad MD5 con %d hilos...\n", numHilos);
        double t3 = obtenerTiempoSegundos();
        descomprimirParalelo(&ctx);
        tDescompresion = obtenerTiempoSegundos() - t3;

        printf("----------------------------------------------------------------------------------------------------\n");
        printf("| %-25s | %-16s | %-16s | %-10s |\n", "Archivo Restaurado", "MD5 Guardado", "MD5 Recalculado", "Estado");
        printf("----------------------------------------------------------------------------------------------------\n");

        for (int i = 0; i < ctx.taskCount; i++) {
            FileTask *t = &ctx.tasks[i];
            if (!t->isHuff && t->compressedSize == 0) continue;

            char md5OrigStr[33], md5RestStr[33];
            imprimirFirmaMD5(t->md5Original, md5OrigStr);
            imprimirFirmaMD5(t->md5Restored, md5RestStr);

            md5OrigStr[16] = '\0'; // Truncar para formato compacto de tabla
            md5RestStr[16] = '\0';

            const char *displayPath = t->restoredPath;
            if (strlen(displayPath) > 25) {
                displayPath = t->restoredPath + strlen(t->restoredPath) - 25;
            }

            printf("| %-25s | %-16s | %-16s | %-10s |\n",
                   displayPath, md5OrigStr, md5RestStr,
                   (t->md5Verified ? "[OK] Válido" : "[FAIL] Corrupto"));
        }
        printf("----------------------------------------------------------------------------------------------------\n");
        printf("      -> Descompresión y verificación finalizada en %.4f s\n\n", tDescompresion);
    }

    double tTotal = obtenerTiempoSegundos() - tInicioTotal;

    // Resumen de Métricas
    printf("====================================================================================================\n");
    printf("                                   RESUMEN DE RENDIMIENTO                                           \n");
    printf("====================================================================================================\n");
    if (ejecutarCompresion) {
        double ratioGlobal = 0.0;
        if (ctx.totalOriginalBytes > 0) {
            ratioGlobal = 100.0 * (1.0 - ((double)ctx.totalCompressedBytes / ctx.totalOriginalBytes));
        }
        printf("Bytes originales totales   : %ld bytes\n", ctx.totalOriginalBytes);
        printf("Bytes comprimidos totales  : %ld bytes\n", ctx.totalCompressedBytes);
        printf("Ratio de compresión global : %.2f%%\n", ratioGlobal);
    }
    if (ejecutarDescompresion) {
        printf("Archivos verificados MD5   : %d / %d (%.2f%%)\n",
               ctx.totalVerifiedFiles, ctx.totalFilesProcessed ? ctx.totalFilesProcessed : ctx.taskCount,
               (ctx.taskCount > 0) ? (100.0 * ctx.totalVerifiedFiles / (ctx.totalFilesProcessed ? ctx.totalFilesProcessed : ctx.taskCount)) : 0.0);
    }
    printf("----------------------------------------------------------------------------------------------------\n");
    printf("Tiempo Conteo Frecuencias  : %.4f s\n", tFrecuencias);
    printf("Tiempo Generación de Árbol : %.6f s\n", tArbol);
    printf("Tiempo Compresión          : %.4f s\n", tCompresion);
    printf("Tiempo Descompresión       : %.4f s\n", tDescompresion);
    printf("TIEMPO TOTAL TRANSCURRIDO  : %.4f s\n", tTotal);
    printf("====================================================================================================\n");

    // Limpieza de memoria dinámica
    limpiarTablaCodigos(ctx.HuffmanCodesArray);
    liberarArbol(ctx.root);
    liberarContexto(&ctx);

    return 0;
}

