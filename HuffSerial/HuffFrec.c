#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h> //Adiccion para usar directorios.
#include <sys/stat.h> //Diferenciar entre archivos y carpetas

#include "huffman.h"

int ASCIIcount[256] = {0};

void countfreq(FILE *RFROM) { //Cuenta todas las frecuencias 
    char buffer[1024]; 
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), RFROM)) > 0) { //lee el documento 
        for(size_t i = 0; i < bytes; i++) {
            ASCIIcount[(unsigned char)buffer[i]]++; //Asigna al valor ascii una suma de frecuencia
        }
    }
}

void procesarRuta(const char *path) {
	struct stat st;
	if (stat(path, &st) != 0) {
		perror("Error al obtener estado de la ruta");
		return;
	}
	
	if (S_ISREG(st.st_mode)) {
		FILE *input = fopen(path, "rb");
		if (input != NULL) {
			countfreq(input);
			fclose(input);
		} else{
			perror("Error abriendo el archivo");
		}
	}
	else if (S_ISDIR(st.st_mode)){
		DIR *dir = opendir(path);
		if (dir == NULL) {
			perror("Error abriendo directorio");
			return;
		}
		
		struct dirent *entry;
		char subPath[1024];
		while((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
				continue;
			}
			snprintf(subPath, sizeof(subPath), "%s/%s", path, entry->d_name);
			procesarRuta(subPath);
		}
		closedir(dir);
	}
}


void extraerfreq(FILE* Freq) { //Extrae las frecuencias guardadas en la tabla de frequencias
    char linea[100];
    int valor_hex, frecuencia;
    char simbolo;

    fgets(linea, sizeof(linea), Freq);
    fgets(linea, sizeof(linea), Freq);

    fgets(linea, sizeof(linea), Freq);
    sscanf(linea, " %x | | %d", &valor_hex, &frecuencia);
    ASCIIcount[0] = frecuencia;

    while (fgets(linea, sizeof(linea), Freq)) {
        if (sscanf(linea, " 0x%02X | '%c' | %d", &valor_hex, &simbolo, &frecuencia) == 3) {
            ASCIIcount[valor_hex] = frecuencia;
        } else if (sscanf(linea, " 0x%02X | | %d", &valor_hex, &frecuencia) == 2) {
            ASCIIcount[valor_hex] = frecuencia;
        }
    }
}
