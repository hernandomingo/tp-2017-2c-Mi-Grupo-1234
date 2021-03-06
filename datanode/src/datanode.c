/*
 ============================================================================
 Name        : datanode.c
 Author      : Grupo 1234
 Description : Proceso DataNode
 Resume      : DataNode es donde persisten los datos.
 Escribe sobre el data.bin
 Puede haber varios DataNode corriendo al mismo tiempo.
 ============================================================================
 */

#include "../../utils/includes.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

#define PACKAGESIZE 1024	// Define cual va a ser el size maximo del paquete a enviar

enum keys {
	IP_PROPIA, PUERTO_PROPIO, FS_IP, FS_PUERTO, RUTA_DATABIN, NOMBRE_NODO
};
char* keysConfigDataNode[] = { "IP_PROPIA", "PUERTO_PROPIO", "FS_IP", "FS_PUERTO", "RUTA_DATABIN", "NOMBRE_NODO",
		NULL };

//FILE* archivo;
char* datosConfigDataNode[6];
char *nameArchivoConfig = "configNodo.txt";
char* carpeta_log = "../log";
int tamanoArchivo;
t_log* logDataNode;
void *mapArchivo;
struct stat buff;
int fd;
int unMega = 1048576;

int conexionAFileSystem() {
	log_trace(logDataNode, "Conexión a FileSystem, IP: %s, Puerto: %s", datosConfigDataNode[FS_IP], datosConfigDataNode[FS_PUERTO]);
	int socketFS = conectarA(datosConfigDataNode[FS_IP], datosConfigDataNode[FS_PUERTO]);
	if (socketFS >= 0) {
		log_trace(logDataNode, "Conectado a FileSystem");
	}else{
		log_error(logDataNode, "Filesystem not ready");
		//preparadoEnviarFs = handshakeClient(&datosConexionFileSystem, NUM_PROCESO_KERNEL);
	}
	return socketFS;
}

void setBloque(int idBloque, char* datos) {
	if (idBloque < tamanoArchivo) {
		log_info(logDataNode, "Escribiendo en bloque: %d", idBloque);
		int bytesEscritos = sizeof(char*) * strlen(datos) + 1;
		int posicion = idBloque * unMega;
		mapArchivo = mmap(0, buff.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, posicion);
		if (mapArchivo == MAP_FAILED) {
			perror("mmap");
			close(fd);
			exit(1);
		}
		strncpy(mapArchivo, datos, bytesEscritos);
		munmap(mapArchivo, buff.st_size);
	} else {
		log_error(logDataNode, "El bloque %i no existe en este nodo", idBloque);
	}
}

//leer los datos del bloque
char* getBloque(int idBloque) {
	if (idBloque >= tamanoArchivo) {
		log_error(logDataNode, "El bloque %i no existe en este nodo", idBloque);
		return 0;
	}
	char *buffer = malloc(unMega);
	int posicion = idBloque * unMega;
	mapArchivo = mmap(0, unMega, PROT_READ, MAP_SHARED, fd, posicion);
	if (mapArchivo == MAP_FAILED) {
		perror("mmap");
		close(fd);
		exit(1);
	}
	strncpy(buffer, mapArchivo, unMega);
	log_info(logDataNode, "Bytes leidos en el bloque %i", idBloque);
	//printf("%s\n",buffer);
	return buffer;
}

int cantidadBloquesAMandar(char * PATH) {
	FILE* archivo = fopen(PATH, "r+");
	int tamanoArchivo = 1;
	int cantidadBloques = 0;
	fseek(archivo, 0, SEEK_END);
	tamanoArchivo = ftell(archivo);
	cantidadBloques = tamanoArchivo / unMega;
	return cantidadBloques;
}

void mandarInfoAFilesystem(int socket, char **datosConfigDataNode) {
	char* nombre = datosConfigDataNode[5];
	char* ip = datosConfigDataNode[0];
	char* puerto = datosConfigDataNode[1];
	int cantidadBloq = cantidadBloquesAMandar(datosConfigDataNode[4]);

	int cantStrings = 4;
	char **arrayMensajesSerializar = malloc(sizeof(char*) * cantStrings);
	if (!arrayMensajesSerializar)
		log_error(logDataNode, "error de malloc 1");

	int i = 0;

	arrayMensajesSerializar[i] = malloc(string_length(nombre) + 1);
	if (!arrayMensajesSerializar[i])
		log_error(logDataNode, "error de malloc 1");
	strcpy(arrayMensajesSerializar[i], nombre);
	i++;

	char *cantidadBloqString = intToArrayZerosLeft(cantidadBloq, 4);
	arrayMensajesSerializar[i] = malloc(string_length(cantidadBloqString) + 1);
	if (!arrayMensajesSerializar[i])
		log_error(logDataNode, "error de malloc 1");
	strcpy(arrayMensajesSerializar[i], cantidadBloqString);
	i++;

	arrayMensajesSerializar[i] = malloc(string_length(ip) + 1);
	if (!arrayMensajesSerializar[i])
		log_error(logDataNode, "error de malloc 1");
	strcpy(arrayMensajesSerializar[i], ip);
	i++;

	arrayMensajesSerializar[i] = malloc(string_length(puerto) + 1);
	if (!arrayMensajesSerializar[i])
		log_error(logDataNode, "error de malloc 1");
	strcpy(arrayMensajesSerializar[i], puerto);
	i++;

	char *mensajeSerializado = serializarMensaje(TIPO_MSJ_DATANODE, arrayMensajesSerializar, cantStrings);
	int bytesEnviados = enviarMensaje(socket, mensajeSerializado);

	liberar_array(arrayMensajesSerializar, cantStrings);

}

