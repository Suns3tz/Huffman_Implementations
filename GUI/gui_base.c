#include <gtk/gtk.h>
#include <stdio.h>

// Variable global para guardar la ruta de la carpeta elegida
typedef struct {
    double porcentajeSalud;
    int firmasVerificadas;
    int totalArchivos;
    double tiempoCompresor;
    double tiempoDescompresor;
    uint64_t tamanoOriginalBytes;
    uint64_t tamanoComprimidoBytes;
    double ratioCompresion;
} EstadisticasHuffman;

int cargarEstadisticas(const char *rutaArchivo, EstadisticasHuffman *stats) {
    FILE *f = fopen(rutaArchivo, "r");
    if (!f) {
        perror("Error al abrir archivo de estadísticas");
        return 0; // Fallo al abrir
    }
    memset(stats, 0, sizeof(EstadisticasHuffman));

    char linea[256];
    while (fgets(linea, sizeof(linea), f)) {
        // Eliminar salto de línea al final
        linea[strcspn(linea, "\r\n")] = 0;

        // Parsear cada clave
        sscanf(linea, "porcentaje_salud=%lf", &stats->porcentajeSalud);
        sscanf(linea, "firmas_verificadas=%d", &stats->firmasVerificadas);
        sscanf(linea, "total_archivos=%d", &stats->totalArchivos);
        sscanf(linea, "tiempo_total_compresor=%lf", &stats->tiempoCompresor);
        sscanf(linea, "tiempo_total_descompresor=%lf", &stats->tiempoDescompresor);
        sscanf(linea, "tamano_total_original_bytes=%" SCNu64, &stats->tamanoOriginalBytes);
        sscanf(linea, "tamano_archivo_comprimido_bytes=%" SCNu64, &stats->tamanoComprimidoBytes);
        sscanf(linea, "radio_compresion=%lf", &stats->ratioCompresion);
    }

    fclose(f);
    return 1; 
}

char carpeta_seleccionada[1024] = "";

GtkWidget *entry_archivo_nuevo = NULL;
GtkWidget *entry_dir_nuevo = NULL;

GtkWidget *btn_run = NULL;
GtkWidget *btn_stats = NULL;

// Callback cuando el usuario presiona el botón "Seleccionar Carpeta"
void on_btn_seleccionar_folder_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *label_ruta = (GtkWidget *)data;

    // Crear diálogo para elegir directorio
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Seleccionar Carpeta a Comprimir",
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancelar", GTK_RESPONSE_CANCEL,
        "_Seleccionar", GTK_RESPONSE_ACCEPT,
        NULL
    );

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        snprintf(carpeta_seleccionada, sizeof(carpeta_seleccionada), "%s", path);
        
        // Actualizar el texto en la interfaz
        char buffer[1100];
        snprintf(buffer, sizeof(buffer), "Carpeta seleccionada: <b>%s</b>", carpeta_seleccionada);
        gtk_label_set_markup(GTK_LABEL(label_ruta), buffer);

        g_free(path);
        
        if(strlen(carpeta_seleccionada) != 0 && (btn_run)){
			gtk_widget_set_sensitive(btn_run, TRUE);
		}else{
			gtk_widget_set_sensitive(btn_run, FALSE);
		}
        
    	
    }

    gtk_widget_destroy(dialog);
}

