#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "huffman_pthread.h"

typedef struct MinHeap {
    MinHeapNode** elements;
    unsigned size;
    unsigned capacity;
} MinHeap;

static MinHeapNode* newNode(unsigned char data, long freq) {
    MinHeapNode* temp = (MinHeapNode*)malloc(sizeof(MinHeapNode));
    temp->left = temp->right = NULL;
    temp->data = data;
    temp->frequency = freq;
    return temp;
}

static MinHeap* createMinHeap(unsigned capacity) {
    MinHeap* minHeap = (MinHeap*)malloc(sizeof(MinHeap));
    minHeap->size = 0;
    minHeap->capacity = capacity;
    minHeap->elements = (MinHeapNode**)malloc(capacity * sizeof(MinHeapNode*));
    return minHeap;
}

static void swapMinHeapNode(MinHeapNode** a, MinHeapNode** b) {
    MinHeapNode* t = *a;
    *a = *b;
    *b = t;
}

static void siftDown(MinHeap* minHeap, int pos) {
    while (2 * pos + 1 < (int)minHeap->size) {
        int smallest = 2 * pos + 1;
        if (smallest + 1 < (int)minHeap->size && minHeap->elements[smallest + 1]->frequency < minHeap->elements[smallest]->frequency) {
            smallest++;
        }
        if (minHeap->elements[pos]->frequency <= minHeap->elements[smallest]->frequency) {
            break;
        }
        swapMinHeapNode(&minHeap->elements[pos], &minHeap->elements[smallest]);
        pos = smallest;
    }
}

static int isSizeOne(MinHeap* minHeap) {
    return (minHeap->size == 1);
}

static MinHeapNode* extractMin(MinHeap* minHeap) {
    MinHeapNode* temp = minHeap->elements[0];
    minHeap->elements[0] = minHeap->elements[minHeap->size - 1];
    minHeap->size--;
    siftDown(minHeap, 0);
    return temp;
}

static void insertMinHeap(MinHeap* minHeap, MinHeapNode* minHeapNode) {
    minHeap->size++;
    int i = minHeap->size - 1;
    while (i && minHeapNode->frequency < minHeap->elements[(i - 1) / 2]->frequency) {
        minHeap->elements[i] = minHeap->elements[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap->elements[i] = minHeapNode;
}

static void buildMinHeap(MinHeap* minHeap) {
    for (int i = ((int)minHeap->size - 1) / 2; i >= 0; --i) {
        siftDown(minHeap, i);
    }
}

static int isLeaf(MinHeapNode* root) {
    return !(root->left) && !(root->right);
}

static MinHeap* createAndBuildMinHeap(long countArray[256]) {
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

MinHeapNode* buildHuffmanTree(long countArray[256]) {
    MinHeapNode *left, *right, *top;
    MinHeap* minHeap = createAndBuildMinHeap(countArray);

    if (minHeap->size == 0) {
        free(minHeap->elements);
        free(minHeap);
        return NULL;
    }

    // Caso especial: solo hay un tipo de byte en todos los archivos
    if (minHeap->size == 1) {
        MinHeapNode* only = extractMin(minHeap);
        top = newNode('$', only->frequency);
        top->left = only;
        top->right = NULL;
        free(minHeap->elements);
        free(minHeap);
        return top;
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

static void storeCode(int arr[], int n, unsigned char symbol, char* codesArray[256]) {
    char* code = (char*)malloc(n + 1);
    for (int i = 0; i < n; ++i) {
        code[i] = '0' + arr[i];
    }
    code[n] = '\0';
    codesArray[symbol] = code;
}

void generarTablaCodigos(MinHeapNode* root, int arr[], int top, char* codesArray[256]) {
    if (root == NULL) return;

    if (root->left) {
        arr[top] = 0;
        generarTablaCodigos(root->left, arr, top + 1, codesArray);
    }
    if (root->right) {
        arr[top] = 1;
        generarTablaCodigos(root->right, arr, top + 1, codesArray);
    }
    if (isLeaf(root)) {
        // Si el árbol sólo tiene un nodo hoja en la raíz
        if (top == 0) {
            arr[0] = 0;
            storeCode(arr, 1, root->data, codesArray);
        } else {
            storeCode(arr, top, root->data, codesArray);
        }
    }
}

void limpiarTablaCodigos(char* codesArray[256]) {
    for (int i = 0; i < 256; i++) {
        if (codesArray[i] != NULL) {
            free(codesArray[i]);
            codesArray[i] = NULL;
        }
    }
}

void liberarArbol(MinHeapNode* root) {
    if (root == NULL) return;
    liberarArbol(root->left);
    liberarArbol(root->right);
    free(root);
}

