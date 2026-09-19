#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <openssl/md5.h>

#include "huffman_pthread.h"

static long leerCabecera3Bytes(FILE* archivo) {
    unsigned char byte1 = fgetc(archivo);
    unsigned char byte2 = fgetc(archivo);
    unsigned char byte3 = fgetc(archivo);
    return ((long)byte1 << 16) | ((long)byte2 << 8) | byte3;
}

static void leerMD5Header(FILE* archivo, unsigned char* md5Guardado) {
    if (fread(md5Guardado, 1, 16, archivo) != 16) {
        memset(md5Guardado, 0, 16);
    }
}

void restaurarNombreSinHuff(const char* huffName, char* restoredName) {
    strcpy(restoredName, huffName);
    char *extension = strstr(restoredName, ".huff");
    if (extension != NULL) {
        *extension = '\0';
    } else {
        strcat(restoredName, ".out");
    }
}

static void descomprimirFlujo(FILE* inFile, FILE* outFile, long originalSize, MinHeapNode* root) {
    if (root == NULL) return;

    MinHeapNode* currentNode = root;
    int bitBuffer = 0;
    int bitsLeft = 0;
    long bytesWritten = 0;

    // Caso de un solo símbolo único
    if (root->left == NULL && root->right == NULL) {
        while (bytesWritten < originalSize) {
            fputc(root->data, outFile);
            bytesWritten++;
        }
        return;
    }

    while (bytesWritten < originalSize) {
        if (bitsLeft == 0) {
            bitBuffer = fgetc(inFile);
            if (bitBuffer == EOF) break;
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

int descomprimirArchivoIndividualPthread(FileTask *task, MinHeapNode* root) {
    FILE *in = fopen(task->compressedPath, "rb");
    if (!in) {
        perror("Error al abrir archivo comprimido para descompresión");
        return 0;
    }

    // 1. Leer tamaño original (3 bytes)
    long originalSize = leerCabecera3Bytes(in);

    // 2. Leer firma MD5 original (16 bytes)
    leerMD5Header(in, task->md5Original);

    // 3. Crear archivo de salida para restauración
    FILE *out = fopen(task->restoredPath, "wb");
    if (!out) {
        perror("Error al crear archivo restaurado");
        fclose(in);
        return 0;
    }

    // 4. Decodificar flujo de bits con el árbol de Huffman compartido
    descomprimirFlujo(in, out, originalSize, root);

    fclose(in);
    fclose(out);

    // 5. Verificar integridad recalculando MD5 del archivo restaurado
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
        if (!task->isHuff && task->compressedSize == 0) continue;

        if (descomprimirArchivoIndividualPthread(task, ctx->root)) {
            pthread_mutex_lock(&ctx->statsMutex);
            ctx->totalVerifiedFiles++;
            pthread_mutex_unlock(&ctx->statsMutex);
        }
    }

    return NULL;
}

void descomprimirParalelo(SharedContext *ctx) {
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
}

