# Implementación Paralela con Pthreads (HuffPthread) - Archivo Único

Esta carpeta contiene la implementación concurrente del algoritmo de compresión y descompresión de Huffman utilizando hilos POSIX (**Pthreads**) y **memoria compartida**, empaquetando todos los archivos de un directorio en un **único archivo contenedor `.huff`** (similar a `zip` o `tar.gz`).

## Características de la Implementación
- **Empaquetado en un Único Archivo Contenedor (`<directorio>.huff`):**
  - **Encabezado Global:** Contiene la firma identificadora (`HUFF`), la versión, la cantidad de archivos y la tabla global de frecuencias de Huffman (1024 bytes).
  - **Catálogo de Archivos:** Metadatos individuales de cada archivo (ruta relativa, tamaño original, tamaño comprimido, *offset* en bytes dentro del archivo contenedor y firma criptográfica MD5 de 16 bytes).
  - **Flujos de Datos Continuos:** Los flujos de bits comprimidos de todos los archivos empaquetados consecutivamente.
- **Compresión Concurrente en RAM (Cero contención de disco):**
  - Cada hilo de trabajo comprime un archivo directamente a un buffer en memoria RAM (`malloc`).
  - Una vez procesados todos los archivos en paralelo, se vuelcan ordenadamente al archivo único contenedor `.huff`.
- **Descompresión Concurrente por Offsets:**
  - Al descomprimir, los hilos leen concurrentemente desde sus respectivos desplazamientos (*offsets*) dentro del archivo `.huff`, reconstruyen la estructura de directorios y extraen los archivos.
- **Verificación de Integridad MD5:**
  - Cada archivo extraído es recalculado con MD5 y comparado contra la firma original almacenada en el catálogo (`[OK] Válido`).
- **Comportamiento Limpio Estilo UNIX:**
  - Al comprimir (`-c`), se genera `<directorio>.huff` y se eliminan los archivos originales (a menos que se use `-k`).
  - Al descomprimir (`-d`), se restauran los archivos originales y se elimina el contenedor `.huff` (a menos que se use `-k`).
  - En ciclo completo (`-all`), se prueba todo el ciclo y se deja la carpeta limpia con los archivos originales intactos.
- **Bandera `-k` / `--keep`:**
  - Conserva tanto los archivos originales como el archivo contenedor `.huff`.

---

## Compilación

```bash
cd "/home/vboxuser/Desktop/Proyecto 1/Huffman_Implementations/HuffPthread"
make
```

---

## Uso

```bash
./huffPthread <directorio_o_archivo.huff> [num_hilos] [modo: -c | -d | -all] [-k | --keep]
```

### Argumentos:
- `<directorio_o_archivo.huff>`: Ruta de la carpeta a comprimir o del archivo `.huff` a descomprimir.
- `[num_hilos]`: (Opcional) Número de hilos concurrentes. Por defecto usa los núcleos detectados del CPU.
- `[modo]`:
  - `-c`: Comprime la carpeta en un único archivo `<nombre>.huff`.
  - `-d`: Descomprime y extrae los archivos desde el contenedor `<nombre>.huff`.
  - `-all`: (Por defecto) Ciclo completo de compresión, extracción y verificación MD5.
- `-k, --keep`: (Opcional) Conservar tanto los archivos originales como el `.huff`.

### Ejemplos:

```bash
# 1. Comprimir una carpeta completa en un solo archivo .huff (con 4 hilos):
./huffPthread "../../test_data/libros_gutenberg" 4 -c

# 2. Descomprimir y extraer todos los archivos del .huff (con 4 hilos):
./huffPthread "../../test_data/libros_gutenberg.huff" 4 -d

# 3. Comprimir conservando la carpeta original:
./huffPthread "../../test_data/libros_gutenberg" 4 -c -k

# 4. Probar rendimiento completo sin dejar residuos:
./huffPthread "../../test_data/libros_gutenberg" 4 -all
```
