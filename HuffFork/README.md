# HuffFork — Compresor/Descompresor Huffman con Procesos (`fork()`)

Implementación paralela del algoritmo de Huffman usando múltiples procesos mediante `fork()`, pipes y archivos temporales como mecanismos de IPC. Genera un único archivo `.huff` que contiene todos los archivos de un directorio, con firmas MD5 por archivo para verificar integridad.

**Autora:** Cristina Urbina C.

---

## Requisitos

- Compilador `gcc`
- Herramienta `make`
- Biblioteca de desarrollo OpenSSL (`libssl-dev`)
- Entorno Debian 13 con GNOME

En sistemas basados en Debian/Ubuntu:

```bash
sudo apt install -y build-essential libssl-dev
```

---

## Compilación

Desde el directorio `HuffFork/`:

```bash
make
```

Esto genera el ejecutable `huffFork` usando el `Makefile` incluido:

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2 -Wno-deprecated-declarations -Wno-unused-result
LIBS = -lcrypto

SRCS = huffFork.c huffForkComp.c huffForkDecomp.c huffTreeFork.c formato.c
TARGET = huffFork

all: $(TARGET)

$(TARGET): $(SRCS) huffman.h formato.h
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
```

Para limpiar el binario:

```bash
make clean
```

### Explicación de las banderas del compilador

- **`-Wall -Wextra`**: habilitan todas las advertencias comunes y adicionales. Se usan para detectar errores potenciales en el código. El proyecto compila sin errores; solo emite advertencias que se explican abajo.
- **`-O2`**: nivel de optimización moderado. Acelera el binario final sin tiempos de compilación excesivos.
- **`-Wno-deprecated-declarations`**: silencia las advertencias sobre funciones de OpenSSL 3.0 marcadas como obsoletas (`MD5_Init`, `MD5_Update`, `MD5_Final`). Estas funciones siguen funcionando correctamente y son válidas para el proyecto. OpenSSL las marca como "deprecated" porque en criptografía moderna se prefieren algoritmos más fuertes que MD5, pero el enunciado del proyecto exige explícitamente MD5. Por lo tanto, se silencia la advertencia sin cambiar la funcionalidad.
- **`-Wno-unused-result`**: silencia las advertencias sobre valores de retorno de `fread`, `write` y `read` que no se verifican. En este proyecto los archivos y pipes son controlados (no hay entrada maliciosa), por lo que ignorar el retorno no representa un riesgo real. Silenciar la advertencia mantiene la salida de compilación limpia.
- **`-lcrypto`**: enlaza la biblioteca OpenSSL, necesaria para las funciones MD5.

---

## Ejecución

```bash
./huffFork comprimir    <directorio>
./huffFork descomprimir <archivo.huff> <directorio_salida>
```

### Ejemplo

```bash
./huffFork comprimir    ../grandes
./huffFork descomprimir ../grandes.huff ../salida_grandes
```

Los reportes de estadísticas se escriben en `results/compresor.txt` y `results/descompresor.txt`, relativos al directorio desde donde se ejecuta el binario.

---

## Prueba con 50 archivos mixtos

Este escenario genera 50 archivos con tres tipos de contenido distintos para probar el compresor en condiciones variadas:

- **Texto repetitivo**: comprime muchísimo.
- **Texto aleatorio en base64**: comprime poco.
- **Binario puro aleatorio**: casi no comprime.

```bash
# Crear el directorio con 50 archivos mixtos
rm -rf ../grandes
mkdir -p ../grandes
for i in $(seq 1 50); do
    case $((i % 3)) in
        0)
            # Texto repetitivo
            for j in $(seq 1 200); do
                echo "linea $j repetida del archivo $i" >> ../grandes/archivo_$i.txt
            done
            ;;
        1)
            # Texto aleatorio en base64
            head -c 2048 /dev/urandom | base64 > ../grandes/archivo_$i.txt
            ;;
        2)
            # Binario puro
            head -c 4096 /dev/urandom > ../grandes/archivo_$i.bin
            ;;
    esac
done

# Compilar
make

# Limpiar resultados y salida anteriores
rm -rf results ../salida_grandes

# Comprimir
./huffFork comprimir ../grandes

# Descomprimir
./huffFork descomprimir ../grandes.huff ../salida_grandes

# Verificar integridad
diff -r ../grandes ../salida_grandes

