#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <openssl/md5.h>

#include "huffman.h"

// Obtiene el tamaño en bytes de un archivo en disco
uint64_t obtenerTamanoArchivo(const char *ruta) {
    struct stat st;
    if (stat(ruta, &st) == 0) {
        return (uint64_t)st.st_size;
    }
    return 0;
}

// Función para guardar en "stats_serial"
void guardarEstadisticasExplicito(double salud, int correctos, int totales, 
                                 double tComp, double tDecomp, 
                                 uint64_t tamOrig, uint64_t tamComp) {
    FILE *f = fopen("stats_serial", "w");
    if (!f) {
        perror("Error creando archivo stats_serial");
        return;
    }

    double ratio = 0.0;
    if (tamOrig > 0) {
        ratio = (1.0 - ((double)tamComp / (double)tamOrig)) * 100.0;
    }

    fprintf(f, "porcentaje_salud=%.2f\n", salud);
    fprintf(f, "firmas_verificadas=%d\n", correctos);
    fprintf(f, "total_archivos=%d\n", totales);
    fprintf(f, "tiempo_total_compresor=%.4f\n", tComp);
    fprintf(f, "tiempo_total_descompresor=%.4f\n", tDecomp);
    fprintf(f, "tamano_total_original_bytes=%llu\n", (unsigned long long)tamOrig);
    fprintf(f, "tamano_archivo_comprimido_bytes=%llu\n", (unsigned long long)tamComp);
    fprintf(f, "radio_compresion=%.2f\n", ratio);

    fclose(f);
}

// Recorre el catálogo e inspecciona el tamaño total original además de validar MD5
double verificarSaludPaquete(const char *archivoHuff, const char *dirDestino, 
                            int *outTotales, int *outCorrectos, uint64_t *outTamanoOriginal) {
    FILE *in = fopen(archivoHuff, "rb");
    if (!in) {
        printf("Error: No se pudo abrir el archivo .huff para verificar salud.\n");
        return 0.0;
    }
    
    // 1. Saltar Encabezado Global (ID + Magic Header + Versión + Hash Global)
    fseek(in, 1 + 4 + 1 + 16, SEEK_CUR);

    // 2. Leer Tabla de Frecuencias
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
    uint64_t acumuladoTamanoOriginal = 0;

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
        fread(md5Esperado, 1, 16, in);

        if (!esDir) {
            archivosTotales++;
            acumuladoTamanoOriginal += tamOriginal;

            char rutaExtraida[2048];
            if (dirDestino && strlen(dirDestino) > 0) {
                snprintf(rutaExtraida, sizeof(rutaExtraida), "%s/%s", dirDestino, rutaRelativa);
            } else {
                snprintf(rutaExtraida, sizeof(rutaExtraida), "%s", rutaRelativa);
            }

            unsigned char md5Calculado[16];
            calcularMD5(rutaExtraida, md5Calculado);
            
            //Contabiliza aciertos de md5
            if (memcmp(md5Esperado, md5Calculado, 16) == 0) {
                archivosCorrectos++;
            } else {
                printf("Error: Mismatch de MD5 en el archivo %s\n", rutaExtraida);
            }
        }
    }

    fclose(in);

    if (outTotales) *outTotales = archivosTotales;
    if (outCorrectos) *outCorrectos = archivosCorrectos;
    if (outTamanoOriginal) *outTamanoOriginal = acumuladoTamanoOriginal;

    if (archivosTotales == 0) return 0.0;
    return ((double)archivosCorrectos / archivosTotales) * 100.0;
}

