#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h>

// Estructura compartida
typedef struct MinHeapNode {
    unsigned char data; 
    int frequency;
    struct MinHeapNode* left;
    struct MinHeapNode* right;
} MinHeapNode;

// Variables globales exportadas
extern int ASCIIcount[256];
extern char* HuffmanCodesArray[256];

// Prototipos Frecuencias y Árbol
void countfreq(FILE *RFROM);
void procesarRuta(const char *path);
MinHeapNode* buildHuffmanTree(int countArray[256]);
void generarTablaCodigos(MinHeapNode* root, int arr[], int top);

// Prototipos Compresión y Descompresión
void comprimirRutaRecursiva(const char *path);
void descomprimirRutaRecursiva(const char *path, MinHeapNode* root, int *verificados, int *totales);

#endif
