# Implementación Paralela con Pthreads (HuffPthread)

Esta carpeta contendrá la versión paralelizada del algoritmo de compresión y descompresión de Huffman utilizando hilos de ejecución POSIX (**Pthreads**).

## Objetivos de la implementación
- Distribuir el procesamiento (conteo de frecuencias, compresión/descompresión de archivos o bloques) entre múltiples hilos (`pthread_create`, `pthread_join`).
- Sincronización y manejo de secciones críticas mediante mutexes o barreras si es necesario.
- Medición de tiempos de ejecución para cálculo de métricas de aceleración (*speedup*) y eficiencia.

