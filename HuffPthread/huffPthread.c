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

static int guardarEstadisticas(const char *rutaArchivo, SharedContext *ctx,
                               int ejecutarCompresion, int ejecutarDescompresion,
                               double tCompresor, double tDescompresor, double tTotal) {
    FILE *f = fopen(rutaArchivo, "w");
    if (!f) {
        perror("Error al crear archivo de estadísticas");
        return -1;
    }

    // 1. Porcentaje de salud de la compresión (firmas verificadas / cantidad de archivos)
    if (ejecutarDescompresion) {
        double salud = (ctx->taskCount > 0) ? (100.0 * (double)ctx->totalVerifiedFiles / ctx->taskCount) : 0.0;
        fprintf(f, "porcentaje_salud=%.2f\n", salud);
        fprintf(f, "firmas_verificadas=%d\n", ctx->totalVerifiedFiles);
    } else {
        fprintf(f, "porcentaje_salud=N/A\n");
        fprintf(f, "firmas_verificadas=0\n");
    }
    fprintf(f, "total_archivos=%d\n", ctx->taskCount);

    // 2. Tiempo total de corrida del compresor
    if (ejecutarCompresion) {
        fprintf(f, "tiempo_total_compresor=%.4f\n", tCompresor);
    } else {
        fprintf(f, "tiempo_total_compresor=N/A\n");
    }

    // 3. Tiempo total de corrida del descompresor
    if (ejecutarDescompresion) {
        fprintf(f, "tiempo_total_descompresor=%.4f\n", tDescompresor);
    } else {
        fprintf(f, "tiempo_total_descompresor=N/A\n");
    }

    // 4. Tamaños y Radio de compresión
    uint64_t tamComprimidoFinal = (ctx->archiveFileSize > 0) ? ctx->archiveFileSize : ctx->totalCompressedBytes;
    fprintf(f, "tamano_total_original_bytes=%lu\n", ctx->totalOriginalBytes);
    fprintf(f, "tamano_archivo_comprimido_bytes=%lu\n", tamComprimidoFinal);

    if (ctx->totalOriginalBytes > 0) {
        double ratioAhorro = 100.0 * (1.0 - ((double)tamComprimidoFinal / ctx->totalOriginalBytes));
        fprintf(f, "radio_compresion=%.2f\n", ratioAhorro);
    } else {
        fprintf(f, "radio_compresion=0.00\n");
    }

    fprintf(f, "tiempo_total_ejecucion=%.4f\n", tTotal);

    fclose(f);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Uso: %s <directorio_o_archivo.huff> [num_hilos] [modo: -c | -d | -all] [-k | --keep] [-s <stats_file>]\n", argv[0]);
        printf("  num_hilos:  Opcional (por defecto: núcleos del CPU detectados)\n");
        printf("  modo:       -c: Comprimir a un único archivo .huff\n");
        printf("              -d: Descomprimir desde el archivo .huff\n");
        printf("              -all: Ciclo completo (comprime a .huff, extrae y valida MD5)\n");
        printf("  -k, --keep: Opcional: Conservar archivos originales y el .huff\n");
        printf("  -s, --stats: Opcional: Ruta del archivo para guardar estadísticas (por defecto: estadisticas.txt)\n");
        return 1;
    }

    const char *ruta = argv[1];
    int numHilos = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (numHilos <= 0) numHilos = 4;

    const char *modo = NULL;
    int keepFiles = 0;
    const char *statsFile = "estadisticas.txt";

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--keep") == 0) {
            keepFiles = 1;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "-all") == 0) {
            modo = argv[i];
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stats") == 0) && i + 1 < argc) {
            statsFile = argv[++i];
        } else {
            int parsed = atoi(argv[i]);
            if (parsed > 0) {
                numHilos = parsed;
            }
        }
    }

    // Auto-detectar modo si no se especificó: si la ruta termina en .huff, por defecto es -d, sino -all
    if (modo == NULL) {
        size_t rLen = strlen(ruta);
        if (rLen >= 5 && strcmp(ruta + rLen - 5, ".huff") == 0) {
            modo = "-d";
        } else {
            modo = "-all";
        }
    }

    printf("====================================================================================================\n");
    printf("        COMPRESOR / DESCOMPRESOR HUFFMAN CONCURRENTE EN ARCHIVO ÚNICO (PTHREADS)                   \n");
    printf("====================================================================================================\n");
    printf("Ruta objetivo      : %s\n", ruta);
    printf("Hilos concurrentes : %d\n", numHilos);
    printf("Modo de ejecución  : %s\n", modo);
    printf("Conservar residuos : %s\n", keepFiles ? "SÍ (-k activo)" : "NO (eliminación limpia automática)");
    printf("====================================================================================================\n\n");

    SharedContext ctx;
    if (inicializarContexto(&ctx, numHilos, keepFiles, ruta) != 0) {
        return 1;
    }

    double tInicioTotal = obtenerTiempoSegundos();
    double tFrecuencias = 0.0, tArbol = 0.0, tCompresion = 0.0, tEmpaquetado = 0.0, tDescompresion = 0.0;
    double tLecturaCatalogo = 0.0;

    int ejecutarCompresion = (strcmp(modo, "-c") == 0 || strcmp(modo, "-all") == 0);
    int ejecutarDescompresion = (strcmp(modo, "-d") == 0 || strcmp(modo, "-all") == 0);

    if (ejecutarCompresion) {
        // Escanear ruta y construir catálogo
        escanearRutaRecursiva(ruta, &ctx, ctx.basePath);
        if (ctx.taskCount == 0) {
            fprintf(stderr, "Error: No se encontraron archivos para comprimir en '%s'.\n", ruta);
            liberarContexto(&ctx);
            return 1;
        }
        printf("[INFO] Archivos detectados para empaquetar: %d\n", ctx.taskCount);
        printf("[INFO] Archivo de salida unificado: '%s'\n\n", ctx.archivePath);

        // Fase 1: Conteo de frecuencias concurrente
        printf("[1/3] Calculando frecuencias concurrentemente con %d hilos...\n", numHilos);
        double t0 = obtenerTiempoSegundos();
        calcularFrecuenciasParalelo(&ctx);
        tFrecuencias = obtenerTiempoSegundos() - t0;
        printf("      -> Conteo finalizado en %.4f s\n\n", tFrecuencias);

        // Fase 2: Construcción del árbol y códigos en RAM
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

        // Fase 3: Compresión concurrente en RAM
        printf("[3/3] Comprimiendo archivos concurrentemente en memoria RAM con %d hilos...\n\n", numHilos);
        printf("----------------------------------------------------------------------------------------------------\n");
        printf("| %-25s | %-32s | %-10s | %-10s | %-7s |\n", "Archivo Original", "MD5 Signature (16 bytes)", "Tam. Orig.", "Tam. Huff", "Ratio");
        printf("----------------------------------------------------------------------------------------------------\n");

        double t2 = obtenerTiempoSegundos();
        comprimirParaleloABuffers(&ctx);
        tCompresion = obtenerTiempoSegundos() - t2;

        for (int i = 0; i < ctx.taskCount; i++) {
            FileTask *t = &ctx.tasks[i];
            double ratio = 0.0;
            if (t->originalSize > 0) {
                ratio = 100.0 * (1.0 - ((double)t->compressedSize / t->originalSize));
            }

            char md5Str[33];
            imprimirFirmaMD5(t->md5Original, md5Str);

            const char *displayPath = t->relativePath;
            if (strlen(displayPath) > 25) {
                displayPath = t->relativePath + strlen(t->relativePath) - 25;
            }

            printf("| %-25s | %-32s | %10lu | %10lu | %6.2f%% |\n",
                   displayPath, md5Str, t->originalSize, t->compressedSize, ratio);
        }
        printf("----------------------------------------------------------------------------------------------------\n");
        printf("      -> Compresión en RAM finalizada en %.4f s\n\n", tCompresion);

        // Fase 4: Volcado y empaquetado final al archivo único .huff
        printf("[INFO] Empaquetando catálogo y flujos comprimidos en '%s'...\n", ctx.archivePath);
        double t3 = obtenerTiempoSegundos();
        if (empaquetarArchivoUnificado(&ctx) != 0) {
            liberarContexto(&ctx);
            return 1;
        }
        tEmpaquetado = obtenerTiempoSegundos() - t3;
        printf("      -> Archivo unificado escrito en %.4f s\n\n", tEmpaquetado);
    }

    if (ejecutarDescompresion) {
        // Si no se ejecutó compresión previamente en este proceso (modo solo -d),
        // abrimos el archivo .huff, leemos su catálogo y reconstruimos el árbol
        if (ctx.taskCount == 0) {
            printf("[INFO] Leyendo catálogo del contenedor unificado '%s'...\n", ctx.archivePath);
            double t0Cat = obtenerTiempoSegundos();
            if (leerCatalogoArchivoUnificado(ctx.archivePath, &ctx) != 0) {
                liberarContexto(&ctx);
                return 1;
            }
            tLecturaCatalogo = obtenerTiempoSegundos() - t0Cat;
            printf("       -> Archivos contenidos en el catálogo: %d\n\n", ctx.taskCount);
        }

        printf("[Fase Descompresión] Extrayendo archivos y verificando integridad MD5 con %d hilos...\n", numHilos);
        printf("----------------------------------------------------------------------------------------------------\n");
        printf("| %-25s | %-16s | %-16s | %-10s |\n", "Archivo Extraído", "MD5 Guardado", "MD5 Recalculado", "Estado");
        printf("----------------------------------------------------------------------------------------------------\n");

        double t4 = obtenerTiempoSegundos();
        descomprimirParaleloDesdeUnificado(&ctx);
        tDescompresion = obtenerTiempoSegundos() - t4;

        for (int i = 0; i < ctx.taskCount; i++) {
            FileTask *t = &ctx.tasks[i];
            char md5OrigStr[33], md5RestStr[33];
            imprimirFirmaMD5(t->md5Original, md5OrigStr);
            imprimirFirmaMD5(t->md5Restored, md5RestStr);

            md5OrigStr[16] = '\0';
            md5RestStr[16] = '\0';

            const char *displayPath = t->relativePath;
            if (strlen(displayPath) > 25) {
                displayPath = t->relativePath + strlen(t->relativePath) - 25;
            }

            printf("| %-25s | %-16s | %-16s | %-10s |\n",
                   displayPath, md5OrigStr, md5RestStr,
                   (t->md5Verified ? "[OK] Válido" : "[FAIL] Corrupto"));
        }
        printf("----------------------------------------------------------------------------------------------------\n");
        printf("      -> Extracción y verificación finalizada en %.4f s\n\n", tDescompresion);
    }

    double tTotal = obtenerTiempoSegundos() - tInicioTotal;

    double tTotalCompresor = tFrecuencias + tArbol + tCompresion + tEmpaquetado;
    double tTotalDescompresor = tDescompresion + tLecturaCatalogo;

    // Guardar estadísticas en archivo para la GUI
    if (guardarEstadisticas(statsFile, &ctx, ejecutarCompresion, ejecutarDescompresion,
                            tTotalCompresor, tTotalDescompresor, tTotal) == 0) {
        printf("\n[INFO] Estadísticas guardadas exitosamente en '%s' (formato clave=valor para la GUI).\n", statsFile);
    }

    // Limpieza de memoria dinámica
    limpiarTablaCodigos(ctx.HuffmanCodesArray);
    liberarArbol(ctx.root);
    liberarContexto(&ctx);

    return 0;
}
