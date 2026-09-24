/*
 * formato.c
 * Implementa el formato binario .huff.
 * Autora: Cristina Urbina C.
 */

#include "formato.h"

void escribirModo(FILE *out, uint8_t modo) {
    fwrite(&modo, sizeof(uint8_t), 1, out);
}

uint8_t leerModo(FILE *in) {
    uint8_t modo = 0;
    fread(&modo, sizeof(uint8_t), 1, in);
    return modo;
}

void escribirFrecuencias(FILE *out, int frecuencias[256]) {
    fwrite(frecuencias, sizeof(int), 256, out);
}

void leerFrecuencias(FILE *in, int frecuencias[256]) {
    fread(frecuencias, sizeof(int), 256, in);
}

void escribirCantidadArchivos(FILE *out, uint32_t cantidad) {
    fwrite(&cantidad, sizeof(uint32_t), 1, out);
}

uint32_t leerCantidadArchivos(FILE *in) {
    uint32_t cantidad = 0;
    fread(&cantidad, sizeof(uint32_t), 1, in);
    return cantidad;
}

void escribirMetadataArchivo(FILE *out, const char *nombre, uint64_t tamanoOriginal, 
            uint64_t tamanoComprimido, const unsigned char md5[16]) {
    uint16_t longitud = (uint16_t)strlen(nombre);
    fwrite(&longitud, sizeof(uint16_t), 1, out);
    fwrite(nombre, sizeof(char), longitud, out);
    fwrite(&tamanoOriginal, sizeof(uint64_t), 1, out);
    fwrite(&tamanoComprimido, sizeof(uint64_t), 1, out);
    fwrite(md5, sizeof(unsigned char), 16, out);
}

void leerMetadataArchivo(FILE *in, char *nombreSalida, uint64_t *tamanoOriginal,
            uint64_t *tamanoComprimido, unsigned char md5[16]) {
    uint16_t longitud = 0;
    fread(&longitud, sizeof(uint16_t), 1, in);
    fread(nombreSalida, sizeof(char), longitud, in);
    nombreSalida[longitud] = '\0';
    fread(tamanoOriginal, sizeof(uint64_t), 1, in);
    fread(tamanoComprimido, sizeof(uint64_t), 1, in);
    fread(md5, sizeof(unsigned char), 16, in);
}