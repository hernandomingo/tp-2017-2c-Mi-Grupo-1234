/*
 ============================================================================
 Name        : yama.c
 Author      : Grupo 1234
 Description : Proceso YAMA
 ============================================================================
 */
// ================================================================ //
// YAMA coordina con Master donde correr los jobs.
// Se conecta a FileSystem. Única instancia.
// Solo hay un YAMA corriendo al mismo tiempo.
// ================================================================ //
#include "../../utils/includes.h"

// ================================================================ //
// enum y vectores para los datos de configuración levantados del archivo config
// ================================================================ //
enum keys {
	IP_PROPIA, PUERTO_PROPIO, FS_IP, FS_PUERTO
};
char* keysConfigYama[] = { "IP_PROPIA", "PUERTO_PROPIO", "FS_IP", "FS_PUERTO",
NULL };
char* datosConfigYama[4];

// ================================================================ //
// tabla de estados de YAMA
// ================================================================ //
enum etapasTablaEstados {
	TRANSFORMACION, REDUCC_LOCAL, REDUCC_GLOBAL
};
enum estadoTablaEstados {
	EN_PROCESO, ERROR, FIN_OK
};
struct filaTablaEstados {
	int job;
	int master;
	int nodo;
	int bloque;
	int etapa;
	char[20];
	int estado;
	struct filaTablaEstados *siguiente;
};
//struct filaTablaEstados tablaEstados[100];

typedef struct {
	int tamArchivo;
	char tipoArchivo;
	int cantBloquesArchivo;
	int nodoCopia1;
	int bloqueCopia1;
	int nodoCopia2;
	int bloqueCopia2;
} bloqueArchivo;

int getDatosConfiguracion() {
	char *nameArchivoConfig = "configYama.txt";
	if (leerArchivoConfig(nameArchivoConfig, keysConfigYama, datosConfigYama)) { //leerArchivoConfig devuelve 1 si hay error
		printf("Hubo un error al leer el archivo de configuración");
		return 0;
	}
	return 1;
}

int conexionAFileSystem(t_log* logYAMA) {
	log_info(logYAMA, "Conexión a FileSystem, IP: %s, Puerto: %s", datosConfigYama[FS_IP], datosConfigYama[FS_PUERTO]);
	int socketFS = conectarA(datosConfigYama[FS_IP], datosConfigYama[FS_PUERTO]);
	if (socketFS < 0) {
		puts("Filesystem not ready\n");
		//preparadoEnviarFs = handshakeClient(&datosConexionFileSystem, NUM_PROCESO_KERNEL);
	}
	return socketFS;
}

int inicializoComoServidor(t_log* logYAMA) {
	int listenningSocket = inicializarServer(datosConfigYama[IP_PROPIA], datosConfigYama[PUERTO_PROPIO]);
	if (listenningSocket < 0) {
		log_error(logYAMA, "No pude iniciar como servidor");
		puts("No pude iniciar como servidor");
	}
	return listenningSocket;
}

int recibirConexion(t_log* logYAMA, int listenningSocket) {
	int socketCliente = aceptarConexion(listenningSocket);
	if (socketCliente < 0) {
		log_error(logYAMA, "Hubo un error al aceptar conexiones");
		puts("Hubo un error al aceptar conexiones\n");
	} else {
		log_info(logYAMA, "Cliente conectado, esperando mensajes");
		puts("Esperando mensajes\n");
	}
	return socketCliente;
}

bloqueArchivo* pedirMetadataArchivoFS(int socketFS, char *archivo) {
	printf("archivo: %s\n", archivo);

	int cantStrings = 1, i;
	char **arrayMensajesSerializar = malloc(cantStrings);
	arrayMensajesSerializar[0] = malloc(string_length(archivo) + 1);
	strcpy(arrayMensajesSerializar[0], archivo);
	arrayMensajesSerializar[0][string_length(archivo)] = '\0';

	char *mensajeSerializado = serializarMensaje(TIPO_MSJ_PEDIDO_METADATA_ARCHIVO, arrayMensajesSerializar, cantStrings);
	for (i = 0; i < cantStrings; i++) {
		free(arrayMensajesSerializar[i]);
	}
	free(arrayMensajesSerializar);
	enviarMensaje(socketFS, mensajeSerializado);

	uint32_t headerId = deserializarHeader(socketFS);
	printf("\nmensaje predefinido: %s\n", protocoloMensajesPredefinidos[headerId]);
	int cantidadMensajes = protocoloCantidadMensajes[headerId];
	char **arrayMensajes = deserializarMensaje(socketFS, cantidadMensajes);
	if (headerId != TIPO_MSJ_METADATA_ARCHIVO) {
		perror("El FS no mandó lo solicitado");
		bloqueArchivo *bloquesError = malloc(sizeof(bloqueArchivo));
		bloquesError[0].tamArchivo = 0;
		bloquesError[0].tipoArchivo = 0;
		bloquesError[0].cantBloquesArchivo = 0;
		bloquesError[0].nodoCopia1 = 0;
		bloquesError[0].bloqueCopia1 = 0;
		bloquesError[0].nodoCopia2 = 0;
		bloquesError[0].bloqueCopia2 = 0;
		return bloquesError;
	} else {
		//guardar la data en algún lado
		int tamArchivo = atoi(arrayMensajes[0]);
		char tipoArchivo = arrayMensajes[1][0];
		int cantBloquesArchivo = atoi(arrayMensajes[2]);
		bloqueArchivo *bloques = malloc(cantBloquesArchivo * sizeof(bloqueArchivo));
		int j = 3;
		for (i = 0; i < cantBloquesArchivo; i++) {
			bloques[i].tamArchivo = tamArchivo;
			bloques[i].tipoArchivo = tipoArchivo;
			bloques[i].cantBloquesArchivo = cantBloquesArchivo;
			bloques[i].nodoCopia1 = atoi(arrayMensajes[j]);
			j++;
			bloques[i].bloqueCopia1 = atoi(arrayMensajes[j]);
			j++;
			bloques[i].nodoCopia2 = atoi(arrayMensajes[j]);
			j++;
			bloques[i].bloqueCopia2 = atoi(arrayMensajes[j]);
			j++;
		}
		return bloques;
	}

}

