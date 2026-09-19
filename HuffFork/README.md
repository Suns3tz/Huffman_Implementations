# Implementación Paralela con Procesos / Fork (HuffFork)

Esta carpeta contendrá la versión paralelizada del algoritmo de compresión y descompresión de Huffman utilizando múltiples procesos mediante la llamada al sistema `fork()`.

## Objetivos de la implementación
- Distribuir el procesamiento (conteo de frecuencias, compresión/descompresión de archivos o bloques) entre múltiples procesos hijos creados con `fork()`.
- Comunicación entre procesos (IPC) mediante mecanismos como memoria compartida (`shmget`/`shmat` o `mmap`), tuberías (`pipes`), colas de mensajes o archivos temporales.
- Sincronización y recolección de procesos con `wait()` / `waitpid()`.
- Medición de tiempos de ejecución para comparativa de desempeño frente a las versiones Serial y Pthreads.

