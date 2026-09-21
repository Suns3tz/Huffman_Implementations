#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "huffman_pthread.h"
#include "md5.h"

int leerCatalogoArchivoUnificado(const char *archivePath, SharedContext *ctx) {
    FILE *in = fopen(archivePath, "rb");
    if (!in) {
        perror("Error al abrir archivo unificado .huff para lectura");
        return -1;
    }

    fseek(in, 0, SEEK_END);
    ctx->archiveFileSize = (uint64_t)ftell(in);
    fseek(in, 0, SEEK_SET);

    // 0. Identificador de Implementación (1 byte / unsigned char)
    unsigned char implId = 0;
    if (fread(&implId, sizeof(unsigned char), 1, in) != 1) {
        fprintf(stderr, "Error al leer ID de implementación en '%s'.\n", archivePath);
        fclose(in);
        return -1;
    }
    ctx->implementationId = implId;
    if (implId != HUFF_IMPLEMENTATION_ID) {
        fprintf(stderr, "Error: El archivo '%s' tiene ID de implementación %u (se esperaba %u para Pthreads).\n",
                archivePath, (unsigned int)implId, (unsigned int)HUFF_IMPLEMENTATION_ID);
        fclose(in);
        return -1;
    }

    // 1. Cantidad de Archivos (4 bytes)
    uint32_t totalFiles;
    if (fread(&totalFiles, sizeof(uint32_t), 1, in) != 1) {
        fprintf(stderr, "Error al leer cantidad de archivos en '%s'.\n", archivePath);
        fclose(in);
        return -1;
    }

    // 2. Indicador de Carpeta vs Archivo Solitario (1 byte)
    uint8_t isDir;
    if (fread(&isDir, sizeof(uint8_t), 1, in) != 1) {
        fprintf(stderr, "Error al leer indicador de tipo en '%s'.\n", archivePath);
        fclose(in);
        return -1;
    }
    ctx->isDirectory = (int)isDir;

    // 3. Tabla Global de Frecuencias de Huffman (1024 bytes: 256 * 4 bytes)
    uint32_t freq32[256];
    if (fread(freq32, sizeof(uint32_t), 256, in) != 256) {
        fprintf(stderr, "Error al leer tabla de frecuencias global en '%s'.\n", archivePath);
        fclose(in);
        return -1;
    }

    for (int i = 0; i < 256; i++) {
        ctx->ASCIIcount[i] = (long)freq32[i];
    }

    // Reasignar arreglo de tareas
    if (ctx->taskCapacity < (int)totalFiles) {
        ctx->taskCapacity = (int)totalFiles + 16;
        ctx->tasks = (FileTask*)realloc(ctx->tasks, ctx->taskCapacity * sizeof(FileTask));
    }
    ctx->taskCount = (int)totalFiles;

    // 4. Leer Catálogo de Archivos
    for (int i = 0; i < ctx->taskCount; i++) {
        FileTask *t = &ctx->tasks[i];
        memset(t, 0, sizeof(FileTask));

        uint16_t pathLen;
        if (fread(&pathLen, sizeof(uint16_t), 1, in) != 1) break;
        if (fread(t->relativePath, 1, pathLen, in) != pathLen) break;
        t->relativePath[pathLen] = '\0';

        if (fread(&t->originalSize, sizeof(uint64_t), 1, in) != 1) break;
        if (fread(&t->compressedSize, sizeof(uint64_t), 1, in) != 1) break;
        if (fread(t->md5Original, 1, 16, in) != 16) break;

        ctx->totalOriginalBytes += t->originalSize;
        ctx->totalCompressedBytes += t->compressedSize;

        t->compressedBuffer = NULL;
    }

    // 5. Calcular offsets de datos (donde termina el catálogo y empiezan los bits)
    uint64_t currentOffset = (uint64_t)ftell(in);
    for (int i = 0; i < ctx->taskCount; i++) {
        FileTask *t = &ctx->tasks[i];
        t->dataOffset = currentOffset;
        currentOffset += t->compressedSize;

        // Construir ruta donde se restaurará
        if (ctx->isDirectory) {
            snprintf(t->restoredPath, sizeof(t->restoredPath), "%s/%s", ctx->basePath, t->relativePath);
        } else {
            const char *slash = strrchr(archivePath, '/');
            if (slash) {
                int dirLen = (int)(slash - archivePath);
                if (dirLen == 0) {
                    snprintf(t->restoredPath, sizeof(t->restoredPath), "/%s", t->relativePath);
                } else {
                    snprintf(t->restoredPath, sizeof(t->restoredPath), "%.*s/%s", dirLen, archivePath, t->relativePath);
                }
            } else {
                snprintf(t->restoredPath, sizeof(t->restoredPath), "%s", t->relativePath);
            }
        }
    }

    fclose(in);

    // Reconstruir árbol de Huffman y tabla de códigos en memoria compartida
    ctx->root = buildHuffmanTree(ctx->ASCIIcount);
    int bufferRuta[256];
    generarTablaCodigos(ctx->root, bufferRuta, 0, ctx->HuffmanCodesArray);

    return 0;
}

