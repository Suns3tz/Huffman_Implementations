#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "huffman_pthread.h"

int inicializarContexto(SharedContext *ctx, int numThreads, int keepFiles, const char *ruta) {
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
    ctx->archiveFileSize = 0;
    ctx->totalFilesProcessed = 0;
    ctx->totalVerifiedFiles = 0;
    ctx->implementationId = HUFF_IMPLEMENTATION_ID;
    ctx->numThreads = numThreads;
    ctx->keepFiles = keepFiles;

    // Normalizar ruta
    char rutaLimpia[1024];
    strncpy(rutaLimpia, ruta, sizeof(rutaLimpia) - 1);
    rutaLimpia[sizeof(rutaLimpia) - 1] = '\0';
    size_t len = strlen(rutaLimpia);
    while (len > 1 && rutaLimpia[len - 1] == '/') {
        rutaLimpia[len - 1] = '\0';
        len--;
    }

    snprintf(ctx->originalTarget, sizeof(ctx->originalTarget), "%s", rutaLimpia);

    if (len >= 5 && strcmp(rutaLimpia + len - 5, ".huff") == 0) {
        snprintf(ctx->archivePath, sizeof(ctx->archivePath), "%s", rutaLimpia);
        snprintf(ctx->basePath, sizeof(ctx->basePath), "%.*s", (int)(len - 5), rutaLimpia);
        ctx->isDirectory = 0; // Se actualizará al leer el catálogo del archivo .huff
    } else {
        struct stat st;
        if (stat(rutaLimpia, &st) == 0 && S_ISDIR(st.st_mode)) {
            ctx->isDirectory = 1;
        } else {
            ctx->isDirectory = 0;
        }
        snprintf(ctx->archivePath, sizeof(ctx->archivePath), "%s.huff", rutaLimpia);
        snprintf(ctx->basePath, sizeof(ctx->basePath), "%s", rutaLimpia);
    }

    pthread_mutex_init(&ctx->queueMutex, NULL);
    pthread_mutex_init(&ctx->freqMutex, NULL);
    pthread_mutex_init(&ctx->statsMutex, NULL);

    return 0;
}

void liberarContexto(SharedContext *ctx) {
    if (ctx->tasks) {
        for (int i = 0; i < ctx->taskCount; i++) {
            if (ctx->tasks[i].compressedBuffer) {
                free(ctx->tasks[i].compressedBuffer);
                ctx->tasks[i].compressedBuffer = NULL;
            }
        }
        free(ctx->tasks);
        ctx->tasks = NULL;
    }
    pthread_mutex_destroy(&ctx->queueMutex);
    pthread_mutex_destroy(&ctx->freqMutex);
    pthread_mutex_destroy(&ctx->statsMutex);
}

static void agregarTarea(SharedContext *ctx, const char *path, const char *basePath) {
    if (strcmp(path, ctx->archivePath) == 0) {
        return;
    }

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

    if (!ctx->isDirectory) {
        // Archivo solitario: el nombre relativo es el nombre del archivo sin rutas
        const char *slash = strrchr(path, '/');
        const char *fname = slash ? slash + 1 : path;
        strncpy(t->relativePath, fname, sizeof(t->relativePath) - 1);
        strncpy(t->restoredPath, path, sizeof(t->restoredPath) - 1);
    } else {
        // Carpeta: calcular ruta relativa respecto a basePath
        size_t baseLen = strlen(basePath);
        if (strncmp(path, basePath, baseLen) == 0) {
            const char *rel = path + baseLen;
            while (*rel == '/') rel++;
            if (*rel != '\0') {
                strncpy(t->relativePath, rel, sizeof(t->relativePath) - 1);
            } else {
                const char *slash = strrchr(path, '/');
                strncpy(t->relativePath, slash ? slash + 1 : path, sizeof(t->relativePath) - 1);
            }
        } else {
            const char *slash = strrchr(path, '/');
            strncpy(t->relativePath, slash ? slash + 1 : path, sizeof(t->relativePath) - 1);
        }
        snprintf(t->restoredPath, sizeof(t->restoredPath), "%s/%s", ctx->basePath, t->relativePath);
    }

    t->compressedBuffer = NULL;
    ctx->taskCount++;
}

void escanearRutaRecursiva(const char *path, SharedContext *ctx, const char *basePath) {
    struct stat st;
    if (stat(path, &st) != 0) {
        perror("Error al obtener estado de la ruta");
        return;
    }

    if (S_ISREG(st.st_mode)) {
        agregarTarea(ctx, path, basePath);
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
            escanearRutaRecursiva(subPath, ctx, basePath);
        }
        closedir(dir);
    }
}

static void* workerFrecuencias(void *arg) {
    ThreadArg *tArg = (ThreadArg*)arg;
    SharedContext *ctx = tArg->ctx;

    long localFreq[256];
    memset(localFreq, 0, sizeof(localFreq));

    while (1) {
        int taskIdx = -1;

        pthread_mutex_lock(&ctx->queueMutex);
        if (ctx->nextTaskIdx < ctx->taskCount) {
            taskIdx = ctx->nextTaskIdx++;
        }
        pthread_mutex_unlock(&ctx->queueMutex);

        if (taskIdx == -1) break;

        FileTask *task = &ctx->tasks[taskIdx];
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

void crearDirectoriosPadre(const char *filePath) {
    char temp[1024];
    strncpy(temp, filePath, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *slash = strrchr(temp, '/');
    if (!slash) return;
    *slash = '\0';

    for (char *p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(temp, 0777);
            *p = '/';
        }
    }
    mkdir(temp, 0777);
}

void eliminarRutaRecursiva(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return;

    if (S_ISREG(st.st_mode)) {
        remove(path);
    } else if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) return;

        struct dirent *entry;
        char subPath[1024];
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            snprintf(subPath, sizeof(subPath), "%s/%s", path, entry->d_name);
            eliminarRutaRecursiva(subPath);
        }
        closedir(dir);
        rmdir(path);
    }
}
