# Implementación Paralela con Pthreads (HuffPthread)

Esta carpeta contendrá la versión paralelizada del algoritmo de compresión y descompresión de Huffman utilizando hilos de ejecución POSIX (**Pthreads**).
Esta carpeta contiene la implementación concurrente del algoritmo de compresión y descompresión de Huffman utilizando hilos POSIX (**Pthreads**) y **memoria compartida** para acelerar el procesamiento de archivos y directorios.

## Objetivos de la implementación
- Distribuir el procesamiento (conteo de frecuencias, compresión/descompresión de archivos o bloques) entre múltiples hilos (`pthread_create`, `pthread_join`).
- Sincronización y manejo de secciones críticas mediante mutexes o barreras si es necesario.
- Medición de tiempos de ejecución para cálculo de métricas de aceleración (*speedup*) y eficiencia.
## Características de la Implementación
- **Memoria compartida y cola de trabajo dinámica:** Los archivos descubiertos en el directorio se encolan en una estructura compartida (`SharedContext`). Los hilos toman tareas de forma dinámica (*work-stealing* / cola protegida por mutex), garantizando un balance óptimo de carga entre archivos de diferente tamaño.
- **Conteo de frecuencias paralelo:** Cada hilo acumula frecuencias en un buffer privado para eliminar contención y *false sharing*, consolidando finalmente en la tabla global compartida mediante un mutex.
- **Árbol y códigos de solo lectura:** El árbol de Huffman y la tabla de códigos se construyen en RAM compartida y se acceden de forma concurrente en modo solo lectura por todos los hilos durante la compresión y descompresión (cero bloqueos).
- **Verificación de integridad MD5:** Calcula la firma MD5 de 16 bytes y la almacena en los metadatos del archivo `.huff`. Durante la descompresión, recalcula el MD5 del archivo restaurado y verifica la salud de los datos (`memcmp`).
- **Medición de alta precisión:** Tiempos medidos con `clock_gettime(CLOCK_MONOTONIC)` para cada fase (conteo, árbol, compresión, descompresión y tiempo total).

---

## Compilación

Dentro de esta carpeta:
```bash
make
```

Para limpiar los binarios y objetos:
```bash
make clean
```

---

## Uso

```bash
./huffPthread <directorio_o_archivo> [num_hilos] [modo: -c | -d | -all]
```

### Argumentos:
- `<directorio_o_archivo>`: Ruta del archivo o carpeta a procesar.
- `[num_hilos]`: (Opcional) Número de hilos concurrentes a utilizar. Por defecto utiliza todos los núcleos disponibles en el procesador (`sysconf(_SC_NPROCESSORS_ONLN)`).
- `[modo]`: (Opcional)
  - `-c`: Solo comprimir los archivos a `.huff`.
  - `-d`: Solo descomprimir los archivos `.huff`.
  - `-all`: (Por defecto) Realizar el ciclo completo de compresión, descompresión y verificación de integridad MD5.

### Ejemplos:
```bash
# Ejecutar ciclo completo con detección automática de hilos
./huffPthread ../../pruebas

# Comprimir utilizando 8 hilos
./huffPthread ../../pruebas 8 -c

# Descomprimir utilizando 4 hilos
./huffPthread ../../pruebas 4 -d
```