# Ver los reportes
cat results/compresor.txt
cat results/descompresor.txt
```

### Salida esperada

En `results/compresor.txt`:

```
tiempo_total_compresor=<segundos>
tamano_total_original_bytes=<bytes>
tamano_archivo_comprimido_bytes=<bytes>
radio_compresion=<porcentaje>
```

En `results/descompresor.txt`:

```
porcentaje_salud=100.00
firmas_verificadas=50
total_archivos=50
tiempo_total_descompresor=<segundos>
```

Y `diff -r` no debe imprimir nada (los directorios son idénticos).

---

## Explicación del código

### Estructura del archivo `.huff`

El contenedor único que genera el compresor tiene este formato binario:

```
[1 byte ]  modo (1=serial, 2=fork, 3=pthread)
[256 int]  tabla de frecuencias (256 * sizeof(int) bytes)
[4 bytes]  cantidad de archivos (uint32_t)
[Por cada archivo]
    [2 bytes]  longitud del nombre (uint16_t)
    [N bytes]  nombre del archivo (sin '\0')
    [8 bytes]  tamaño original descomprimido (uint64_t)
    [8 bytes]  tamaño comprimido (uint64_t)
    [16 bytes] MD5 del archivo original
[Datos comprimidos concatenados de todos los archivos]
```

El primer byte identifica el modo de compresión. La tabla de frecuencias permite reconstruir el árbol de Huffman sin necesidad de los archivos originales. El tamaño comprimido por archivo es indispensable para que el descompresor sepa dónde termina la parte de cada archivo dentro del flujo concatenado.

### `formato.h` / `formato.c`

Encapsulan la escritura y lectura del header del `.huff`. Toda función de escritura tiene su par de lectura con el mismo tamaño de campo, garantizando portabilidad entre los tres programas (serial, fork, pthread). Se usan tipos de ancho fijo (`uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`) para que el formato no dependa de la arquitectura.

### `huffman.h`

Header compartido por todos los `.c` de la versión fork. Declara:

- `MinHeapNode`: nodo del árbol de Huffman.
- `ListaArchivos`: lista dinámica de rutas.
- `HuffmanCodesArray`: tabla global de códigos, llenada por el árbol y usada por la compresión.
- Prototipos de todas las funciones públicas.

### `huffFork.c` (main y utilidades)

Es el punto de entrada. Parsea los argumentos y despacha a compresión o descompresión. Además implementa funciones auxiliares que usan tanto el compresor como el descompresor:

- `listarArchivosRecursivo`: recorre el directorio recursivamente y agrega rutas a la lista (ignora archivos `.huff`).
- `contarFrecuenciasArchivo`: lee un archivo byte a byte y acumula frecuencias en un arreglo de 256 enteros.
- `calcularMD5`: calcula el MD5 con OpenSSL usando streaming, sin cargar el archivo completo a memoria.

### `huffForkComp.c` (compresión paralela)

El compresor trabaja en dos fases:

**Fase 1 — Conteo de frecuencias con fork + pipes.**
Por cada archivo de la lista, el padre crea un `pipe()` y hace `fork()`. El hijo cuenta las frecuencias de su archivo y las escribe por el pipe. El padre lee la tabla de 256 ints, espera con `waitpid()` y suma las frecuencias en el arreglo global. Al finalizar esta fase el padre tiene la tabla de frecuencias consolidada de todo el directorio.

**Fase 2 — Compresión con fork + archivos temporales.**
El padre construye el árbol de Huffman global con las frecuencias sumadas y genera la tabla de códigos. Luego crea un directorio temporal con `mkdtemp()` y hace `fork()` por cada archivo. Cada hijo hereda el árbol por copy-on-write (no necesita IPC para el árbol) y comprime su archivo a un temporal `tmp_<pid>_<indice>.bin`. El padre espera a todos los hijos con `wait()`.

Finalmente el padre:
1. Obtiene el tamaño comprimido de cada temporal con `stat()`.
2. Escribe el header del `.huff`: modo, frecuencias, cantidad y metadata por archivo (nombre, tamaño original, tamaño comprimido, MD5).
3. Concatena los temporales en el `.huff` final.
4. Borra los temporales y el directorio temporal con `rmdir()`.
5. Calcula el tiempo total, el radio de compresión y escribe `results/compresor.txt`.

**Estrategia de IPC:**
- `pipe()` en la fase 1 para que los hijos envíen sus tablas de frecuencias al padre.
- Archivos temporales en la fase 2 para evitar escrituras concurrentes al `.huff` final (no hay exclusión mutua entre procesos sin memoria compartida).
- `wait()` / `waitpid()` para sincronización y recolección de hijos.

### `huffForkDecomp.c` (descompresión paralela)

El descompresor sigue un flujo simétrico:

1. Lee el header del `.huff`: modo, frecuencias, cantidad y metadata de cada archivo.
2. Reconstruye el árbol de Huffman desde la tabla de frecuencias guardada.
3. Calcula los offsets de cada archivo dentro de la zona de datos sumando acumulativamente los `tamanoComprimido`.
4. Por cada archivo, hace `fork()`. El hijo:
   - Hace `fseek()` a la posición exacta `offsetDatos + offsets[i]`.
   - Descomprime exactamente `tamanoOriginal` bytes al archivo de salida.
   - Calcula el MD5 del archivo restaurado y lo compara con el guardado en la metadata.
   - Envía el resultado (`1` o `0`) al padre por un pipe.
5. El padre recolecta los resultados con `waitpid()`, cuenta verificados y escribe `results/descompresor.txt` con porcentaje de salud, firmas verificadas, total de archivos y tiempo total.

**Estrategia de IPC:** `pipe()` por cada hijo para transmitir el resultado de la verificación MD5 al padre, y `waitpid()` para sincronización.

### `huffTreeFork.c` (árbol y compresión/descompresión de un archivo)

Contiene toda la lógica del algoritmo de Huffman:

- **Min-heap**: `nuevoNodo`, `crearMinHeap`, `siftDown`, `extraerMin`, `insertarMinHeap`, `buildMinHeap`.
- **`construirArbolHuffman`**: inserta los 256 símbolos con frecuencia mayor a cero en el heap, y combina repetidamente los dos nodos de menor frecuencia hasta obtener la raíz.
- **`generarTablaCodigos`** / **`storeCode`**: recorren el árbol recursivamente, asignando `0` a la rama izquierda y `1` a la derecha, y guardan el string de bits por símbolo en `HuffmanCodesArray`.
- **`comprimirArchivo`**: lee byte a byte, escribe los bits al buffer, empaqueta en bytes completos y hace padding al final con ceros.
- **`descomprimirArchivo`**: maneja el caso especial de árbol de una sola hoja (archivo con un único carácter repetido) escribiendo directamente el carácter, y el caso general recorriendo el árbol bit a bit hasta escribir la cantidad de bytes originales.
- **`liberarArbol`** / **`limpiarTablaCodigos`**: liberación de memoria.

### Decisiones de diseño

1. **Un único `.huff` para todo el directorio**: en vez de generar un `.huff` por archivo, se genera un solo contenedor con metadata por archivo. Simplifica la verificación y centraliza los datos.

2. **Árbol de Huffman global**: se construye con las frecuencias sumadas de todos los archivos. Mejora la compresión y reduce el uso de memoria (un solo árbol).

3. **Guardar la tabla de frecuencias en el `.huff`**: permite reconstruir el árbol sin necesitar los archivos originales.

4. **Guardar `tamanoComprimido` por archivo**: sin este campo el descompresor no puede calcular los offsets de cada archivo dentro del flujo concatenado. Fue una decisión crítica para la correctitud del formato.

5. **Byte de modo con `uint8_t`**: garantiza que ocupa exactamente 1 byte en cualquier plataforma, a diferencia de `int` cuyo tamaño varía.

6. **Un proceso hijo por archivo**: para ***n*** archivos se crean ***n*** hijos en cada fase. Es simple y suficiente para el tamaño del proyecto. Alternativas con pool de procesos tendrían más sobrecarga de sincronización.

7. **Fork + pipes en fase 1, fork + temporales en fase 2**: `pipe()` es el IPC natural para pasar un vector de 256 ints del hijo al padre. Los archivos temporales evitan que dos hijos escriban al mismo archivo final simultáneamente, lo que requeriría exclusión mutua entre procesos.

8. **`clock_gettime(CLOCK_MONOTONIC)`**: reloj monotónico con resolución de nanosegundos, no afectado por cambios de hora del sistema. Se descartó `time()` (resolución de 1 segundo) y `gettimeofday()` (afectable por NTP).

9. **Salida de estadísticas en archivos `clave=valor`**: cada binario escribe sus propias métricas en `results/compresor.txt` o `results/descompresor.txt` con formato simple para que la GUI las parsee sin depender del formato humano de la salida por consola.

10. **Silenciar advertencias con flags del compilador**: en vez de modificar el código con casts y verificaciones innecesarias, se usan `-Wno-deprecated-declarations` y `-Wno-unused-result`. Esto mantiene el código limpio y evita introducir ramas de error para casos que no ocurren en la práctica del proyecto.

11. **Makefile**: en lugar de escribir el comando de compilación cada vez, se usa un `Makefile` que encapsula las banderas, los fuentes, el target y la limpieza. Esto evita errores de tipeo y estandariza la compilación entre los integrantes del grupo.

---

## Créditos

- **Autora del código de la versión fork:** Cristina Urbina C.
- **Algoritmo de Huffman:** David A. Huffman, *A Method for the Construction of Minimum-Redundancy Codes*, 1952.
- **MD5:** Ronald Rivest, RFC 1321, 1992.

---

## Referencias

Huffman, D. A. (1952). A method for the construction of minimum-redundancy codes. *Proceedings of the IRE*, *40*(9), 1098–1101.

Kerrisk, M. (2010). *The Linux programming interface*. No Starch Press.

Linux man-pages project. (2024). *fork(2), pipe(2), wait(2), mkdtemp(3), clock_gettime(2)*. https://man7.org/linux/man-pages/

OpenSSL Project. (2023). *MD5 — OpenSSL documentation*. https://www.openssl.org/docs/man3.0/man3/MD5.html

Rivest, R. (1992). *The MD5 message-digest algorithm* (RFC 1321). Internet Engineering Task Force.