struct filaTablaEstados *primero, *ultimo;

void agregarElemTablaEstados(struct filaTablaEstados fila) {
	struct filaTablaEstados *nuevo;
	/* reservamos memoria para el nuevo elemento */
	nuevo = (struct filaTablaEstados *) malloc(sizeof(struct filaTablaEstados));
	if (nuevo == NULL)
		perror("No hay memoria disponible!\n");
	nuevo->siguiente = NULL;
	/* ahora metemos el nuevo elemento en la lista. lo situamos al final de la lista */
	/* comprobamos si la lista está vacía. si primero==NULL es que no
	 * hay ningún elemento en la lista. también vale ultimo==NULL */
	if (primero == NULL) {
		printf("Primer elemento\n");
		primero = nuevo;
		ultimo = nuevo;
	} else {
		/* el que hasta ahora era el último tiene que apuntar al nuevo */
		ultimo->siguiente = nuevo;
		/* hacemos que el nuevo sea ahora el último */
		ultimo = nuevo;
	}

	//if()
}

int main(int argc, char *argv[]) {
	struct filaTablaEstados *primero, *ultimo;
	t_log* logYAMA;
	logYAMA = log_create("logYAMA.log", "YAMA", false, LOG_LEVEL_TRACE); //creo el logger, sin mostrar por pantalla
	int preparadoEnviarFs = 1, i;
	int cantElementosTablaEstados = 0, maxMasterTablaEstados = 0;
	int maxJobTablaEstados = 0;
	log_info(logYAMA, "Iniciando proceso YAMA");
	printf("\n*** Proceso Yama ***\n");

// 1º) leer archivo de config.
	if (!getDatosConfiguracion()) {
		return EXIT_FAILURE;
	}
	/* ************** conexión como cliente al FS *************** */
	int socketFS;
	if ((socketFS = conexionAFileSystem(logYAMA)) < 0) {
		preparadoEnviarFs = 0;
	}
	/* ************** inicialización como server ************ */
	int listenningSocket;
	if ((listenningSocket = inicializoComoServidor(logYAMA)) < 0) {
		return EXIT_FAILURE;
	}

	int socketCliente;
	if ((socketCliente = recibirConexion(logYAMA, listenningSocket)) < 0) {
		return EXIT_FAILURE;
	}
	int numMaster = socketCliente;

	/* *************************** espera recepción de un mensaje ****************************/
	uint32_t headerId = deserializarHeader(socketCliente);
	printf("\nmensaje predefinido: %s\n", protocoloMensajesPredefinidos[headerId]);
	int cantidadMensajes = protocoloCantidadMensajes[headerId];
	char **arrayMensajes = deserializarMensaje(socketCliente, cantidadMensajes);
	switch (headerId) {
	case TIPO_MSJ_HANDSHAKE:
		;
		free(arrayMensajes);
		break;
	case TIPO_MSJ_PATH_ARCHIVO:
		;
		char *archivo = malloc(string_length(arrayMensajes[0]) + 1);
		strcpy(archivo, arrayMensajes[0]);
		free(arrayMensajes);

		//pide la metadata del archivo al FS
		if (preparadoEnviarFs) {
			bloqueArchivo *bloques = pedirMetadataArchivoFS(socketFS, archivo);
			printf("Tamaño del archivo: %d\nTipo del archivo: %c\n", bloques[0].tamArchivo, bloques[0].tipoArchivo);
			printf("Bloque 1 - Copia 1: Nodo %d - Bloque %d\n", bloques[0].nodoCopia1, bloques[0].bloqueCopia1);
			printf("Bloque 1 - Copia 2: Nodo %d - Bloque %d\n", bloques[0].nodoCopia2, bloques[0].bloqueCopia2);
			printf("Bloque 2 - Copia 1: Nodo %d - Bloque %d\n", bloques[1].nodoCopia1, bloques[1].bloqueCopia1);
			printf("Bloque 2 - Copia 2: Nodo %d - Bloque %d\n", bloques[1].nodoCopia2, bloques[1].bloqueCopia2);

			int cantBloquesArchivo = bloques[0].cantBloquesArchivo;
			for (i = 0; i < cantBloquesArchivo; i++) {
				agregarElemTablaEstados(bloques[i]);
			}


		}

		//genera el nombre del archivo temporal

		//guarda la info de los bloques del archivo en la tabla de estados

		//planificación

		//envía al master la lista de nodos donde trabajar cada bloque

		break;
	case TIPO_MSJ_CUATRO_MENSAJES:
		;
		for (i = 0; i < cantidadMensajes; i++) {
			printf("mensajeRecibido %d: %s\n", i, arrayMensajes[i]);
			free(arrayMensajes[i]);
		}
		free(arrayMensajes);
		break;
	default:
		;
		free(arrayMensajes);
		break;
	}

	cerrarServer(listenningSocket);
//cerrarServer(socketCliente);
	log_info(logYAMA, "Server cerrado");

	log_destroy(logYAMA);
	return EXIT_SUCCESS;
}
