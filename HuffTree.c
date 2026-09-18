#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "huffman.h"

// Tabla global donde se almacenarán las cadenas de bits ('0' y '1') asignadas a cada byte
char* HuffmanCodesArray[256] = {NULL};

// Estructura del nodo del árbol y MinHeap


typedef struct MinHeap {
    MinHeapNode** elements;
    unsigned size;
    unsigned capacity;
} MinHeap;

// Funciones de creación de nodos y MinHeap
MinHeapNode* newNode(unsigned char data, int freq) {
    MinHeapNode* temp = (MinHeapNode*)malloc(sizeof(MinHeapNode));
    temp->left = temp->right = NULL;
    temp->data = data;
    temp->frequency = freq;
    return temp;
}

MinHeap* createMinHeap(unsigned capacity) {
    MinHeap* minHeap = (MinHeap*)malloc(sizeof(MinHeap));
    minHeap->size = 0;
    minHeap->capacity = capacity;
    minHeap->elements = (MinHeapNode**)malloc(capacity * sizeof(MinHeapNode*));
    return minHeap;
}

void swapMinHeapNode(MinHeapNode** a, MinHeapNode** b) {
    MinHeapNode* t = *a;
    *a = *b;
    *b = t;
}

void siftDown(MinHeap* minHeap, int pos) {
    while (2 * pos + 1 < minHeap->size) {
        int smallest = 2 * pos + 1;
        if (smallest + 1 < minHeap->size && minHeap->elements[smallest + 1]->frequency < minHeap->elements[smallest]->frequency) {
            smallest++;
        }
        if (minHeap->elements[pos]->frequency <= minHeap->elements[smallest]->frequency) {
            break;
        }
        swapMinHeapNode(&minHeap->elements[pos], &minHeap->elements[smallest]);
        pos = smallest;
    }
}

int isSizeOne(MinHeap* minHeap) {
    return (minHeap->size == 1);
}

MinHeapNode* extractMin(MinHeap* minHeap) {
    MinHeapNode* temp = minHeap->elements[0];
    minHeap->elements[0] = minHeap->elements[minHeap->size - 1];
    minHeap->size--;
    siftDown(minHeap, 0);
    return temp;
}

void insertMinHeap(MinHeap* minHeap, MinHeapNode* minHeapNode) {
    minHeap->size++;
    int i = minHeap->size - 1;
    while (i && minHeapNode->frequency < minHeap->elements[(i - 1) / 2]->frequency) {
        minHeap->elements[i] = minHeap->elements[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap->elements[i] = minHeapNode;
}

void buildMinHeap(MinHeap* minHeap) {
    for (int i = (minHeap->size - 1) / 2; i >= 0; --i) {
        siftDown(minHeap, i);
    }
}

int isLeaf(MinHeapNode* root) {
    return !(root->left) && !(root->right);
}

// Construye el MinHeap inicial filtrando únicamente los caracteres con frecuencia > 0
MinHeap* createAndBuildMinHeap(int countArray[256]) {
    MinHeap* minHeap = createMinHeap(256);
    for (int i = 0; i < 256; i++) {
        if (countArray[i] > 0) {
            minHeap->elements[minHeap->size] = newNode((unsigned char)i, countArray[i]);
            minHeap->size++;
        }
    }
    buildMinHeap(minHeap);
    return minHeap;
}

// Construye el árbol dinámico a partir del arreglo de frecuencias
MinHeapNode* buildHuffmanTree(int countArray[256]) {
    MinHeapNode *left, *right, *top;
    MinHeap* minHeap = createAndBuildMinHeap(countArray);

    if (minHeap->size == 0) {
        free(minHeap->elements);
        free(minHeap);
        return NULL;
    }

    while (!isSizeOne(minHeap)) {
        left = extractMin(minHeap);
        right = extractMin(minHeap);
        top = newNode('$', left->frequency + right->frequency);
        top->left = left;
        top->right = right;
        insertMinHeap(minHeap, top);
    }

    MinHeapNode* root = extractMin(minHeap);
    free(minHeap->elements);
    free(minHeap);
    return root;
}

// Guarda la secuencia de '0's y '1's en la tabla global de códigos
void storeCode(int arr[], int n, unsigned char symbol) {
    char* code = (char*)malloc(n + 1); 
    for (int i = 0; i < n; ++i) {
        code[i] = '0' + arr[i]; 
    }
    code[n] = '\0'; 
    HuffmanCodesArray[symbol] = code; 
}

// Recorre el árbol asignando '0' a la izquierda y '1' a la derecha
void generarTablaCodigos(MinHeapNode* root, int arr[], int top) {
    if (root == NULL) return;

    if (root->left) { 
        arr[top] = 0; 
        generarTablaCodigos(root->left, arr, top + 1); 
    } 
    if (root->right) { 
        arr[top] = 1; 
        generarTablaCodigos(root->right, arr, top + 1); 
    } 
    if (isLeaf(root)) { 
        storeCode(arr, top, root->data); 
    } 
}

// Libera la memoria consumida por las cadenas dinámicas de códigos
void limpiarTablaCodigos(void) {
    for (int i = 0; i < 256; i++) {
        if (HuffmanCodesArray[i] != NULL) {
            free(HuffmanCodesArray[i]);
            HuffmanCodesArray[i] = NULL;
        }
    }
}

// Libera la memoria consumida por el árbol de Huffman
void liberarArbol(MinHeapNode* root) {
    if (root == NULL) return;
    liberarArbol(root->left);
    liberarArbol(root->right);
    free(root);
}
