#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/md5.h>

#include "huffman.h"

extern char* HuffmanCodesArray[256];
extern int ASCIIcount[256];


void initCatalogo(ListaCatalogo *cat) {
    cat->cantidad = 0;
    cat->capacidad = 16;
    cat->elementos = (MetadatoNodo*)malloc(cat->capacidad * sizeof(MetadatoNodo));
}

void agregarCatalogo(ListaCatalogo *cat, MetadatoNodo nodo) {
    if (cat->cantidad >= cat->capacidad) {
        cat->capacidad *= 2;
        cat->elementos = (MetadatoNodo*)realloc(cat->elementos, cat->capacidad * sizeof(MetadatoNodo));
    }
    cat->elementos[cat->cantidad++] = nodo;
}

void freeCatalogo(ListaCatalogo *cat) {
    free(cat->elementos);
}

//Calcular MD5 del Archivo
static int calcularMD5Archivo(const char *rutaArchivo, unsigned char *md5Out) {
    FILE *f = fopen(rutaArchivo, "rb");
    if (!f) return 0;

    MD5_CTX ctx;
    MD5_Init(&ctx);

    unsigned char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        MD5_Update(&ctx, buffer, bytes);
    }
    MD5_Final(md5Out, &ctx);

    fclose(f);
    return 1;
}


// Explorar estructura, calcular MD5s y contar frecuencias

void explorarYContar(const char *rutaBase, const char *subRuta, ListaCatalogo *cat) {
    char rutaCompleta[1024];
    if (subRuta && strlen(subRuta) > 0) {
        snprintf(rutaCompleta, sizeof(rutaCompleta), "%s/%s", rutaBase, subRuta);
    } else {
        snprintf(rutaCompleta, sizeof(rutaCompleta), "%s", rutaBase);
    }

    struct stat st;
    if (stat(rutaCompleta, &st) != 0) return;

    MetadatoNodo nodo;
    memset(&nodo, 0, sizeof(MetadatoNodo));

    // Determinar la ruta relativa a guardar en el archivo
    const char *nombreRelativo = (subRuta && strlen(subRuta) > 0) ? subRuta : strrchr(rutaBase, '/');
    if (nombreRelativo && nombreRelativo[0] == '/') nombreRelativo++;
    if (!nombreRelativo) nombreRelativo = rutaBase;

    strncpy(nodo.ruta_relativa, nombreRelativo, sizeof(nodo.ruta_relativa) - 1);
    nodo.len_ruta = (uint16_t)strlen(nodo.ruta_relativa);

    if (S_ISDIR(st.st_mode)) {
        nodo.es_directorio = 1;
        nodo.tam_original = 0;
        memset(nodo.md5, 0, 16); // Las carpetas no llevan firma MD5
        agregarCatalogo(cat, nodo);

        DIR *dir = opendir(rutaCompleta);
        if (!dir) return;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            char nuevaSubRuta[1024];
            if (subRuta && strlen(subRuta) > 0) {
                snprintf(nuevaSubRuta, sizeof(nuevaSubRuta), "%s/%s", subRuta, entry->d_name);
            } else {
                snprintf(nuevaSubRuta, sizeof(nuevaSubRuta), "%s", entry->d_name);
            }
            explorarYContar(rutaBase, nuevaSubRuta, cat);
        }
        closedir(dir);
    } else if (S_ISREG(st.st_mode)) {
        // Evitar empaquetar archivos .huff preexistentes
        if (strstr(rutaCompleta, ".huff") != NULL) return;

        nodo.es_directorio = 0;
        nodo.tam_original = st.st_size;

        //  CALCULAR MD5 ÚNICO DEL ARCHIVO ACTUAL
        if (!calcularMD5Archivo(rutaCompleta, nodo.md5)) {
            fprintf(stderr, "Advertencia: No se pudo generar MD5 para %s\n", rutaCompleta);
            memset(nodo.md5, 0, 16);
        }

        agregarCatalogo(cat, nodo);

        // Contar frecuencias para el árbol global de Huffman
        FILE *in = fopen(rutaCompleta, "rb");
        if (in) {
            unsigned char buffer[4096];
            size_t bytes;
            while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
                for (size_t i = 0; i < bytes; i++) {
                    ASCIIcount[buffer[i]]++;
                }
            }
            fclose(in);
        }
    }
}


void escribirTablaFrecuencias(FILE *outFile) {
    uint8_t simbolosPresentes = 0;
    for (int i = 0; i < 256; i++) {
        if (ASCIIcount[i] > 0) simbolosPresentes++;
    }

    fputc(simbolosPresentes, outFile);

    for (int i = 0; i < 256; i++) {
        if (ASCIIcount[i] > 0) {
            uint8_t simbolo = (uint8_t)i;
            uint32_t freq = (uint32_t)ASCIIcount[i];
            fputc(simbolo, outFile);
            fwrite(&freq, sizeof(uint32_t), 1, outFile);
        }
    }
}

