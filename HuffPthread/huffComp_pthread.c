#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <openssl/md5.h>

#include "huffman_pthread.h"

void calcularMD5(const char *rutaArchivo, unsigned char *output) {
    FILE *archivo = fopen(rutaArchivo, "rb");
    if (!archivo) return;

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

static long obtenerTamanoArchivo(FILE* archivo) {
    fseek(archivo, 0, SEEK_END);
    long size = ftell(archivo);
    fseek(archivo, 0, SEEK_SET);
    return size;
}

static void escribirCabecera3Bytes(FILE* archivo, long cant) {
    unsigned char byte1 = (cant >> 16) & 0xFF;
    unsigned char byte2 = (cant >> 8) & 0xFF;
    unsigned char byte3 = cant & 0xFF;
    fputc(byte1, archivo);
    fputc(byte2, archivo);
    fputc(byte3, archivo);
}

void cambiarExtensionAHuff(const char* ogName, char* newName) {
    strcpy(newName, ogName);
    char *extension = strrchr(newName, '.');
    if (extension != NULL) {
        *extension = '\0';
    }
    strcat(newName, ".huff");
}

static void comprimirFlujo(FILE* inFile, FILE* outFile, char* codesArray[256]) {
    int bitBuffer = 0;
    int bitCount = 0;
    int currentByte;

    while ((currentByte = fgetc(inFile)) != EOF) {
        const char *huffCode = codesArray[(unsigned char)currentByte];
        if (huffCode == NULL) continue;

        for (int i = 0; huffCode[i] != '\0'; i++) {
            bitBuffer <<= 1;
            if (huffCode[i] == '1') {
                bitBuffer |= 1;
            }
            bitCount++;
            if (bitCount == 8) {
                fputc(bitBuffer, outFile);
                bitBuffer = 0;
                bitCount = 0;
            }
        }
    }

    if (bitCount > 0) {
        bitBuffer <<= (8 - bitCount);
        fputc(bitBuffer, outFile);
    }
}

void comprimirArchivoIndividualPthread(FileTask *task, char* codesArray[256]) {
    FILE *in = fopen(task->originalPath, "rb");
    if (!in) {
        perror("Error al abrir archivo para comprimir");
        return;
    }

    FILE *out = fopen(task->compressedPath, "wb");
    if (!out) {
        perror("Error al crear archivo .huff");
        fclose(in);
        return;
    }

    long originalSize = obtenerTamanoArchivo(in);
    task->originalSize = originalSize;

    // 1. Escribir tamaño de archivo (3 bytes de encabezado)
    escribirCabecera3Bytes(out, originalSize);

    // 2. Calcular y escribir firma MD5 (16 bytes)
    calcularMD5(task->originalPath, task->md5Original);
    fwrite(task->md5Original, 1, 16, out);

    // 3. Comprimir contenido con la tabla compartida de solo lectura
    rewind(in);
    comprimirFlujo(in, out, codesArray);

    task->compressedSize = obtenerTamanoArchivo(out);

    fclose(in);
    fclose(out);
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
        if (task->isHuff) continue;

        comprimirArchivoIndividualPthread(task, ctx->HuffmanCodesArray);

        pthread_mutex_lock(&ctx->statsMutex);
        ctx->totalOriginalBytes += task->originalSize;
        ctx->totalCompressedBytes += task->compressedSize;
        ctx->totalFilesProcessed++;
        pthread_mutex_unlock(&ctx->statsMutex);
    }

    return NULL;
}

void comprimirParalelo(SharedContext *ctx) {
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

