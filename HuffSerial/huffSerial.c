#include <stdio.h>
#include <stdlib.h>
#include "huffman.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: %s <directorio_a_comprimir>\n", argv[0]);
        return 1;
    }

    printf("====================================================================================================\n");
    printf("                           COMPRESOR HUFFMAN SERIAL (PROYECTO 1)                                   \n");
    printf("====================================================================================================\n\n");

    // Paso 1: Recorrer el directorio y calcular la frecuencia global de caracteres
    printf("[1/3] Analizando frecuencias en todo el directorio: %s...\n", argv[1]);
    procesarRuta(argv[1]);

    // Paso 2: Construir el árbol de Huffman y generar la tabla de códigos
    printf("[2/3] Construyendo Árbol de Huffman unificado en RAM...\n");
    MinHeapNode* root = buildHuffmanTree(ASCIIcount);
    
    if (root == NULL) {
        fprintf(stderr, "Error: No se encontraron archivos válidos o el directorio está vacío.\n");
        return 1;
    }

    int bufferRuta[256];
    int top = 0;
    generarTablaCodigos(root, bufferRuta, top);

    // Paso 3: Comprimir todos los archivos recursivamente guardando el MD5 en la cabecera
    printf("[3/3] Comprimiendo archivos y generando firmas MD5 en metadatos...\n\n");
    printf("----------------------------------------------------------------------------------------------------\n");
    printf("| Archivo Original       | MD5 Signature (16 bytes)         | Tam. Orig. | Tam. Huff  | Ratio   |\n");
    printf("----------------------------------------------------------------------------------------------------\n");

    comprimirRutaRecursiva(argv[1]);

    printf("----------------------------------------------------------------------------------------------------\n");
    printf("\n[OK] ¡Proceso de compresión serial finalizado con éxito!\n");

    return 0;
}