GtkWidget* crearPanelModulo(const char *rutaStatsActual, const char *rutaStatsSerial, const char *nombreModulo) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 15);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 15);

    EstadisticasHuffman stats, statsSerial;
    int tieneActual = cargarEstadisticas(rutaStatsActual, &stats);
    int tieneSerial = cargarEstadisticas(rutaStatsSerial, &statsSerial);

    if (!tieneActual) {
        GtkWidget *lblError = gtk_label_new(NULL);
        char bufError[256];
        snprintf(bufError, sizeof(bufError), "<i>No se encontraron datos para %s.</i>", nombreModulo);
        gtk_label_set_markup(GTK_LABEL(lblError), bufError);
        gtk_grid_attach(GTK_GRID(grid), lblError, 0, 0, 2, 1);
        return grid;
    }

    // Cálculo de aceleración 
    double speedupComp = 0.0, pctAceleracionComp = 0.0;
    double speedupDecomp = 0.0, pctAceleracionDecomp = 0.0;

    if (tieneSerial && stats.tiempoCompresor > 0.0 && statsSerial.tiempoCompresor > 0.0) {
        speedupComp = statsSerial.tiempoCompresor / stats.tiempoCompresor;
        pctAceleracionComp = (speedupComp - 1.0) * 100.0;
    }

    if (tieneSerial && stats.tiempoDescompresor > 0.0 && statsSerial.tiempoDescompresor > 0.0) {
        speedupDecomp = statsSerial.tiempoDescompresor / stats.tiempoDescompresor;
        pctAceleracionDecomp = (speedupDecomp - 1.0) * 100.0;
    }

    char bufVal[256];
    int fila = 0;

    // Helper interno para añadir filas
    #define AGREGAR_METRICA(etiqueta, valor) { \
        GtkWidget *lblKey = gtk_label_new(NULL); \
        GtkWidget *lblVal = gtk_label_new(NULL); \
        gtk_label_set_markup(GTK_LABEL(lblKey), "<b>" etiqueta ":</b>"); \
        gtk_label_set_markup(GTK_LABEL(lblVal), valor); \
        gtk_widget_set_halign(lblKey, GTK_ALIGN_START); \
        gtk_widget_set_halign(lblVal, GTK_ALIGN_START); \
        gtk_grid_attach(GTK_GRID(grid), lblKey, 0, fila, 1, 1); \
        gtk_grid_attach(GTK_GRID(grid), lblVal, 1, fila, 1, 1); \
        fila++; \
    }

    // 1. Salud e Integridad
    snprintf(bufVal, sizeof(bufVal), "%.2f%% (%d / %d archivos verifiados)", 
             stats.porcentajeSalud, stats.firmasVerificadas, stats.totalArchivos);
    AGREGAR_METRICA("Porcentaje de salud", bufVal);

    // 2. Tiempos
    snprintf(bufVal, sizeof(bufVal), "%.4f segundos", stats.tiempoCompresor);
    AGREGAR_METRICA("Tiempo total del compresor", bufVal);

    snprintf(bufVal, sizeof(bufVal), "%.4f segundos", stats.tiempoDescompresor);
    AGREGAR_METRICA("Tiempo total del descompresor", bufVal);

    // 3. Aceleración respecto a Serial
    if (strcmp(rutaStatsActual, rutaStatsSerial) != 0) {
        snprintf(bufVal, sizeof(bufVal), "%.2f%% (%.2fx)", pctAceleracionComp, speedupComp);
        AGREGAR_METRICA("Aceleración del compresor", bufVal);

        snprintf(bufVal, sizeof(bufVal), "%.2f%% (%.2fx)", pctAceleracionDecomp, speedupDecomp);
        AGREGAR_METRICA("Aceleración del descompresor", bufVal);
    }

    // 4. Tamaños y Ratios
    snprintf(bufVal, sizeof(bufVal), "%" PRIu64 " bytes", stats.tamanoOriginalBytes);
    AGREGAR_METRICA("Tamaño total archivos originales", bufVal);

    snprintf(bufVal, sizeof(bufVal), "%" PRIu64 " bytes", stats.tamanoComprimidoBytes);
    AGREGAR_METRICA("Tamaño archivo comprimido", bufVal);

    snprintf(bufVal, sizeof(bufVal), "%.2f%%", stats.ratioCompresion);
    AGREGAR_METRICA("Radio de compresión", bufVal);

    #undef AGREGAR_METRICA

    return grid;
}

// Callback para abrir la ventana secundaría con las 3 secciones
void on_btn_stats_clicked(GtkWidget *widget, gpointer window_padre) {
    (void)widget;

    GtkWidget *ventanaResultados = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ventanaResultados), "Resultados Comparativos");
    gtk_window_set_default_size(GTK_WINDOW(ventanaResultados), 500, 350);
    gtk_window_set_transient_for(GTK_WINDOW(ventanaResultados), GTK_WINDOW(window_padre));

    GtkWidget *notebook = gtk_notebook_new();

    // 1. Pestaña Serial
    GtkWidget *panelSerial = crearPanelModulo("stats_serial","stats_serial", "Serial");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), panelSerial, gtk_label_new("Serial"));

    // 2. Pestaña Concurrente (Pthreads/OpenMP)
    GtkWidget *panelConcurrente = crearPanelModulo("stats_pthread.txt","stats_serial", "Concurrente");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), panelConcurrente, gtk_label_new("Concurrente"));

    // 3. Pestaña Paralelo 
    GtkWidget *panelParalelo = crearPanelModulo("stats_paralelo","stats_serial", "Paralelo");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), panelParalelo, gtk_label_new("Paralelo"));

    gtk_container_add(GTK_CONTAINER(ventanaResultados), notebook);
    gtk_widget_show_all(ventanaResultados);
}

