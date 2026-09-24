#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <openssl/md5.h>

#define MAGIC_HEADER "HUFF"
#define MAGIC_HEADER_LEN 4
#define PATH_MAX_LEN 1024

static inline void calcularMD5(const char *rutaArchivo, unsigned char *output) {
    FILE *archivo = fopen(rutaArchivo, "rb");
    if (!archivo) return;

    MD5_CTX mdContext;
    unsigned char buffer[1024];
    size_t bytes;

    MD5_Init(&mdContext);
    while ((bytes = fread(buffer, 1, sizeof(buffer), archivo)) > 0) {
        MD5_Update(&mdContext, buffer, bytes);
    }
    MD5_Final(output, &mdContext);

    fclose(archivo);
}

// Nodo del árbol de Huffman
typedef struct MinHeapNode {
    unsigned char data;
    int frequency;
    struct MinHeapNode *left;
    struct MinHeapNode *right;
} MinHeapNode;

// Estructura para el Montículo Mínimo (MinHeap)
typedef struct MinHeap {
    MinHeapNode** elements;      // Arreglo de punteros a nodos
    unsigned size;               // Tamaño actual del heap
    unsigned capacity;           // Capacidad total reservada
} MinHeap;

// Metadatos de cada nodo (archivo o carpeta) dentro del paquete .huff
typedef struct {
    uint8_t es_directorio;       // 1 = Directorio, 0 = Archivo regular
    uint16_t len_ruta;           // Longitud en bytes de la ruta relativa
    char ruta_relativa[PATH_MAX_LEN]; // Ruta dentro del paquete
    uint64_t tam_original;       // Tamaño en bytes sin comprimir
    unsigned char md5[16];       // Firma MD5
} MetadatoNodo;

// Lista dinámica para almacenar el catálogo de archivos y carpetas
typedef struct {
    MetadatoNodo *elementos;
    size_t cantidad;
    size_t capacidad;
} ListaCatalogo;

extern char* HuffmanCodesArray[256]; // Tabla global de códigos ('0' y '1')
extern int ASCIIcount[256];          // Tabla global de frecuencias por byte

// Prototipos de MinHeap y Huffman Tree
MinHeapNode* newNode(unsigned char data, int freq);
MinHeap* createMinHeap(unsigned capacity);
void swapMinHeapNode(MinHeapNode** a, MinHeapNode** b);
void siftDown(MinHeap* minHeap, int pos);
int isSizeOne(MinHeap* minHeap);
MinHeapNode* extractMin(MinHeap* minHeap);
void insertMinHeap(MinHeap* minHeap, MinHeapNode* minHeapNode);
void buildMinHeap(MinHeap* minHeap);
int isLeaf(MinHeapNode* root);

MinHeap* createAndBuildMinHeap(const int countArray[256]);
MinHeapNode* buildHuffmanTree(const int countArray[256]);
void storeCode(int arr[], int n, unsigned char symbol);
void generarTablaCodigos(MinHeapNode* root, int arr[], int top);
void limpiarTablaCodigos(void);
void liberarArbol(MinHeapNode* root);

// Lectura de frecuencias
void countfreq(FILE *RFROM);
void procesarRuta(const char *path);
void extraerfreq(FILE* Freq);

// Compresión y empaquetado
void initCatalogo(ListaCatalogo *cat);
void agregarCatalogo(ListaCatalogo *cat, MetadatoNodo nodo);
void freeCatalogo(ListaCatalogo *cat);

void explorarYContar(const char *rutaBase, const char *subRuta, ListaCatalogo *cat);
void escribirTablaFrecuencias(FILE *outFile);
void comprimirContenidoArchivo(FILE *inFile, FILE *outFile, int *bitBuffer, int *bitCount);

void comprimirDirectorioUnico(const char *rutaOrigen, const char *archivoSalidaHuff);
int descomprimirPaqueteUnico(const char *archivoHuff, const char *directorioDestino);



#endif // HUFFMAN_H
