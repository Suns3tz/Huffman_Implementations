#ifndef HUFFMAN_PTHREAD_H
#define HUFFMAN_PTHREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/stat.h>

// Identificador de Implementación (1 byte / unsigned char)
// 1 = Serial, 2 = Fork, 3 = Pthreads
#define HUFF_IMPLEMENTATION_ID 3

// Nodo del árbol de Huffman y MinHeap
typedef struct MinHeapNode {
    unsigned char data; 
    long frequency;
    struct MinHeapNode* left;
    struct MinHeapNode* right;
} MinHeapNode;

// Información y estado de cada archivo en la cola de trabajo y catálogo
typedef struct {
    char originalPath[1024];      // Ruta en disco original (ej: /dir/sub/doc.txt)
    char relativePath[1024];      // Nombre relativo en catálogo (ej: doc.txt o sub/doc.txt)
    char restoredPath[1024];      // Ruta donde se restaura el archivo
    uint64_t originalSize;        // Tamaño original descomprimido (8 bytes)
    uint64_t compressedSize;      // Tamaño comprimido en bytes (8 bytes)
    uint64_t dataOffset;          // Offset calculado dentro del archivo .huff
    unsigned char md5Original[16];// MD5 Signature original (16 bytes)
    unsigned char md5Restored[16];// MD5 Signature del archivo restaurado
    int md5Verified;              // 1 si coincide, 0 si falla
    unsigned char *compressedBuffer; // Buffer en memoria RAM para compresión
} FileTask;

// Estructura en Memoria Compartida para coordinar los hilos POSIX
typedef struct {
    FileTask *tasks;
    int taskCount;
    int taskCapacity;
    int nextTaskIdx;             // Índice dinámico para asignación de tareas
    pthread_mutex_t queueMutex;  // Mutex para sincronizar acceso a la cola de trabajo

    // Tabla de frecuencias compartida
    long ASCIIcount[256];
    pthread_mutex_t freqMutex;   // Mutex para consolidar frecuencias globales

    // Árbol y tabla de códigos en memoria compartida (solo lectura para hilos)
    MinHeapNode* root;
    char* HuffmanCodesArray[256];

    // Estadísticas globales compartidas
    uint64_t totalOriginalBytes;
    uint64_t totalCompressedBytes;
    uint64_t archiveFileSize;
    int totalFilesProcessed;
    int totalVerifiedFiles;
    pthread_mutex_t statsMutex;  // Mutex para actualizar estadísticas globales

    // Configuración de concurrencia y retención de archivos
    unsigned char implementationId; // ID de implementación (3 = Pthreads)
    int numThreads;
    int keepFiles;               // 1: Conservar originales y .huff; 0: Eliminar residuos
    int isDirectory;             // 1: se comprimió una carpeta; 0: archivo solitario
    char archivePath[1024];      // Ruta del archivo unificado .huff
    char basePath[1024];         // Directorio base para rutas relativas
    char originalTarget[1024];   // Ruta exacta del objetivo original ingresado por el usuario
} SharedContext;

// Estructura de argumento individual para cada hilo de trabajo
typedef struct {
    int threadId;
    SharedContext *ctx;
} ThreadArg;

// Prototipos de Escaneo y Frecuencias
int inicializarContexto(SharedContext *ctx, int numThreads, int keepFiles, const char *ruta);
void liberarContexto(SharedContext *ctx);
void escanearRutaRecursiva(const char *path, SharedContext *ctx, const char *basePath);
void calcularFrecuenciasParalelo(SharedContext *ctx);

// Prototipos de Árbol y Códigos
MinHeapNode* buildHuffmanTree(long countArray[256]);
void generarTablaCodigos(MinHeapNode* root, int arr[], int top, char* codesArray[256]);
void limpiarTablaCodigos(char* codesArray[256]);
void liberarArbol(MinHeapNode* root);

// Prototipos de Compresión Unificada
void comprimirParaleloABuffers(SharedContext *ctx);
int empaquetarArchivoUnificado(SharedContext *ctx);

// Prototipos de Descompresión Unificada
int leerCatalogoArchivoUnificado(const char *archivePath, SharedContext *ctx);
void descomprimirParaleloDesdeUnificado(SharedContext *ctx);

// Utilidades Criptográficas y de Rutas
void calcularMD5(const char *rutaArchivo, unsigned char *output);
void calcularMD5Buffer(const unsigned char *buffer, size_t len, unsigned char *output);
void crearDirectoriosPadre(const char *filePath);
void eliminarRutaRecursiva(const char *path);

#endif
