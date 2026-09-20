#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "huffman_pthread.h"
#include "md5.h"

void calcularMD5(const char *rutaArchivo, unsigned char *output) {
    FILE *archivo = fopen(rutaArchivo, "rb");
    if (!archivo) {
        memset(output, 0, 16);
        return;
    }

    MD5_CTX mdContext;
    unsigned char buffer[4096];
    size_t bytes;

    MD5_Init(&mdContext);
    while ((bytes = fread(buffer, 1, sizeof(buffer), archivo)) > 0) {
        MD5_Update(&mdContext, buffer, bytes);
    }
    MD5_Final(output, &mdContext);

    fclose(archivo);
}

void calcularMD5Buffer(const unsigned char *buffer, size_t len, unsigned char *output) {
    MD5_CTX mdContext;
    MD5_Init(&mdContext);
    MD5_Update(&mdContext, buffer, len);
    MD5_Final(output, &mdContext);
}

typedef struct {
    unsigned char *data;
    size_t size;
    size_t capacity;
} MemoryBuffer;

static void memBufInit(MemoryBuffer *mb, size_t initialCapacity) {
    if (initialCapacity < 256) initialCapacity = 256;
    mb->data = (unsigned char*)malloc(initialCapacity);
    mb->size = 0;
    mb->capacity = initialCapacity;
}

static inline void memBufAppendByte(MemoryBuffer *mb, unsigned char byte) {
    if (mb->size >= mb->capacity) {
        mb->capacity = (mb->capacity * 3) / 2 + 1024;
        mb->data = (unsigned char*)realloc(mb->data, mb->capacity);
    }
    mb->data[mb->size++] = byte;
}

static void comprimirFlujoAMemoria(FILE* inFile, MemoryBuffer *mb, char* codesArray[256]) {
    int bitBuffer = 0;
    int bitCount = 0;
    unsigned char readBuf[4096];
    size_t bytes;

    while ((bytes = fread(readBuf, 1, sizeof(readBuf), inFile)) > 0) {
        for (size_t b = 0; b < bytes; b++) {
            const char *huffCode = codesArray[readBuf[b]];
            if (huffCode == NULL) continue;

            for (int i = 0; huffCode[i] != '\0'; i++) {
                bitBuffer <<= 1;
                if (huffCode[i] == '1') {
                    bitBuffer |= 1;
                }
                bitCount++;
                if (bitCount == 8) {
                    memBufAppendByte(mb, (unsigned char)bitBuffer);
                    bitBuffer = 0;
                    bitCount = 0;
                }
            }
        }
    }

    if (bitCount > 0) {
        bitBuffer <<= (8 - bitCount);
        memBufAppendByte(mb, (unsigned char)bitBuffer);
    }
}

static void comprimirArchivoABuffer(FileTask *task, char* codesArray[256]) {
    FILE *in = fopen(task->originalPath, "rb");
    if (!in) {
        perror("Error al abrir archivo para comprimir");
        return;
    }

    fseek(in, 0, SEEK_END);
    task->originalSize = (uint64_t)ftell(in);
    fseek(in, 0, SEEK_SET);

    calcularMD5(task->originalPath, task->md5Original);

    MemoryBuffer mb;
    memBufInit(&mb, (size_t)(task->originalSize / 2 + 512));

    comprimirFlujoAMemoria(in, &mb, codesArray);
    fclose(in);

    task->compressedBuffer = mb.data;
    task->compressedSize = (uint64_t)mb.size;
}

static void* workerCompresion(void *arg) {
    ThreadArg *tArg = (ThreadArg*)arg;
    SharedContext *ctx = tArg->ctx;

    while (1) {
        int taskIdx = -1;

        pthread_mutex_lock(&ctx->queueMutex);
        if (ctx->nextTaskIdx < ctx->taskCount) {
            taskIdx = ctx->nextTaskIdx++;
        }
        pthread_mutex_unlock(&ctx->queueMutex);

        if (taskIdx == -1) break;

        FileTask *task = &ctx->tasks[taskIdx];
        comprimirArchivoABuffer(task, ctx->HuffmanCodesArray);

        pthread_mutex_lock(&ctx->statsMutex);
        ctx->totalOriginalBytes += task->originalSize;
        ctx->totalCompressedBytes += task->compressedSize;
        ctx->totalFilesProcessed++;
        pthread_mutex_unlock(&ctx->statsMutex);
    }

    return NULL;
}

