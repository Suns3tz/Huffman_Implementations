/*
 * formato.h
 * Define el formato binario del archivo .huff.
 * Autora: Cristina Urbina C.
 *
 * Estructura del archivo .huff:
 *   [1 byte ]  modo (1=serial, 2=fork, 3=pthread)
 *   [256 int]  tabla de frecuencias (256 * sizeof(int) bytes)
 *   [4 bytes]  cantidad de archivos (uint32_t)
 *   [Por cada archivo]
 *       [2 bytes]  longitud del nombre (uint16_t)
 *       [N bytes]  nombre del archivo (sin '\0')
 *       [8 bytes]  tamaño original descomprimido (uint64_t)
 *       [8 bytes]  tamaño comprimido (uint64_t)
 *       [16 bytes] MD5 del archivo original
 *   [Datos comprimidos concatenados de todos los archivos]
 */

#ifndef FORMATO_H
#define FORMATO_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODO_SERIAL    1
#define MODO_FORK      2
#define MODO_PTHREAD   3

void escribirModo(FILE *out, uint8_t modo);
uint8_t leerModo(FILE *in);
void escribirFrecuencias(FILE *out, int frecuencias[256]);
void leerFrecuencias(FILE *in, int frecuencias[256]);
void escribirCantidadArchivos(FILE *out, uint32_t cantidad);
uint32_t leerCantidadArchivos(FILE *in);

void escribirMetadataArchivo(FILE *out, const char *nombre, uint64_t tamanoOriginal, 
    uint64_t tamanoComprimido, const unsigned char md5[16]);

void leerMetadataArchivo(FILE *in, char *nombreSalida, uint64_t *tamanoOriginal, 
    uint64_t *tamanoComprimido, unsigned char md5[16]);

#endif /* FORMATO_H */