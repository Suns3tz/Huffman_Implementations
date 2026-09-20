#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


// ============================================================================
// CONSTANTES Y CONSTANTES MÁGICAS
// ============================================================================
#define MAGIC_HEADER "HUFF"
#define MAGIC_HEADER_LEN 4

// ============================================================================
// ESTRUCTURAS DEL ÁRBOL DE HUFFMAN Y MINHEAP
// ============================================================================

// Nodo del árbol de Huffman
typedef struct MinHeapNode {
    unsigned char data;          // Byte/Carácter ASCII representado
    int frequency;               // Frecuencia de aparición
    struct MinHeapNode *left;    // Hijo izquierdo (bit '0')
    struct MinHeapNode *right;   // Hijo derecho (bit '1')
} MinHeapNode;

// Estructura para el Montículo Mínimo (MinHeap)
typedef struct MinHeap {
    MinHeapNode** elements;      // Arreglo de punteros a nodos
    unsigned size;               // Tamaño actual del heap
    unsigned capacity;           // Capacidad total reservada
} MinHeap;

// ============================================================================
// ESTRUCTURAS PARA EL CATÁLOGO DE METADATOS Y EMPAQUETADO
// ============================================================================

// Metadatos de cada nodo (archivo o carpeta) dentro del paquete .huff
typedef struct {
    uint8_t es_directorio;       // 1 = Directorio, 0 = Archivo regular
    uint16_t len_ruta;           // Longitud en bytes de la ruta relativa
    char ruta_relativa[1024];    // Ruta dentro del paquete (ej. "fotos/playa.jpg")
    uint64_t tam_original;       // Tamaño en bytes sin comprimir
    unsigned char md5[16];
} MetadatoNodo;

// Lista dinámica para almacenar el catálogo de archivos y carpetas
typedef struct {
    MetadatoNodo *elementos;
    size_t cantidad;
    size_t capacidad;
} ListaCatalogo;

// ============================================================================
// VARIABLES GLOBALES DECLARADAS (EXTERN)
// ============================================================================
extern char* HuffmanCodesArray[256]; // Tabla global de códigos ('0' y '1')
extern int ASCIIcount[256];          // Tabla global de frecuencias por byte

// ============================================================================
// PROTOTIPOS DE FUNCIONES - ÁRBOL Y MINHEAP (huffman.c)
// ============================================================================
MinHeapNode* newNode(unsigned char data, int freq);
MinHeap* createMinHeap(unsigned capacity);
void swapMinHeapNode(MinHeapNode** a, MinHeapNode** b);
void siftDown(MinHeap* minHeap, int pos);
int isSizeOne(MinHeap* minHeap);
MinHeapNode* extractMin(MinHeap* minHeap);
void insertMinHeap(MinHeap* minHeap, MinHeapNode* minHeapNode);
void buildMinHeap(MinHeap* minHeap);
int isLeaf(MinHeapNode* root);

MinHeap* createAndBuildMinHeap(int countArray[256]);
MinHeapNode* buildHuffmanTree(int countArray[256]);
void storeCode(int arr[], int n, unsigned char symbol);
void generarTablaCodigos(MinHeapNode* root, int arr[], int top);
void limpiarTablaCodigos(void);
void liberarArbol(MinHeapNode* root);

// ============================================================================
// PROTOTIPOS DE FUNCIONES - LECTURA DE FRECUENCIAS (HuffFrec.c)
// ============================================================================
void countfreq(FILE *RFROM);
void procesarRuta(const char *path);
void extraerfreq(FILE* Freq);

// ============================================================================
// PROTOTIPOS DE FUNCIONES - COMPRESIÓN Y EMPAQUETADO (huffComp.c)
// ============================================================================
void initCatalogo(ListaCatalogo *cat);
void agregarCatalogo(ListaCatalogo *cat, MetadatoNodo nodo);
void freeCatalogo(ListaCatalogo *cat);

void explorarYContar(const char *rutaBase, const char *subRuta, ListaCatalogo *cat);
void escribirTablaFrecuencias(FILE *outFile);
void comprimirContenidoArchivo(FILE *inFile, FILE *outFile, int *bitBuffer, int *bitCount);

// Función principal expuesta para comprimir directorios o archivos en un único paquete .huff
void comprimirDirectorioUnico(const char *rutaOrigen, const char *archivoSalidaHuff);
// PROTOTIPO PARA LA DESCOMPRESIÓN DE PAQUETES
int descomprimirPaqueteUnico(const char *archivoHuff, const char *directorioDestino);
#ifdef __cplusplus
}
#endif

#endif // HUFFMAN_H