int main(int argc, char *argv[]) {

	crearCarpetaDeLog(carpeta_log);
	logDataNode = log_create("../log/logDataNode.log", "DataNode", true, LOG_LEVEL_TRACE); //creo el logger, mostrando por pantalla
	log_trace(logDataNode, "Iniciando proceso DataNode");
	printf("\n*** Proceso DataNode ***\n");
	int preparadoEnviarFs = 1, i;

	// 1º) leo archivo de config.
	if (leerArchivoConfig(nameArchivoConfig, keysConfigDataNode, datosConfigDataNode)) { //leerArchivoConfig devuelve 1 si hay error
		log_error(logDataNode, "Hubo un error al leer el archivo de configuracion");
		return EXIT_FAILURE;
	}

	//TODO revisar esto!
	//2°) abro el archivo data.bin
	//archivo = fopen(datosConfigDataNode[4], "rb+");
	/*
	 //tamaño de archivo data.bin
	 int fd = fileno(archivo);
	 struct stat buff;
	 fstat(fd, &buff);
	 tamanoArchivo = buff.st_size / unMega;
	 */
	fd = open(datosConfigDataNode[4], O_RDWR);
	if (fd == -1) {
		perror("open");
		exit(1);
	}
	if (fstat(fd, &buff) < 0) {
		perror("fstat");
		close(fd);
		exit(1);
	}
	//tamaño de archivo data.bin
	tamanoArchivo = buff.st_size / unMega;

	//3°) me conecto al FS y espero solicitudes
	int socketFS = conexionAFileSystem();
	if (socketFS < 0) {
		preparadoEnviarFs = 0;
	}

	int modulo = datanode;
	send(socketFS, &modulo, sizeof(int), MSG_WAITALL);
	mandarInfoAFilesystem(socketFS, datosConfigDataNode);
	int j = 1;

	while (1) {

		int32_t header = deserializarHeader(socketFS);
		switch (header) {

		case TIPO_MSJ_ARCHIVO: {
			int cantMensajes = protocoloCantidadMensajes[header];
			char** arrayMensajesRecibidos = deserializarMensaje(socketFS, cantMensajes);
			setBloque(atoi(arrayMensajesRecibidos[1]), arrayMensajesRecibidos[0]);
			log_trace(logDataNode, "Setie bloque. Numero de bloque %s, Tipo archivo %s, Recibi mensaje %d", arrayMensajesRecibidos[1], arrayMensajesRecibidos[2], j);
			liberar_array(arrayMensajesRecibidos, cantMensajes);
			j++;
		}
		break;
		case TIPO_MSJ_PEDIR_BLOQUES: {
			//piden bloques y los mando
			int cantMensajes = protocoloCantidadMensajes[header];
			char** arrayMensajesRecibidos = deserializarMensaje(socketFS, cantMensajes);
			log_trace(logDataNode, "Bloque %d", atoi(arrayMensajesRecibidos[0]));
			char* buffer = getBloque(atoi(arrayMensajesRecibidos[0]));
			liberar_array(arrayMensajesRecibidos, cantMensajes);
			munmap(mapArchivo, unMega);

			log_info(logDataNode, "Paso getBloque");

			//SEND
			int cantStrings1 = 1;
			char** arrayMensajesSerializarEnviar = malloc(sizeof(char*) * cantStrings1);
			if (!arrayMensajesSerializarEnviar) {
				log_error(logDataNode, "error de malloc 1");
			}

			int i = 0;

			arrayMensajesSerializarEnviar[i] = malloc(string_length(buffer) + 1);
			if (!arrayMensajesSerializarEnviar[i]) {
				log_error(logDataNode, "error de malloc 1");
			}
			strcpy(arrayMensajesSerializarEnviar[i], buffer);
			free(buffer);
			i++;

			char *mensajeSerializado = serializarMensaje(TIPO_MSJ_BLOQUE_DESDE_DATANODE, arrayMensajesSerializarEnviar, cantStrings1);
			//printf("mensajeSerializado : %s\n", mensajeSerializado);
			int bytesEnviados = enviarMensaje(socketFS, mensajeSerializado);
			liberar_array(arrayMensajesSerializarEnviar, cantStrings1);
			free(mensajeSerializado);
			log_trace(logDataNode, "Bytes enviados: %d", bytesEnviados);
		}
		break;
		case TIPO_MSJ_HANDSHAKE_RESPUESTA_DENEGADO:{
			log_error(logDataNode, "FileSystem no permitio que me conectarara");
			exit(0);
			break;
		}break;
		case TIPO_MSJ_OK:{
			enviarHeaderSolo(socketFS,TIPO_MSJ_OK);
		}break;

		default: {
			log_error(logDataNode, "Se cayo FileSystem");
			exit(0);
			break;

		}

		break;
		}
	}

	close(fd);
	log_destroy(logDataNode);
	return EXIT_SUCCESS;
}
