#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/md5.h>

#include "huffman.h"

// --------------------------------------------------------------------------
// Función auxiliar para calcular el MD5 de un archivo en disco
// --------------------------------------------------------------------------
int calcularMD5(const char *rutaArchivo, unsigned char *md5Out) {
    FILE *f = fopen(rutaArchivo, "rb");
    if (!f) return 0;

    MD5_CTX ctx;
    MD5_Init(&ctx);

    unsigned char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        MD5_Update(&ctx, buffer, bytes);
    }
    MD5_Final(md5Out, &ctx);

    fclose(f);
    return 1;
}

// --------------------------------------------------------------------------
// Función en el main para verificar la salud leyendo el .huff y los archivos extraídos
// --------------------------------------------------------------------------
double verificarSaludPaquete(const char *archivoHuff, const char *dirDestino) {
    FILE *in = fopen(archivoHuff, "rb");
    if (!in) {
        printf("Error: No se pudo abrir el archivo .huff para verificar salud.\n");
        return 0.0;
    }

    // 1. Saltar Encabezado Global (Magic Header + Versión + Hash Global)
    fseek(in, 4 + 1 + 16, SEEK_CUR);

    // 2. Leer Tabla de Frecuencias (Saltarla para llegar al catálogo)
    int simbolosPresentes = fgetc(in);
    int totalSimbolos = (simbolosPresentes == 0) ? 256 : simbolosPresentes;
    fseek(in, totalSimbolos * (1 + sizeof(uint32_t)), SEEK_CUR);

    // 3. Leer Cantidad de Elementos del Catálogo
    uint32_t totalElementos = 0;
    if (fread(&totalElementos, sizeof(uint32_t), 1, in) != 1) {
        fclose(in);
        return 0.0;
    }

    int archivosTotales = 0;
    int archivosCorrectos = 0;

    // 4. Recorrer el Catálogo
    for (uint32_t i = 0; i < totalElementos; i++) {
        uint8_t esDir = fgetc(in);
        uint16_t lenRuta = 0;
        fread(&lenRuta, sizeof(uint16_t), 1, in);

        char rutaRelativa[1024] = {0};
        fread(rutaRelativa, 1, lenRuta, in);

        uint64_t tamOriginal = 0;
        fread(&tamOriginal, sizeof(uint64_t), 1, in);

        unsigned char md5Esperado[16];
        fread(md5Esperado, 1, 16, in); // Leer firma MD5 guardada

        // Ignorar carpetas
        if (!esDir) {
            archivosTotales++;

            // Construir la ruta donde debió extraerse el archivo
            char rutaExtraida[2048];
            if (dirDestino && strlen(dirDestino) > 0) {
                snprintf(rutaExtraida, sizeof(rutaExtraida), "%s/%s", dirDestino, rutaRelativa);
            } else {
                snprintf(rutaExtraida, sizeof(rutaExtraida), "%s", rutaRelativa);
            }

            // Calcular MD5 del archivo extraído
            unsigned char md5Calculado[16];
            if (calcularMD5(rutaExtraida, md5Calculado)) {
                if (memcmp(md5Esperado, md5Calculado, 16) == 0) {
                    archivosCorrectos++;
                } else {
                    printf(" [!] Archivo corrupto o alterado: %s\n", rutaExtraida);
                }
            } else {
                printf(" [!] No se encontró el archivo extraído: %s\n", rutaExtraida);
            }
        }
    }

    fclose(in);

    if (archivosTotales == 0) return 0.0;
    return ((double)archivosCorrectos / archivosTotales) * 100.0;
}

// --------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Uso:\n");
        printf("  Comprimir:    %s -c <carpeta_origen> <salida.huff>\n", argv[0]);
        printf("  Descomprimir: %s -d <archivo.huff> <carpeta_destino>\n", argv[0]);
        return 1;
    }

    struct timespec inicio, fin;
    double tiempoSegundos;

    if (strcmp(argv[1], "-c") == 0) {
        printf("=== INICIANDO COMPRESIÓN ===\n");

        clock_gettime(CLOCK_MONOTONIC, &inicio);
        comprimirDirectorioUnico(argv[2], argv[3]);
        clock_gettime(CLOCK_MONOTONIC, &fin);

        tiempoSegundos = (fin.tv_sec - inicio.tv_sec) + 
                         (fin.tv_nsec - inicio.tv_nsec) / 1e9;

        printf("\n------------------------------------------\n");
        printf("Tiempo total de compresión: %.4f segundos\n", tiempoSegundos);
        printf("------------------------------------------\n");

    } else if (strcmp(argv[1], "-d") == 0) {
        printf("=== INICIANDO DESCOMPRESIÓN ===\n");

        // 1. Medir y ejecutar descompresión
        clock_gettime(CLOCK_MONOTONIC, &inicio);
        descomprimirPaqueteUnico(argv[2], argv[3]);
        clock_gettime(CLOCK_MONOTONIC, &fin);

        tiempoSegundos = (fin.tv_sec - inicio.tv_sec) + 
                         (fin.tv_nsec - inicio.tv_nsec) / 1e9;

        // 2. Medir salud del paquete en el main
        printf("\nVerificando integridad del paquete extraído...\n");
        double salud = verificarSaludPaquete(argv[2], argv[3]);

        printf("\n------------------------------------------\n");
        printf("REPORTE DE EJECUCIÓN Y SALUD:\n");
        printf("  - Tiempo total de descompresión: %.4f segundos\n", tiempoSegundos);
        printf("  - Porcentaje de salud del paquete: %.2f%%\n", salud);
        printf("------------------------------------------\n");

    } else {
        printf("Error: Opción '%s' no válida.\n", argv[1]);
        return 1;
    }

    return 0;
}