void comprimirContenidoArchivo(FILE *inFile, FILE *outFile, int *bitBuffer, int *bitCount) {
    int currentByte;
    while ((currentByte = fgetc(inFile)) != EOF) {
        const char *huffCode = HuffmanCodesArray[(unsigned char)currentByte];
        if (!huffCode) continue;

        for (int i = 0; huffCode[i] != '\0'; i++) {
            *bitBuffer <<= 1;
            if (huffCode[i] == '1') {
                *bitBuffer |= 1;
            }
            (*bitCount)++;
            if (*bitCount == 8) {
                fputc(*bitBuffer, outFile);
                *bitBuffer = 0;
                *bitCount = 0;
            }
        }
    }
}


void comprimirDirectorioUnico(const char *rutaOrigen, const char *archivoSalidaHuff) {
    // 1. Asegurar extensión .huff
    char nombreFinalSalida[1024];
    size_t len = strlen(archivoSalidaHuff);

    if (len < 5 || strcmp(archivoSalidaHuff + len - 5, ".huff") != 0) {
        snprintf(nombreFinalSalida, sizeof(nombreFinalSalida), "%s.huff", archivoSalidaHuff);
    } else {
        snprintf(nombreFinalSalida, sizeof(nombreFinalSalida), "%s", archivoSalidaHuff);
    }

    ListaCatalogo cat;
    initCatalogo(&cat);

    memset(ASCIIcount, 0, sizeof(ASCIIcount));

    printf("Analizando estructura de carpetas, MD5s y frecuencias...\n");
    explorarYContar(rutaOrigen, "", &cat);

    if (cat.cantidad == 0) {
        printf("Error: No se encontraron elementos válidos para comprimir.\n");
        freeCatalogo(&cat);
        return;
    }

    MinHeapNode* root = buildHuffmanTree(ASCIIcount);
    int bitArray[256];
    generarTablaCodigos(root, bitArray, 0);

    FILE *out = fopen(nombreFinalSalida, "wb");
    if (!out) {
        perror("Error al crear el paquete .huff");
        freeCatalogo(&cat);
        liberarArbol(root);
        return;
    }

    // 1. ENCABEZADO GLOBAL ( ID + Magic "HUFF" + Versión 0x01)
    
    
    uint8_t id_algoritmo = 1;
    fputc(id_algoritmo, out);
    
    fwrite(MAGIC_HEADER, 1, 4, out);
    uint8_t version = 0x01;
    fputc(version, out);

    // Reservar 16 bytes vacíos para el Hash MD5 global
    unsigned char md5Dummy[16] = {0};
    fwrite(md5Dummy, 1, 16, out);

    // 2. TABLA DE FRECUENCIAS
    escribirTablaFrecuencias(out);

    // 3. CATÁLOGO / DIRECTORIO DE METADATOS
    uint32_t totalElementos = (uint32_t)cat.cantidad;
    fwrite(&totalElementos, sizeof(uint32_t), 1, out);

    for (size_t i = 0; i < cat.cantidad; i++) {
        fputc(cat.elementos[i].es_directorio, out);
        fwrite(&cat.elementos[i].len_ruta, sizeof(uint16_t), 1, out);
        fwrite(cat.elementos[i].ruta_relativa, 1, cat.elementos[i].len_ruta, out);
        fwrite(&cat.elementos[i].tam_original, sizeof(uint64_t), 1, out);
        fwrite(cat.elementos[i].md5, 1, 16, out); 
    }

    // 4. CUERPO DE DATOS COMPRIMIDOS (Bitstream)
    int bitBuffer = 0;
    int bitCount = 0;

    for (size_t i = 0; i < cat.cantidad; i++) {
        if (!cat.elementos[i].es_directorio) {
            char rutaFisica[2048];

            if (strncmp(cat.elementos[i].ruta_relativa, rutaOrigen, strlen(rutaOrigen)) == 0) {
                snprintf(rutaFisica, sizeof(rutaFisica), "%s", cat.elementos[i].ruta_relativa);
            } else {
                snprintf(rutaFisica, sizeof(rutaFisica), "%s/%s", rutaOrigen, cat.elementos[i].ruta_relativa);
            }

            FILE *in = fopen(rutaFisica, "rb");
            if (!in) {
                fprintf(stderr, "Error: No se pudo abrir '%s' para comprimir\n", rutaFisica);
                continue;
            }

            comprimirContenidoArchivo(in, out, &bitBuffer, &bitCount);
            fclose(in);
        }
    }

    // Flush de los bits restantes
    if (bitCount > 0) {
        bitBuffer <<= (8 - bitCount);
        fputc(bitBuffer, out);
    }

    fclose(out);

    liberarArbol(root);
    limpiarTablaCodigos();
    freeCatalogo(&cat);

    printf("¡Empaquetado y compresión completados con éxito en '%s'!\n", nombreFinalSalida);
}