void comprimirParaleloABuffers(SharedContext *ctx) {
    ctx->nextTaskIdx = 0;
    pthread_t *threads = (pthread_t*)malloc(ctx->numThreads * sizeof(pthread_t));
    ThreadArg *args = (ThreadArg*)malloc(ctx->numThreads * sizeof(ThreadArg));

    for (int i = 0; i < ctx->numThreads; i++) {
        args[i].threadId = i;
        args[i].ctx = ctx;
        pthread_create(&threads[i], NULL, workerCompresion, &args[i]);
    }

    for (int i = 0; i < ctx->numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(args);
}

int empaquetarArchivoUnificado(SharedContext *ctx) {
    FILE *out = fopen(ctx->archivePath, "wb");
    if (!out) {
        perror("Error al crear archivo unificado .huff");
        return -1;
    }

    // 1. Cantidad de Archivos (4 bytes)
    uint32_t count = (uint32_t)ctx->taskCount;
    fwrite(&count, sizeof(uint32_t), 1, out);

    // 2. Indicador de Carpeta vs Archivo Solitario (1 byte)
    uint8_t isDir = (uint8_t)ctx->isDirectory;
    fwrite(&isDir, sizeof(uint8_t), 1, out);

    // 3. Tabla Global de Frecuencias de Huffman (1024 bytes: 256 * 4 bytes)
    uint32_t freq32[256];
    for (int i = 0; i < 256; i++) {
        freq32[i] = (uint32_t)ctx->ASCIIcount[i];
    }
    fwrite(freq32, sizeof(uint32_t), 256, out);

    // Calcular dataOffset para cada archivo
    uint64_t currentOffset = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) * 256;
    for (int i = 0; i < ctx->taskCount; i++) {
        uint16_t pathLen = (uint16_t)strlen(ctx->tasks[i].relativePath);
        currentOffset += sizeof(uint16_t) + pathLen + sizeof(uint64_t) * 2 + 16;
    }
    for (int i = 0; i < ctx->taskCount; i++) {
        ctx->tasks[i].dataOffset = currentOffset;
        currentOffset += ctx->tasks[i].compressedSize;
    }

    // 4. Catálogo de Archivos
    for (int i = 0; i < ctx->taskCount; i++) {
        FileTask *t = &ctx->tasks[i];
        uint16_t pathLen = (uint16_t)strlen(t->relativePath);
        // - Longitud del nombre (2 bytes)
        fwrite(&pathLen, sizeof(uint16_t), 1, out);
        // - Nombre (cadena)
        fwrite(t->relativePath, 1, pathLen, out);
        // - Tamaño original descomprimido (8 bytes)
        fwrite(&t->originalSize, sizeof(uint64_t), 1, out);
        // - Tamaño comprimido en bytes (8 bytes)
        fwrite(&t->compressedSize, sizeof(uint64_t), 1, out);
        // - MD5 Signature original (16 bytes)
        fwrite(t->md5Original, 1, 16, out);
    }

    // 5. DATOS COMPRIMIDOS (Bits Huffman de todos los archivos concatenados)
    for (int i = 0; i < ctx->taskCount; i++) {
        FileTask *t = &ctx->tasks[i];
        if (t->compressedBuffer && t->compressedSize > 0) {
            fwrite(t->compressedBuffer, 1, t->compressedSize, out);
            free(t->compressedBuffer);
            t->compressedBuffer = NULL;
        }
    }

    fclose(out);

    // 6. Eliminar archivo u objetivo original si no se especificó -k
    if (!ctx->keepFiles) {
        eliminarRutaRecursiva(ctx->originalTarget);
    }

    return 0;
}