void on_btn_run_clicked(GtkWidget *widget, gpointer data) {
    if (strlen(carpeta_seleccionada) == 0) return;

    // 1. Obtener el nombre original de la carpeta usando GLib
    char *nombre_base = g_path_get_basename(carpeta_seleccionada);

    // 2. Definir nombres automáticos para archivo .huff y carpeta extraída
    char archivo_huff[1024];
    char carpeta_salida[1024];

    snprintf(archivo_huff, sizeof(archivo_huff), "%s.huff", nombre_base);
    snprintf(carpeta_salida, sizeof(carpeta_salida), "%s", nombre_base);


    // 3. Ejecutar Compresión Serial
    char cmd_comp_serial[3000];
    snprintf(cmd_comp_serial, sizeof(cmd_comp_serial),
             "../HuffSerial/huffSerial -all \"%s\"",
             carpeta_seleccionada);

    int res_serial = system(cmd_comp_serial);
    if (res_serial == 0) {
         if (btn_stats) gtk_widget_set_sensitive(btn_stats, TRUE);
	}else {
		perror("Error al ejecutar la compresión serial");
	}
    
    // ------------------------------------------------------
    // Concurrente (PTHREADS)
    // ------------------------------------------------------
    
    char cmd_pthread[3000];
    snprintf(cmd_pthread, sizeof(cmd_pthread),
             "../HuffPthread/huffPthread \"%s\" -all -k -s stats_pthread.txt",
             carpeta_seleccionada);
             
    int res_pthread = system(cmd_pthread);
    
    if (res_pthread == 0) {
		if(btn_stats) {
			gtk_widget_set_sensitive(btn_stats, TRUE);
		}else{
			perror("Error al ejecutar el modulo pthread");
		}
	}
    
    

    g_free(nombre_base);
}


int main(int argc, char *argv[]) {
    // 1. Inicializar GTK
    gtk_init(&argc, &argv);

    // 2. Crear la Ventana Principal
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Proyecto Huffman - Benchmark GTK");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 200);
    gtk_container_set_border_width(GTK_CONTAINER(window), 15);

    // Cerrar el programa si se da clic en la 'X'
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // 3. Contenedor Principal 
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    // 4. Sección de Selección de Carpeta 
    
    GtkWidget *frame_comp = gtk_frame_new(" Módulo de Compresión ");
    GtkWidget *box_comp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box_comp), 10);
    gtk_container_add(GTK_CONTAINER(frame_comp), box_comp);

    GtkWidget *folder_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *btn_folder = gtk_button_new_with_label("Elegir directorio");
    GtkWidget *lbl_folder = gtk_label_new("Ninguna carpeta seleccionada");
    gtk_label_set_use_markup(GTK_LABEL(lbl_folder), TRUE);
    gtk_box_pack_start(GTK_BOX(folder_box), btn_folder, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(folder_box), lbl_folder, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_comp), folder_box, FALSE, FALSE, 0);
   
    gtk_box_pack_start(GTK_BOX(main_box), frame_comp, FALSE, FALSE, 0);
    

    g_signal_connect(btn_folder, "clicked", G_CALLBACK(on_btn_seleccionar_folder_clicked), lbl_folder);

    // 5. Botones para Iniciar Pruebas 
    
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	btn_run = gtk_button_new_with_label("Comprimir y Descomprimir de manera Serial, paralela y concurrente.");
	btn_stats = gtk_button_new_with_label("Ver estadisticas");
	
	gtk_widget_set_sensitive(btn_run, FALSE);
	gtk_widget_set_sensitive(btn_stats, FALSE);
	
	gtk_box_pack_start(GTK_BOX(btn_box), btn_run, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(btn_box), btn_stats, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(main_box), btn_box, FALSE, FALSE, 0);
	
	//Logica de botones
	g_signal_connect(btn_run, "clicked", G_CALLBACK(on_btn_run_clicked), NULL);
	g_signal_connect(btn_stats, "clicked", G_CALLBACK(on_btn_stats_clicked), NULL);

	
    // 6. Mostrar todo y ejecutar
    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
