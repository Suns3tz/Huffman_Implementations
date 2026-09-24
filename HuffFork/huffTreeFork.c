/*
 * huffTreeFork.c
 * Construye el árbol de Huffman y comprime/descomprime un solo archivo.
 * Autora: Cristina Urbina C.
 */

#include "huffman.h"

char *HuffmanCodesArray[256] = {NULL};

typedef struct MinHeap {
    MinHeapNode **elements;
    unsigned size;
    unsigned capacity;
} MinHeap;

static MinHeapNode* nuevoNodo(unsigned char data, int freq) {
    MinHeapNode *t = (MinHeapNode*)malloc(sizeof(MinHeapNode));
    t->left = t->right = NULL;
    t->data = data;
    t->frequency = freq;
    return t;
}

static MinHeap* crearMinHeap(unsigned cap) {
    MinHeap *h = (MinHeap*)malloc(sizeof(MinHeap));
    h->size = 0;
    h->capacity = cap;
    h->elements = (MinHeapNode**)malloc(cap * sizeof(MinHeapNode*));
    return h;
}

static void swap(MinHeapNode **a, MinHeapNode **b) {
    MinHeapNode *t = *a; *a = *b; *b = t;
}

static void siftDown(MinHeap *h, int pos) {
    while (2*pos+1 < (int)h->size) {
        int smallest = 2*pos+1;
        if (smallest+1 < (int)h->size &&
            h->elements[smallest+1]->frequency < h->elements[smallest]->frequency)
            smallest++;
        if (h->elements[pos]->frequency <= h->elements[smallest]->frequency) break;
        swap(&h->elements[pos], &h->elements[smallest]);
        pos = smallest;
    }
}

static MinHeapNode* extraerMin(MinHeap *h) {
    MinHeapNode *t = h->elements[0];
    h->elements[0] = h->elements[h->size-1];
    h->size--;
    siftDown(h, 0);
    return t;
}

static void insertarMinHeap(MinHeap *h, MinHeapNode *n) {
    h->size++;
    int i = h->size - 1;
    while (i && n->frequency < h->elements[(i-1)/2]->frequency) {
        h->elements[i] = h->elements[(i-1)/2];
        i = (i-1)/2;
    }
    h->elements[i] = n;
}

static void buildMinHeap(MinHeap *h) {
    for (int i = (h->size-1)/2; i >= 0; --i) siftDown(h, i);
}

static int esHoja(MinHeapNode *r) {
    return !(r->left) && !(r->right);
}

MinHeapNode* construirArbolHuffman(int frecuencias[256]) {
    MinHeap *h = crearMinHeap(256);
    for (int i = 0; i < 256; i++) {
        if (frecuencias[i] > 0) {
            h->elements[h->size++] = nuevoNodo((unsigned char)i, frecuencias[i]);
        }
    }
    if (h->size == 0) {
        free(h->elements); free(h);
        return NULL;
    }
    buildMinHeap(h);
    while (h->size > 1) {
        MinHeapNode *l = extraerMin(h);
        MinHeapNode *r = extraerMin(h);
        MinHeapNode *t = nuevoNodo('$', l->frequency + r->frequency);
        t->left = l; t->right = r;
        insertarMinHeap(h, t);
    }
    MinHeapNode *root = extraerMin(h);
    free(h->elements); free(h);
    return root;
}

static void storeCode(int arr[], int n, unsigned char sym) {
    char *code = (char*)malloc(n+1);
    for (int i = 0; i < n; i++) code[i] = '0' + arr[i];
    code[n] = '\0';
    HuffmanCodesArray[sym] = code;
}

void generarTablaCodigos(MinHeapNode *root, int arr[], int top) {
    if (!root) return;
    if (root->left) { arr[top] = 0; generarTablaCodigos(root->left, arr, top+1); }
    if (root->right) { arr[top] = 1; generarTablaCodigos(root->right, arr, top+1); }
    if (esHoja(root)) storeCode(arr, top, root->data);
}

void liberarArbol(MinHeapNode *root) {
    if (!root) return;
    liberarArbol(root->left);
    liberarArbol(root->right);
    free(root);
}

void limpiarTablaCodigos(void) {
    for (int i = 0; i < 256; i++) {
        if (HuffmanCodesArray[i]) { free(HuffmanCodesArray[i]); HuffmanCodesArray[i] = NULL; }
    }
}

/* ---------- Compresión y descompresión de un solo archivo ---------- */

void comprimirArchivo(FILE *in, FILE *out) {
    int bitBuffer = 0, bitCount = 0, c;

    while ((c = fgetc(in)) != EOF) {
        const char *code = HuffmanCodesArray[(unsigned char)c];
        if (!code) continue;

        for (int i = 0; code[i]; i++) {
            bitBuffer <<= 1;

            if (code[i] == '1') bitBuffer |= 1;
            bitCount++;

            if (bitCount == 8) {
                fputc(bitBuffer, out);
                bitBuffer = 0; bitCount = 0;
            }
        }
    }
    if (bitCount > 0) {
        bitBuffer <<= (8 - bitCount);
        fputc(bitBuffer, out);
    }
}

void descomprimirArchivo(FILE *in, FILE *out, uint64_t tamanoOriginal, MinHeapNode *root) {
    if (!root) return;
    MinHeapNode *actual = root;
    int bitBuffer = 0, bitsLeft = 0;
    uint64_t bytesEscritos = 0;

    if (!root->left && !root->right) {
        while (bytesEscritos < tamanoOriginal) {
            fputc(root->data, out);
            bytesEscritos++;
        }
        return;
    }

    while (bytesEscritos < tamanoOriginal) {
        if (bitsLeft == 0) {
            bitBuffer = fgetc(in);
            if (bitBuffer == EOF) break;
            bitsLeft = 8;
        }

        int bit = (bitBuffer >> 7) & 1;
        bitBuffer <<= 1;
        bitsLeft--;

        if (bit == 0) {
            actual = actual->left;
        }
        else{
            actual = actual->right;
        }

        if (actual && !actual->left && !actual->right) {
            fputc(actual->data, out);
            bytesEscritos++;
            actual = root;
        }
    }
}