#include <stdio.h>
#include <stdlib.h>
#include "huffman.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: %s <directorio_origen_libros>\n", argv[0]);
        return 1;
    }

    const char* dirOrigen = argv[1];
    const char* dirComprimidos = "archivos_comprimidos";
    const char* dirDescomprimidos = "archivos_descomprimidos";

    printf("====================================================================================================\n");
    printf("                       HUFFMAN SERIAL - GESTIÓN DE DIRECTORIOS DE SALIDA                          \n");
    printf("====================================================================================================\n\n");

    // 1. Frecuencias
    printf("[1/4] Analizando frecuencias en origen: '%s'...\n", dirOrigen);
    procesarRutaFrecuencias(dirOrigen);

    // 2. Árbol en RAM
    printf("[2/4] Construyendo Árbol de Huffman unificado en RAM...\n");
    MinHeapNode* root = buildHuffmanTree(ASCIIcount);
    if (!root) {
        fprintf(stderr, "Error: No se pudieron leer archivos del directorio origen.\n");
        return 1;
    }
    int bufferRuta[256], top = 0;
    generarTablaCodigos(root, bufferRuta, top);

    // 3. Compresión hacia carpeta dedicada
    printf("[3/4] Comprimiendo archivos desde '%s' hacia '%s'...\n\n", dirOrigen, dirComprimidos);
    printf("----------------------------------------------------------------------------------------------------\n");
    printf("| Archivo Original       | MD5 Signature (16 bytes)         | Tam. Orig. | Tam. Huff  | Ratio   |\n");
    printf("----------------------------------------------------------------------------------------------------\n");

    comprimirRutaRecursivaConDestino(dirOrigen, dirComprimidos);

    printf("----------------------------------------------------------------------------------------------------\n\n");

    // 4. Descompresión hacia carpeta dedicada
    printf("[4/4] Descomprimiendo desde '%s' hacia '%s'...\n\n", dirComprimidos, dirDescomprimidos);

    int verificados = 0, totales = 0;
    descomprimirRutaRecursivaConDestino(dirComprimidos, dirDescomprimidos, root, &verificados, &totales);

    printf("\n====================================================================================================\n");
    printf("Archivos .huff procesados : %d\n", totales);
    printf("Verificaciones MD5 exito  : %d\n", verificados);
    if (totales > 0) {
        printf("Salud de la compresión    : %.2f%%\n", ((double)verificados / totales) * 100.0);
    }
    printf("====================================================================================================\n");

    return 0;
}
