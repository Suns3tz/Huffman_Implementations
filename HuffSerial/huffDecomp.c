#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <openssl/md5.h>

#include "huffman.h"

// Crea directorios recursivamente (equivalente a mkdir -p)
static void crearDirectoriosRecursivo(const char *path) {
    char temp[1024];
    char *p = NULL;
    size_t len;

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);
    if (temp[len - 1] == '/') temp[len - 1] = 0;

    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
#ifdef _WIN32
            mkdir(temp);
#else
            mkdir(temp, 0755);
#endif
            *p = '/';
        }
    }
#ifdef _WIN32
    mkdir(temp);
#else
    mkdir(temp, 0755);
#endif
}

// Reconstruye la tabla de frecuencias leyendo la sección correspondiente del .huff
static void leerTablaFrecuencias(FILE *inFile, int countArray[256]) {
    memset(countArray, 0, sizeof(int) * 256);

    int simbolosPresentes = fgetc(inFile);
    if (simbolosPresentes == EOF) return;

    // Si simbolosPresentes es 0, significa que los 256 símbolos están presentes
	int total = (simbolosPresentes == 0) ? 256 : simbolosPresentes;

    for (int i = 0; i < total; i++) {
        uint8_t simbolo = (uint8_t)fgetc(inFile);
        uint32_t freq = 0;
        if (fread(&freq, sizeof(uint32_t), 1, inFile) == 1) {
            countArray[simbolo] = (int)freq;
        }
    }
}

// Extrae el contenido decodificando los bits del Árbol de Huffman
// En tu archivo de descompresión (huffDecomp.c):
static void decodificarArchivo(FILE *inFile, FILE *outFile, uint64_t tamOriginal, MinHeapNode *root, int *bitBuffer, int *bitsLeft) {
    if (!root || tamOriginal == 0) return;

    MinHeapNode *currentNode = root;
    uint64_t bytesEscritos = 0;

    // Caso especial: El árbol solo consta de 1 carácter
    if (root->left == NULL && root->right == NULL) {
        while (bytesEscritos < tamOriginal) {
            fputc(root->data, outFile);
            bytesEscritos++;
        }
        return;
    }

    while (bytesEscritos < tamOriginal) {
        if (*bitsLeft == 0) {
            *bitBuffer = fgetc(inFile);
            if (*bitBuffer == EOF) break;
            *bitsLeft = 8;
        }

        // Extrae el bit MSB actual sin alterar bitBuffer
        int bit = (*bitBuffer >> (*bitsLeft - 1)) & 1;
        (*bitsLeft)--;

        if (bit == 0) {
            currentNode = currentNode->left;
        } else {
            currentNode = currentNode->right;
        }

        if (currentNode != NULL && currentNode->left == NULL && currentNode->right == NULL) {
            fputc(currentNode->data, outFile);
            bytesEscritos++;
            currentNode = root; // Reiniciar navegación en el árbol
        }
    }
}

// Función principal para descomprimir un único paquete .huff
int descomprimirPaqueteUnico(const char *archivoHuff, const char *directorioDestino) {
    FILE *in = fopen(archivoHuff, "rb");
    if (!in) {
        perror("Error al abrir el paquete .huff");
        return 0;
    }

    // 1. Validar Encabezado
	
	int id_algoritmo_raw = fgetc(in);
    if (id_algoritmo_raw == EOF) {
        fprintf(stderr, "Error: El archivo está vacío o corrupto.\n");
        fclose(in);
        return 0;
    }
    
    char magic[4];
    if (fread(magic, 1, 4, in) != 4 || memcmp(magic, MAGIC_HEADER, 4) != 0) {
        fprintf(stderr, "Error: El archivo no es un paquete .huff válido.\n");
        fclose(in);
        return 0;
    }

    uint8_t version = (uint8_t)fgetc(in);
    if (version != 0x01) {
        fprintf(stderr, "Error: Versión no soportada.\n");
        fclose(in);
        return 0;
    }

    unsigned char md5Guardado[16];
    fread(md5Guardado, 1, 16, in);

    // 2. Leer Tabla de Frecuencias
    int frecuenciasLocales[256];
    leerTablaFrecuencias(in, frecuenciasLocales);

    MinHeapNode *root = buildHuffmanTree(frecuenciasLocales);
    if (!root) {
        fprintf(stderr, "Error al reconstruir el árbol de Huffman.\n");
        fclose(in);
        return 0;
    }

    // 3. Leer Catálogo
    uint32_t totalElementos = 0;
    fread(&totalElementos, sizeof(uint32_t), 1, in);

    ListaCatalogo cat;
    cat.cantidad = totalElementos;
    cat.elementos = (MetadatoNodo *)malloc(sizeof(MetadatoNodo) * totalElementos);

    for (uint32_t i = 0; i < totalElementos; i++) {
        cat.elementos[i].es_directorio = (uint8_t)fgetc(in);
        fread(&cat.elementos[i].len_ruta, sizeof(uint16_t), 1, in);
        memset(cat.elementos[i].ruta_relativa, 0, sizeof(cat.elementos[i].ruta_relativa));
        fread(cat.elementos[i].ruta_relativa, 1, cat.elementos[i].len_ruta, in);
        fread(&cat.elementos[i].tam_original, sizeof(uint64_t), 1, in);
        fread(cat.elementos[i].md5, 1, 16, in);
    }

    // 4. Extraer Bitstream
    int bitBuffer = 0;
    int bitsLeft = 0;

    for (uint32_t i = 0; i < totalElementos; i++) {
        
        if (cat.elementos[i].es_directorio && strchr(cat.elementos[i].ruta_relativa, '/') == NULL) {
        continue;
		}
        
        char rutaDestinoCompleta[2048];
        if (directorioDestino && strlen(directorioDestino) > 0) {
            snprintf(rutaDestinoCompleta, sizeof(rutaDestinoCompleta), "%s/%s", directorioDestino, cat.elementos[i].ruta_relativa);
        } else {
            snprintf(rutaDestinoCompleta, sizeof(rutaDestinoCompleta), "%s", cat.elementos[i].ruta_relativa);
        }

        if (cat.elementos[i].es_directorio) {
            crearDirectoriosRecursivo(rutaDestinoCompleta);
        } else {
            char *ultimoSlash = strrchr(rutaDestinoCompleta, '/');
            if (ultimoSlash) {
                *ultimoSlash = '\0';
                crearDirectoriosRecursivo(rutaDestinoCompleta);
                *ultimoSlash = '/';
            }

            FILE *out = fopen(rutaDestinoCompleta, "wb");
            if (!out) {
                perror("Error creando archivo extraído");
                continue;
            }

            decodificarArchivo(in, out, cat.elementos[i].tam_original, root, &bitBuffer, &bitsLeft);
            fflush(out);
            fclose(out);
            printf("[EXTRAÍDO] %s (%lu bytes)\n", rutaDestinoCompleta, (unsigned long)cat.elementos[i].tam_original);
        }
    }
	
    fclose(in);
    liberarArbol(root);
    free(cat.elementos);
    printf("\n¡Descompresión completada exitosamente!\n");
    return 1;
}
