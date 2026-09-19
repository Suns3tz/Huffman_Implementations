#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>

#include "huffman_pthread.h"

int inicializarContexto(SharedContext *ctx, int numThreads) {
    ctx->taskCapacity = 64;
    ctx->taskCount = 0;
    ctx->nextTaskIdx = 0;
    ctx->tasks = (FileTask*)malloc(ctx->taskCapacity * sizeof(FileTask));
    if (!ctx->tasks) {
        perror("Error al asignar memoria para las tareas");
        return -1;
    }

    memset(ctx->ASCIIcount, 0, sizeof(ctx->ASCIIcount));
    for (int i = 0; i < 256; i++) {
        ctx->HuffmanCodesArray[i] = NULL;
    }
    ctx->root = NULL;

    ctx->totalOriginalBytes = 0;
    ctx->totalCompressedBytes = 0;
    ctx->totalFilesProcessed = 0;
    ctx->totalVerifiedFiles = 0;
    ctx->numThreads = numThreads;

    pthread_mutex_init(&ctx->queueMutex, NULL);
    pthread_mutex_init(&ctx->freqMutex, NULL);
    pthread_mutex_init(&ctx->statsMutex, NULL);

    return 0;
}

void liberarContexto(SharedContext *ctx) {
    if (ctx->tasks) {
        free(ctx->tasks);
        ctx->tasks = NULL;
    }
    pthread_mutex_destroy(&ctx->queueMutex);
    pthread_mutex_destroy(&ctx->freqMutex);
    pthread_mutex_destroy(&ctx->statsMutex);
}

static void agregarTarea(SharedContext *ctx, const char *path) {
    if (ctx->taskCount >= ctx->taskCapacity) {
        ctx->taskCapacity *= 2;
        FileTask *temp = (FileTask*)realloc(ctx->tasks, ctx->taskCapacity * sizeof(FileTask));
        if (!temp) {
            perror("Error al redimensionar cola de tareas");
            return;
        }
        ctx->tasks = temp;
    }

    FileTask *t = &ctx->tasks[ctx->taskCount];
    memset(t, 0, sizeof(FileTask));
    strncpy(t->originalPath, path, sizeof(t->originalPath) - 1);

    size_t len = strlen(path);
    if (len >= 5 && strcmp(path + len - 5, ".huff") == 0) {
        t->isHuff = 1;
        strncpy(t->compressedPath, path, sizeof(t->compressedPath) - 1);
        restaurarNombreSinHuff(path, t->restoredPath);
    } else {
        t->isHuff = 0;
        cambiarExtensionAHuff(path, t->compressedPath);
        snprintf(t->restoredPath, sizeof(t->restoredPath), "%s.restored", path);
    }

    ctx->taskCount++;
}

void escanearRutaRecursiva(const char *path, SharedContext *ctx) {
    struct stat st;
    if (stat(path, &st) != 0) {
        perror("Error al obtener estado de la ruta");
        return;
    }

    if (S_ISREG(st.st_mode)) {
        agregarTarea(ctx, path);
    } else if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) {
            perror("Error al abrir directorio");
            return;
        }

        struct dirent *entry;
        char subPath[1024];

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            snprintf(subPath, sizeof(subPath), "%s/%s", path, entry->d_name);
            escanearRutaRecursiva(subPath, ctx);
        }
        closedir(dir);
    }
}

// Función ejecutada por cada hilo para contar frecuencias
static void* workerFrecuencias(void *arg) {
    ThreadArg *tArg = (ThreadArg*)arg;
    SharedContext *ctx = tArg->ctx;

    // Arreglo local privado para evitar false sharing y contención de mutex
    long localFreq[256];
    memset(localFreq, 0, sizeof(localFreq));

    while (1) {
        int taskIdx = -1;

        // Sección crítica para tomar la siguiente tarea de la cola en memoria compartida
        pthread_mutex_lock(&ctx->queueMutex);
        if (ctx->nextTaskIdx < ctx->taskCount) {
            taskIdx = ctx->nextTaskIdx++;
        }
        pthread_mutex_unlock(&ctx->queueMutex);

        if (taskIdx == -1) {
            break; // No hay más tareas
        }

        FileTask *task = &ctx->tasks[taskIdx];
        if (task->isHuff) {
            continue; // Los archivos ya comprimidos no se incluyen en el conteo de frecuencias
        }

        FILE *f = fopen(task->originalPath, "rb");
        if (!f) continue;

        unsigned char buffer[4096];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
            for (size_t i = 0; i < bytes; i++) {
                localFreq[buffer[i]]++;
            }
        }
        fclose(f);
    }

    // Consolidación en la tabla global compartida protegida por freqMutex
    pthread_mutex_lock(&ctx->freqMutex);
    for (int i = 0; i < 256; i++) {
        ctx->ASCIIcount[i] += localFreq[i];
    }
    pthread_mutex_unlock(&ctx->freqMutex);

    return NULL;
}

void calcularFrecuenciasParalelo(SharedContext *ctx) {
    ctx->nextTaskIdx = 0;
    pthread_t *threads = (pthread_t*)malloc(ctx->numThreads * sizeof(pthread_t));
    ThreadArg *args = (ThreadArg*)malloc(ctx->numThreads * sizeof(ThreadArg));

    for (int i = 0; i < ctx->numThreads; i++) {
        args[i].threadId = i;
        args[i].ctx = ctx;
        pthread_create(&threads[i], NULL, workerFrecuencias, &args[i]);
    }

    for (int i = 0; i < ctx->numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(args);
}

