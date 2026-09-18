#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/md5.h> // Para verificación MD5 (-lcrypto)

#include "huffman.h"



// Lee el tamaño original de 3 bytes desde el encabezado .huff
long readHeader3Byte(FILE* archivo) {
    unsigned char byte1 = fgetc(archivo);
    unsigned char byte2 = fgetc(archivo);
    unsigned char byte3 = fgetc(archivo);
    return ((long)byte1 << 16) | ((long)byte2 << 8) | byte3;
}

// Extrae los 16 bytes de la firma MD5 guardados en los metadatos .huff
void leerMD5Header(FILE* archivo, unsigned char* md5Guardado) {
    fread(md5Guardado, 1, 16, archivo);
}

// Calcula la firma MD5 de un archivo en el sistema de archivos
void calcularMD5Archivo(const char *rutaArchivo, unsigned char *output) {
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

// Recorre el árbol de Huffman procesando los bits de entrada
void Decompress(FILE* inFile, FILE* outFile, long originalSize, MinHeapNode* root) {
    if (root == NULL) return;

    MinHeapNode* currentNode = root;
    int bitBuffer = 0;
    int bitsLeft = 0;
    long bytesWritten = 0;

    // Si el árbol sólo consta de una hoja (ej. archivo de un solo tipo de carácter)
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

// Quita la extensión .huff para restaurar el nombre original
void restaurarNombreOriginal(const char* nombreHuff, char* nombreSalida) {
    strcpy(nombreSalida, nombreHuff);
    char *extension = strstr(nombreSalida, ".huff");
    if (extension != NULL) {
        *extension = '\0';
    } else {
        strcat(nombreSalida, ".out");
    }
}

// Descomprime un archivo individual y retorna 1 si la firma MD5 es válida, 0 si falla
int descomprimirArchivoIndividual(const char* rutaArchivoHuff, MinHeapNode* root) {
    FILE *IN = fopen(rutaArchivoHuff, "rb");
    if (IN == NULL) {
        perror("Error abriendo archivo .huff");
        return 0;
    }

    // 1. Leer tamaño original (3 bytes)
    long originalSize = readHeader3Byte(IN);

    // 2. Leer metadatos del MD5 original (16 bytes)
    unsigned char md5Original[16];
    leerMD5Header(IN, md5Original);

    // 3. Crear archivo de salida sin extensión .huff
    char rutaRestaurada[1024];
    restaurarNombreOriginal(rutaArchivoHuff, rutaRestaurada);

    FILE *OUT = fopen(rutaRestaurada, "wb");
    if (OUT == NULL) {
        perror("Error al crear archivo de salida descomprimido");
        fclose(IN);
        return 0;
    }

    // 4. Decodificar flujo de bits
    Decompress(IN, OUT, originalSize, root);

    fclose(IN);
    fclose(OUT);

    // 5. Verificar salud de compresión recalculando el MD5 del archivo generado
    unsigned char md5Calculado[16];
    calcularMD5Archivo(rutaRestaurada, md5Calculado);

    if (memcmp(md5Original, md5Calculado, 16) == 0) {
        printf("[OK] Archivo restaurado e integridad MD5 verificada: %s\n", rutaRestaurada);
        return 1; 
    } else {
        printf("[ERROR] Fallo de integridad MD5 en el archivo: %s\n", rutaRestaurada);
        return 0;
    }
}

// Recorre un directorio buscando y descomprimiendo todos los archivos .huff
void descomprimirRutaRecursiva(const char *path, MinHeapNode* root, int *verificados, int *totales) {
    struct stat st;
    if (stat(path, &st) != 0) return;

    if (S_ISREG(st.st_mode)) {
        if (strstr(path, ".huff") != NULL) {
            (*totales)++;
            if (descomprimirArchivoIndividual(path, root)) {
                (*verificados)++;
            }
        }
    } 
    else if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (dir == NULL) return;

        struct dirent *entry;
        char subPath[1024];

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            snprintf(subPath, sizeof(subPath), "%s/%s", path, entry->d_name);
            descomprimirRutaRecursiva(subPath, root, verificados, totales);
        }
        closedir(dir);
    }
}