// --------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso:\n");
        printf("  Comprimir:     %s -c <carpeta_origen> <salida.huff>\n", argv[0]);
        printf("  Descomprimir:  %s -d <archivo.huff> <carpeta_destino>\n", argv[0]);
        printf("  Ejecutar Todo: %s -all <carpeta_origen>\n", argv[0]);
        return 1;
    }

    struct timespec inicio, fin;
    double tiempoCompresion = 0.0;
    double tiempoDescompresion = 0.0;

    if (strcmp(argv[1], "-all") == 0) {
        printf("=== INICIANDO BENCHMARK COMPLETO (SERIAL) ===\n");

        char archivoHuff[1024];
        char carpetaDestino[1024];

       
        snprintf(archivoHuff, sizeof(archivoHuff), "%s.huff", argv[2]);
        snprintf(carpetaDestino, sizeof(carpetaDestino), "%s_extraido", argv[2]);

        // 1. Compresión
        printf("\n[1/3] Comprimiendo...\n");
        clock_gettime(CLOCK_MONOTONIC, &inicio);
        comprimirDirectorioUnico(argv[2], archivoHuff);
        clock_gettime(CLOCK_MONOTONIC, &fin);
        tiempoCompresion = (fin.tv_sec - inicio.tv_sec) + (fin.tv_nsec - inicio.tv_nsec) / 1e9;

        // 2. Descompresión
        printf("\n[2/3] Descomprimiendo...\n");
        clock_gettime(CLOCK_MONOTONIC, &inicio);
        descomprimirPaqueteUnico(archivoHuff, carpetaDestino);
        clock_gettime(CLOCK_MONOTONIC, &fin);
        tiempoDescompresion = (fin.tv_sec - inicio.tv_sec) + (fin.tv_nsec - inicio.tv_nsec) / 1e9;

        // 3. Verificación de Salud y Recolección de Tamaños
        printf("\n[3/3] Verificando salud y métricas...\n");
        int tot = 0, corr = 0;
        uint64_t tamanoOriginalBytes = 0;
        double salud = verificarSaludPaquete(archivoHuff, carpetaDestino, &tot, &corr, &tamanoOriginalBytes);
        uint64_t tamanoComprimidoBytes = obtenerTamanoArchivo(archivoHuff);

        guardarEstadisticasExplicito(salud, corr, tot, tiempoCompresion, tiempoDescompresion, tamanoOriginalBytes, tamanoComprimidoBytes);

        printf("\n------------------------------------------\n");
        printf("REPORTE FINAL DE BENCHMARK (stats_serial):\n");
        printf("  - Porcentaje Salud:         %.2f%%\n", salud);
        printf("  - Tiempo Compresión:        %.4f s\n", tiempoCompresion);
        printf("  - Tiempo Descompresión:     %.4f s\n", tiempoDescompresion);
        printf("  - Tamaños:                  Original: %llu B | Comprimido: %llu B\n", 
               (unsigned long long)tamanoOriginalBytes, (unsigned long long)tamanoComprimidoBytes);
        printf("------------------------------------------\n");

    } else if (strcmp(argv[1], "-c") == 0) {
        if (argc < 4) { printf("Error de parámetros para -c\n"); return 1; }
        printf("=== INICIANDO COMPRESIÓN ===\n");

        clock_gettime(CLOCK_MONOTONIC, &inicio);
        comprimirDirectorioUnico(argv[2], argv[3]);
        clock_gettime(CLOCK_MONOTONIC, &fin);

        tiempoCompresion = (fin.tv_sec - inicio.tv_sec) + (fin.tv_nsec - inicio.tv_nsec) / 1e9;
        uint64_t tamComp = obtenerTamanoArchivo(argv[3]);

        guardarEstadisticasExplicito(100.0, 0, 0, tiempoCompresion, 0.0, 0, tamComp);

        printf("\nTiempo total de compresión: %.4f segundos\n", tiempoCompresion);

    } else if (strcmp(argv[1], "-d") == 0) {
        if (argc < 4) { printf("Error de parámetros para -d\n"); return 1; }
        printf("=== INICIANDO DESCOMPRESIÓN ===\n");

        clock_gettime(CLOCK_MONOTONIC, &inicio);
        descomprimirPaqueteUnico(argv[2], argv[3]);
        clock_gettime(CLOCK_MONOTONIC, &fin);

        tiempoDescompresion = (fin.tv_sec - inicio.tv_sec) + (fin.tv_nsec - inicio.tv_nsec) / 1e9;

        int tot = 0, corr = 0;
        uint64_t tamOrig = 0;
        double salud = verificarSaludPaquete(argv[2], argv[3], &tot, &corr, &tamOrig);
        uint64_t tamComp = obtenerTamanoArchivo(argv[2]);

        guardarEstadisticasExplicito(salud, corr, tot, 0.0, tiempoDescompresion, tamOrig, tamComp);

        printf("\nTiempo total de descompresión: %.4f segundos\n", tiempoDescompresion);
        printf("Porcentaje de salud: %.2f%%\n", salud);

    } else {
        printf("Error: Opción '%s' no válida.\n", argv[1]);
        return 1;
    }

    return 0;
}
