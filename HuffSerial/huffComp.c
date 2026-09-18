#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/md5.h> // Necesario para MD5 (-lcrypto al compilar)

#include "huffman.h"

extern char* HuffmanCodesArray[256];

// Calcula los 16 bytes de la firma MD5 de un archivo
void calcularMD5(const char *rutaArchivo, unsigned char *output) {
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

long countC(FILE* archivo) {
    fseek(archivo, 0, SEEK_END);
    long size = ftell(archivo);
    fseek(archivo, 0, SEEK_SET); 
    return size;
}

// Imprime la tabla comparativa incluyendo el Hash MD5 formateado a Hexadecimal
void writeCompressionTable(const char* originalFileName, long originalSize, long compressedSize, const unsigned char* md5) {
    double compressionPercentage = 0.0;
    if (originalSize > 0) {
        compressionPercentage = 100.0 * (1.0 - ((double)compressedSize / originalSize));
    }

    char md5String[33];
    for (int i = 0; i < 16; i++) {
        sprintf(&md5String[i * 2], "%02x", md5[i]);
    }

    printf("| %-20s | %-32s | %-10ld | %-10ld | %6.2f%% |\n", 
            originalFileName, md5String, originalSize, compressedSize, compressionPercentage);
    printf("----------------------------------------------------------------------------------------------------\n");
}

void writeheader3byte(FILE* archivo, long cant) {
    unsigned char byte1 = (cant >> 16) & 0xFF;
    unsigned char byte2 = (cant >> 8) & 0xFF;
    unsigned char byte3 =  cant & 0xFF;   
    fputc(byte1, archivo);
    fputc(byte2, archivo);
    fputc(byte3, archivo);
}

void changeExtension(const char* OgName, char* NewName) {
    strcpy(NewName, OgName);
    char *extension = strrchr(NewName, '.');
    if (extension != NULL) {
        *extension = '\0';
    }
    strcat(NewName, ".huff");
}

void Compress(FILE* inFile, FILE* outFile) {
    int bitBuffer = 0;  
    int bitCount = 0;   
    int currentByte;
    
    while ((currentByte = fgetc(inFile)) != EOF) {
        const char *huffCode = HuffmanCodesArray[(unsigned char)currentByte];

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

void comprimirArchivoIndividual(const char* rutaArchivo) {
    FILE *IN = fopen(rutaArchivo, "rb");
    if (IN == NULL) {
        perror("Error al abrir archivo para comprimir");
        return;
    }

    char NewName[1024];
    changeExtension(rutaArchivo, NewName);

    FILE *OUT = fopen(NewName, "wb");
    if (OUT == NULL) {
        perror("Error al crear archivo comprimido .huff");
        fclose(IN);
        return;
    }

    long originalSize = countC(IN);

    // 1. Escribir tamaño de archivo (3 bytes de encabezado)
    writeheader3byte(OUT, originalSize);

    // 2. Calcular y ESCRIBIR LA FIRMA MD5 (16 bytes) EN LOS METADATOS DEL ARCHIVO
    unsigned char md5Hash[16];
    calcularMD5(rutaArchivo, md5Hash);
    fwrite(md5Hash, 1, 16, OUT);

    // 3. Comprimir contenido
    rewind(IN);
    Compress(IN, OUT);

    long newSize = countC(OUT);

    fclose(IN);
    fclose(OUT);

    writeCompressionTable(rutaArchivo, originalSize, newSize, md5Hash);
}

void comprimirRutaRecursiva(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return;

    if (S_ISREG(st.st_mode)) {
        if (strstr(path, ".huff") == NULL) {
            comprimirArchivoIndividual(path);
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
            comprimirRutaRecursiva(subPath);
        }
        closedir(dir);
    }
}
