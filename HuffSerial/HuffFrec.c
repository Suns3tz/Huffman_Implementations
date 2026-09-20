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


void extraerfreq(FILE* Freq) {
    char linea[256];
    int valor_hex, frecuencia;

    memset(ASCIIcount, 0, sizeof(ASCIIcount));

    while (fgets(linea, sizeof(linea), Freq)) {
        // Intenta buscar el patrón básico: Hexadecimal y Frecuencia
        if (sscanf(linea, "%x %d", &valor_hex, &frecuencia) == 2 ||
            sscanf(linea, "0x%x | %*s | %d", &valor_hex, &frecuencia) == 2) {
            if (valor_hex >= 0 && valor_hex < 256) {
                ASCIIcount[valor_hex] = frecuencia;
            }
        }
    }
}

