# Implementaciones del Algoritmo de Huffman (Proyecto 1)

Este proyecto es una implementación del algoritmo de compresión y descompresión sin pérdida de **Huffman** en lenguaje **C**. Permite procesar archivos individuales o directorios de forma recursiva, generando archivos comprimidos (`.huff`) y validando la integridad de los datos mediante firmas criptográficas **MD5**.

---

## Requisitos

- Compilador `gcc`
- Biblioteca de desarrollo OpenSSL (`libssl-dev`)

En sistemas basados en Debian/Ubuntu:
```bash
sudo apt install -y build-essential libssl-dev
```
*(Si no tiene `sudo` configurado, ingrese como `root` con `su -` antes de instalar).*

---

## Compilación y Ejecución

1. **Navegar al directorio de la versión serial:**
   ```bash
   cd HuffSerial
   ```

2. **Compilar:**
   ```bash
   gcc -Wall -Wextra -O2 huffSerial.c HuffFrec.c HuffTree.c huffComp.c huffDecomp.c -o huffSerial -lcrypto
   ```

3. **Ejecutar:**
   ```bash
   ./huffSerial <ruta_al_archivo_o_directorio>
   ```

   **Ejemplo:**
   ```bash
   ./huffSerial ../../pruebas
   ```