static void descomprimirFlujoDesdeOffset(FILE* inFile, FILE* outFile, uint64_t originalSize, uint64_t compressedSize, MinHeapNode* root) {
    if (root == NULL) return;

    MinHeapNode* currentNode = root;
    int bitBuffer = 0;
    int bitsLeft = 0;
    uint64_t bytesWritten = 0;
    uint64_t compressedBytesRead = 0;

    // Caso de símbolo único
    if (root->left == NULL && root->right == NULL) {
        while (bytesWritten < originalSize) {
            fputc(root->data, outFile);
            bytesWritten++;
        }
        return;
    }

    while (bytesWritten < originalSize && compressedBytesRead <= compressedSize) {
        if (bitsLeft == 0) {
            bitBuffer = fgetc(inFile);
            if (bitBuffer == EOF) break;
            compressedBytesRead++;
            bitsLeft = 8;
        }

        int bit = (bitBuffer >> 7) & 1;
        bitBuffer <<= 1;
        bitsLeft--;

        if (bit == 0) {
            currentNode = currentNode->left;
        } else {
            currentNode = currentNode->right;
        }

        if (currentNode != NULL && currentNode->left == NULL && currentNode->right == NULL) {
            fputc(currentNode->data, outFile);
            bytesWritten++;
            currentNode = root;
        }
    }
}

static int descomprimirArchivoDesdeOffset(const char *archivePath, FileTask *task, MinHeapNode *root) {
    FILE *in = fopen(archivePath, "rb");
    if (!in) {
        perror("Error al abrir archivo unificado para descompresión");
        return 0;
    }

    fseek(in, (long)task->dataOffset, SEEK_SET);

    crearDirectoriosPadre(task->restoredPath);

    FILE *out = fopen(task->restoredPath, "wb");
    if (!out) {
        perror("Error al crear archivo restaurado");
        fclose(in);
        return 0;
    }

    descomprimirFlujoDesdeOffset(in, out, task->originalSize, task->compressedSize, root);

    fclose(in);
    fclose(out);

    // Validar integridad MD5
    calcularMD5(task->restoredPath, task->md5Restored);
    if (memcmp(task->md5Original, task->md5Restored, 16) == 0) {
        task->md5Verified = 1;
        return 1;
    } else {
        task->md5Verified = 0;
        return 0;
    }
}

static void* workerDescompresion(void *arg) {
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
        if (descomprimirArchivoDesdeOffset(ctx->archivePath, task, ctx->root)) {
            pthread_mutex_lock(&ctx->statsMutex);
            ctx->totalVerifiedFiles++;
            pthread_mutex_unlock(&ctx->statsMutex);
        }
    }

    return NULL;
}

void descomprimirParaleloDesdeUnificado(SharedContext *ctx) {
    ctx->nextTaskIdx = 0;
    pthread_t *threads = (pthread_t*)malloc(ctx->numThreads * sizeof(pthread_t));
    ThreadArg *args = (ThreadArg*)malloc(ctx->numThreads * sizeof(ThreadArg));

    for (int i = 0; i < ctx->numThreads; i++) {
        args[i].threadId = i;
        args[i].ctx = ctx;
        pthread_create(&threads[i], NULL, workerDescompresion, &args[i]);
    }

    for (int i = 0; i < ctx->numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(args);

    // Si todos los archivos fueron verificados exitosamente y !keepFiles, eliminar el contenedor .huff
    if (!ctx->keepFiles && ctx->totalVerifiedFiles == ctx->taskCount) {
        remove(ctx->archivePath);
    }
}
