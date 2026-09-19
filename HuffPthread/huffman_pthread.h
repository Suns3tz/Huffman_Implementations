#ifndef HUFFMAN_PTHREAD_H
#define HUFFMAN_PTHREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/stat.h>

// Estructura del nodo del árbol de Huffman y MinHeap
typedef struct MinHeapNode {
    unsigned char data; 
    long frequency;
    struct MinHeapNode* left;
    struct MinHeapNode* right;
} MinHeapNode;

// Información y estado de cada archivo en la cola de trabajo
typedef struct {
    char originalPath[1024];
    char compressedPath[1024];
    char restoredPath[1024];
    long originalSize;
    long compressedSize;
    unsigned char md5Original[16];
    unsigned char md5Restored[16];
    int md5Verified; // 1 si coincide, 0 si falla
    int isHuff;      // 1 si el archivo original termina en .huff
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
    long totalOriginalBytes;
    long totalCompressedBytes;
    int totalFilesProcessed;
    int totalVerifiedFiles;
    pthread_mutex_t statsMutex;  // Mutex para actualizar estadísticas globales

    // Configuración de concurrencia
    int numThreads;
} SharedContext;

// Estructura de argumento individual para cada hilo de trabajo
typedef struct {
    int threadId;
    SharedContext *ctx;
} ThreadArg;

// Prototipos de Escaneo y Frecuencias
int inicializarContexto(SharedContext *ctx, int numThreads);
void liberarContexto(SharedContext *ctx);
void escanearRutaRecursiva(const char *path, SharedContext *ctx);
void calcularFrecuenciasParalelo(SharedContext *ctx);

// Prototipos de Árbol y Códigos
MinHeapNode* buildHuffmanTree(long countArray[256]);
void generarTablaCodigos(MinHeapNode* root, int arr[], int top, char* codesArray[256]);
void limpiarTablaCodigos(char* codesArray[256]);
void liberarArbol(MinHeapNode* root);

// Prototipos de Compresión Paralela
void comprimirParalelo(SharedContext *ctx);
void comprimirArchivoIndividualPthread(FileTask *task, char* codesArray[256]);

// Prototipos de Descompresión Paralela
void descomprimirParalelo(SharedContext *ctx);
int descomprimirArchivoIndividualPthread(FileTask *task, MinHeapNode* root);

// Utilidades Criptográficas y Auxiliares
void calcularMD5(const char *rutaArchivo, unsigned char *output);
void cambiarExtensionAHuff(const char* ogName, char* newName);
void restaurarNombreSinHuff(const char* huffName, char* restoredName);

#endif

