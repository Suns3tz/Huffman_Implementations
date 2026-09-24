/*
 * huffman.h
 * Declara estructuras y prototipos compartidos para la versión fork.
 * Autora: Cristina Urbina C.
 */

#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <openssl/md5.h>

#include "formato.h"

/* Define el nodo del árbol de Huffman */
typedef struct MinHeapNode {
    unsigned char data;
    int frequency;
    struct MinHeapNode *left;
    struct MinHeapNode *right;
} MinHeapNode;

/* Define la lista dinámica de rutas de archivos */
typedef struct {
    char **rutas;
    int cantidad;
    int capacidad;
} ListaArchivos;

/* Declara la tabla global de códigos de Huffman */
extern char *HuffmanCodesArray[256];

/* ---------- Lista de archivos ---------- */
void inicializarLista(ListaArchivos *lista);
void agregarRuta(ListaArchivos *lista, const char *ruta);
void liberarLista(ListaArchivos *lista);
void listarArchivosRecursivo(const char *path, ListaArchivos *lista);

/* ---------- Frecuencias ---------- */
void contarFrecuenciasArchivo(const char *ruta, int frecuencias[256]);

/* ---------- Árbol ---------- */
MinHeapNode* construirArbolHuffman(int frecuencias[256]);
void generarTablaCodigos(MinHeapNode *root, int arr[], int top);
void liberarArbol(MinHeapNode *root);
void limpiarTablaCodigos(void);

/* ---------- MD5 ---------- */
void calcularMD5(const char *rutaArchivo, unsigned char salida[16]);

/* ---------- Compresión / Descompresión ---------- */
void comprimirArchivo(FILE *in, FILE *out);
void descomprimirArchivo(FILE *in, FILE *out, uint64_t tamanoOriginal, MinHeapNode *root);

#endif /* HUFFMAN_H */