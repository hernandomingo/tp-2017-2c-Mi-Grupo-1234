/*
 ============================================================================
 Name        : yama.c
 Author      : Grupo 1234
 Description : Proceso YAMA
 Resume      : YAMA coordina con Master donde correr los jobs.
 Se conecta a FileSystem. Única instancia.
 Solo hay un YAMA corriendo al mismo tiempo.
 ============================================================================
 */

#define CANT_MAX_FD	50

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <errno.h>
#include <signal.h>

//para el select
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

#include <unistd.h>

#include "../../utils/constantes.h"
#include "../../utils/utils.h"
#include "../../utils/conexionesSocket.h"
#include "../../utils/archivoConfig.h"
#include "../../utils/comunicacion.h"

char* carpeta_log = "../log";
t_log* logYAMA;

typedef struct {
	int nroNodo;
	int bloque;
	int bytesOcupados;
	char temporal[LARGO_TEMPORAL];
} nodoParaAsignar;

typedef struct {
	int nodoCopia1;
	int bloqueCopia1;
	int nodoCopia2;
	int bloqueCopia2;
	int bytesBloque;
} bloqueArchivo;

#include "globalNodos.c"
#include "../../utils/protocolo.c"
#include "tablaEstados.h"
#include "planificacion.h"
#include "nroMasterJob.c"
#include "inicializacion.c"
#include <signal.h>
#include "comunicacionesFS.c"

//limpia toda la memoria de las estructuras administrativas y demás
//ideal para liberar la memmoria en el cierre del proceso
void limpiarMemoria() {
	free(listaGlobalNodos);
	eliminarListaMasterJobCompleta();
	eliminarListaTablaEstadosCompleta();
}

// mover a utils.h o alguna libreria
void sig_handler(int signal) {
	switch (signal) {
	case SIGINT:
		puts("\nAlguien presionó ctrl+c, saliendo....\n");
		log_info(logYAMA, "Alguien presionó ctrl+c, se finaliza el proceso");
		log_info(logYAMA, "Server cerrado");
		log_destroy(logYAMA);
		limpiarMemoria();
		exit(1);
		break;
	case SIGUSR1:	//recarga de la configuración
		puts("\nRecieved SIGUSR1, reloading config...\n");
		log_info(logYAMA, "Recieved SIGUSR1, reloading config...");
		//recargarConfig = 1;
		if (!getDatosConfiguracion()) {
			log_error(logYAMA, "No se pudieron recargar los datos del archivo de configuración");
			puts("Error al recargar los datos del archivo de configuración");
		} else {
			strcpy(algoritmoPlanificacion, datosConfigYama[ALGORITMO_BALANCEO]);
			retardoPlanificacion = atoi(datosConfigYama[RETARDO_PLANIFICACION]);
		}
		log_info(logYAMA, "Se recargó la configuración");
		log_info(logYAMA, "Retardo de planificación: %d", retardoPlanificacion);
		log_info(logYAMA, "Algoritmo de planificación: %d", algoritmoPlanificacion);
		puts("Se recargó la configuración del YAMA");
		printf("retardoPlanificacion: %d\n", retardoPlanificacion);
		printf("algoritmoPlanificacion: %s\n", algoritmoPlanificacion);
		break;
	default:
		break;
	}
}

char* serializarMensajeTransformacion(nodoParaAsignar *datosParaTransformacion, int cantPartesArchivo) {
	int i, j, k, cantStringsASerializar, largoStringDestinoCopia;

	cantStringsASerializar = (cantPartesArchivo * 6) + 1;
	char **arrayMensajes = malloc(sizeof(char*) * cantStringsASerializar);

	largoStringDestinoCopia = 4 + 1;
	arrayMensajes[0] = malloc(largoStringDestinoCopia);
	strcpy(arrayMensajes[0], intToArrayZerosLeft(cantPartesArchivo, 4));
	j = 1;
//	printf("\n ---------- Tabla de transformación a enviar a master ---------- \n");
//	printf("\tNodo\tIP\t\tPuerto\tBloque\tBytes\t\tTemporal\n");
//	printf("---------------------------------------------------------------------------------------------\n");
	for (i = 0; i < cantPartesArchivo; i++) {
//		printf("\t%d\t%s\t%d\t%d\t%d\t\t%s\n", datosParaTransformacion[i].nroNodo, getDatosGlobalesNodo(datosParaTransformacion[i].nroNodo)->ip, getDatosGlobalesNodo(datosParaTransformacion[i].nroNodo)->puerto, datosParaTransformacion[i].bloque, datosParaTransformacion[i].bytesOcupados, datosParaTransformacion[i].temporal);

		//número de nodo
		largoStringDestinoCopia = 4 + 1;
		arrayMensajes[j] = malloc(largoStringDestinoCopia);
		strcpy(arrayMensajes[j], intToArrayZerosLeft(datosParaTransformacion[i].nroNodo, 4));
		j++;

		//IP
		largoStringDestinoCopia = string_length(getDatosGlobalesNodo(datosParaTransformacion[i].nroNodo)->ip) + 1;
		arrayMensajes[j] = malloc(largoStringDestinoCopia);
		strcpy(arrayMensajes[j], getDatosGlobalesNodo(datosParaTransformacion[i].nroNodo)->ip);
		j++;

		//puerto
		largoStringDestinoCopia = LARGO_PUERTO + 1;
		arrayMensajes[j] = malloc(largoStringDestinoCopia);
		strcpy(arrayMensajes[j], intToArrayZerosLeft(getDatosGlobalesNodo(datosParaTransformacion[i].nroNodo)->puerto, LARGO_PUERTO));
		j++;

		//bloque
		largoStringDestinoCopia = 4 + 1;
		arrayMensajes[j] = malloc(largoStringDestinoCopia);
		strcpy(arrayMensajes[j], intToArrayZerosLeft(datosParaTransformacion[i].bloque, 4));
		j++;

		//bytes ocupados
		largoStringDestinoCopia = 8 + 1;
		arrayMensajes[j] = malloc(largoStringDestinoCopia);
		strcpy(arrayMensajes[j], intToArrayZerosLeft(datosParaTransformacion[i].bytesOcupados, 8));
		j++;

		//temporal
		largoStringDestinoCopia = string_length(datosParaTransformacion[i].temporal) + 1;
		arrayMensajes[j] = malloc(largoStringDestinoCopia);
		strcpy(arrayMensajes[j], datosParaTransformacion[i].temporal);
		j++;
	}
	//printf("\n");
	char *mensajeSerializado = serializarMensaje(TIPO_MSJ_TABLA_TRANSFORMACION, arrayMensajes, cantStringsASerializar);
	for (j = 0; j < cantStringsASerializar; j++) {
		free(arrayMensajes[j]);
	}
	free(arrayMensajes);
	return mensajeSerializado;

}

char* serializarMensajeReduccLocal(int nroNodoRecibido, datosMasterJob *masterJobActual, char *temporalReduccLocal) {
	int j, k, h;
	int cantidadTemporalesTransformacion = getCantFilasByJMNEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, TRANSFORMACION, FIN_OK);
	char **temporales = malloc(sizeof(char*) * cantidadTemporalesTransformacion);
	getAllTemporalesByJMNEtEs(temporales, masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, TRANSFORMACION, FIN_OK);

	//armar el string serializado
	int cantNodosReduccLocal = 1;
	int cantStrings = 1 + cantNodosReduccLocal * (4 + cantidadTemporalesTransformacion + 1);
	char **arrayMensajesSerializarReduccLocal = malloc(sizeof(char*) * cantStrings);

	//cantidad de nodos
	j = 0;
	arrayMensajesSerializarReduccLocal[j] = malloc(4 + 1);
	if (!arrayMensajesSerializarReduccLocal[j])
		perror("error de malloc");
	strcpy(arrayMensajesSerializarReduccLocal[j], intToArrayZerosLeft(cantNodosReduccLocal, 4));
	j++;

	//para cada nodo
	for (k = 0; k < cantNodosReduccLocal; k++) {
		//nro de nodo
		arrayMensajesSerializarReduccLocal[j] = malloc(4 + 1);
		if (!arrayMensajesSerializarReduccLocal[j])
			perror("error de malloc");
		strcpy(arrayMensajesSerializarReduccLocal[j], intToArrayZerosLeft(nroNodoRecibido, 4));
		j++;
		//IP del nodo
		arrayMensajesSerializarReduccLocal[j] = malloc(string_length(getDatosGlobalesNodo(nroNodoRecibido)->ip) + 1);
		if (!arrayMensajesSerializarReduccLocal[j])
			perror("error de malloc");
		strcpy(arrayMensajesSerializarReduccLocal[j], getDatosGlobalesNodo(nroNodoRecibido)->ip);
		j++;
		//puerto del nodo
		arrayMensajesSerializarReduccLocal[j] = malloc(LARGO_PUERTO + 1);
		if (!arrayMensajesSerializarReduccLocal[j])
			perror("error de malloc");
		strcpy(arrayMensajesSerializarReduccLocal[j], intToArrayZerosLeft(getDatosGlobalesNodo(nroNodoRecibido)->puerto, LARGO_PUERTO));
		j++;
		//cantidad de temporales
		arrayMensajesSerializarReduccLocal[j] = malloc(4 + 1);
		if (!arrayMensajesSerializarReduccLocal[j])
			perror("error de malloc");
		strcpy(arrayMensajesSerializarReduccLocal[j], intToArrayZerosLeft(cantidadTemporalesTransformacion, 4));
		j++;
		//todos los temporales uno a continuación del otro
		for (h = 0; h < cantidadTemporalesTransformacion; h++) {
			//temporal h
//			printf("temporal %s\n", temporales[h]);
			arrayMensajesSerializarReduccLocal[j] = malloc(string_length(temporales[h]) + 1);
			if (!arrayMensajesSerializarReduccLocal[j])
				perror("error de malloc");
			strcpy(arrayMensajesSerializarReduccLocal[j], temporales[h]);
			j++;
		}
		arrayMensajesSerializarReduccLocal[j] = malloc(string_length(temporalReduccLocal) + 1);
		if (!arrayMensajesSerializarReduccLocal[j])
			perror("error de malloc");
		strcpy(arrayMensajesSerializarReduccLocal[j], temporalReduccLocal);
		j++;
	}

	//mensaje serializado
	char *mensajeSerializadoReduccLocal = serializarMensaje(TIPO_MSJ_TABLA_REDUCCION_LOCAL, arrayMensajesSerializarReduccLocal, cantStrings);
	for (j = 0; j < cantStrings; j++) {
		free(arrayMensajesSerializarReduccLocal[j]);
	}
	free(arrayMensajesSerializarReduccLocal);
	free(temporales);
	return mensajeSerializadoReduccLocal;
}

char* serializarMensajeReduccGlobal(int cantNodosReduccGlobal, struct filaTablaEstados *filasReduccGlobal, char* temporalRedGlobal) {
	int i, j;
	int cantStrings = 1 + cantNodosReduccGlobal * 4 + 1;
	char **arrayMensajesSerializarRedGlobal = malloc(sizeof(char*) * cantStrings);

	//cantidad de nodos
	j = 0;
	arrayMensajesSerializarRedGlobal[j] = malloc(4 + 1);
	if (!arrayMensajesSerializarRedGlobal[j])
		perror("error de malloc");
	strcpy(arrayMensajesSerializarRedGlobal[j], intToArrayZerosLeft(cantNodosReduccGlobal, 4));
	j++;
	int nroNodo;
	for (i = 0; i < cantNodosReduccGlobal; i++) {
		nroNodo = filasReduccGlobal[i].nodo;
//		printf("Nodo a serializar\nnroNodo %d\n", nroNodo);
//		printf("nombre %s - número %d - ip %s - puerto %d\n", getDatosGlobalesNodo(nroNodo)->nombre, getDatosGlobalesNodo(nroNodo)->numero, getDatosGlobalesNodo(nroNodo)->ip, getDatosGlobalesNodo(nroNodo)->puerto);

		//nro de nodo
		arrayMensajesSerializarRedGlobal[j] = malloc(4 + 1);
		if (!arrayMensajesSerializarRedGlobal[j])
			perror("error de malloc");
		strcpy(arrayMensajesSerializarRedGlobal[j], intToArrayZerosLeft(nroNodo, 4));
		j++;
		//IP del nodo
		arrayMensajesSerializarRedGlobal[j] = malloc(string_length(getDatosGlobalesNodo(nroNodo)->ip) + 1);
		if (!arrayMensajesSerializarRedGlobal[j])
			perror("error de malloc");
		strcpy(arrayMensajesSerializarRedGlobal[j], getDatosGlobalesNodo(nroNodo)->ip);
		j++;
		//puerto del nodo
		arrayMensajesSerializarRedGlobal[j] = malloc(LARGO_PUERTO + 1);
		if (!arrayMensajesSerializarRedGlobal[j])
			perror("error de malloc");
		strcpy(arrayMensajesSerializarRedGlobal[j], intToArrayZerosLeft(getDatosGlobalesNodo(nroNodo)->puerto, LARGO_PUERTO));
		j++;
		//temporal de la fila
		arrayMensajesSerializarRedGlobal[j] = malloc(string_length(filasReduccGlobal[i].temporal) + 1);
		if (!arrayMensajesSerializarRedGlobal[j])
			perror("error de malloc");
		strcpy(arrayMensajesSerializarRedGlobal[j], filasReduccGlobal[i].temporal);
		j++;
	}
	//temporal global
	arrayMensajesSerializarRedGlobal[j] = malloc(string_length(temporalRedGlobal) + 1);
	if (!arrayMensajesSerializarRedGlobal[j])
		perror("error de malloc");
	strcpy(arrayMensajesSerializarRedGlobal[j], temporalRedGlobal);
	j++;

	char *mensajeSerializadoRedGlobal = serializarMensaje(TIPO_MSJ_TABLA_REDUCCION_GLOBAL, arrayMensajesSerializarRedGlobal, cantStrings);
	for (j = 0; j < cantStrings; j++) {
		free(arrayMensajesSerializarRedGlobal[j]);
	}
	free(arrayMensajesSerializarRedGlobal);
	return mensajeSerializadoRedGlobal;
}

char* serializarMensajeAlmFinal(int nroNodoReduccGlobal, char *temporalReduccGlobal) {
	int i, j;
	int cantStrings = 4;
	char **arrayMensajesSerializar = malloc(sizeof(char*) * cantStrings);

	j = 0;
	//nro de nodo
	arrayMensajesSerializar[j] = malloc(4 + 1);
	if (!arrayMensajesSerializar[j])
		perror("error de malloc");
	strcpy(arrayMensajesSerializar[j], intToArrayZerosLeft(nroNodoReduccGlobal, 4));
	j++;
	//IP del nodo
	arrayMensajesSerializar[j] = malloc(string_length(getDatosGlobalesNodo(nroNodoReduccGlobal)->ip) + 1);
	if (!arrayMensajesSerializar[j])
		perror("error de malloc");
	strcpy(arrayMensajesSerializar[j], getDatosGlobalesNodo(nroNodoReduccGlobal)->ip);
	j++;
	//puerto del nodo
	arrayMensajesSerializar[j] = malloc(LARGO_PUERTO + 1);
	if (!arrayMensajesSerializar[j])
		perror("error de malloc");
	strcpy(arrayMensajesSerializar[j], intToArrayZerosLeft(getDatosGlobalesNodo(nroNodoReduccGlobal)->puerto, LARGO_PUERTO));
	j++;
	//temporal de la fila
	arrayMensajesSerializar[j] = malloc(string_length(temporalReduccGlobal) + 1);
	if (!arrayMensajesSerializar[j])
		perror("error de malloc");
	strcpy(arrayMensajesSerializar[j], temporalReduccGlobal);
	j++;

	char *mensajeSerializado = serializarMensaje(TIPO_MSJ_TABLA_ALMACENAMIENTO_FINAL, arrayMensajesSerializar, cantStrings);
	for (j = 0; j < cantStrings; j++) {
		free(arrayMensajesSerializar[j]);
	}
	free(arrayMensajesSerializar);
	return mensajeSerializado;
}

liberarCargaJob(int socketConectado, int nodoFallado) {
	datosMasterJob *masterJobActual = getDatosMasterJobByFD(socketConectado);

	int cantNodosUsados = (int) masterJobActual->cantNodosUsados;
	int i, j, k;
	for (i = 0; i < cantNodosUsados; i++) {
//		printf("carga antes de restar, nodo %d - %d: %d\n", masterJobActual->nodosUsados[i].numero, getDatosGlobalesNodo(masterJobActual->nodosUsados[i].numero)->numero, getDatosGlobalesNodo(masterJobActual->nodosUsados[i].numero)->carga);
		disminuirCargaGlobalNodo(masterJobActual->nodosUsados[i].numero, masterJobActual->nodosUsados[i].cantidadVecesUsados);
		//getDatosGlobalesNodo(masterJobActual->nodosUsados[i].numero)->carga -= masterJobActual->nodosUsados[i].cantidadVecesUsados;
//		printf("carga después de restar, nodo %d - %d: %d\n", masterJobActual->nodosUsados[i].numero, getDatosGlobalesNodo(masterJobActual->nodosUsados[i].numero)->numero, getDatosGlobalesNodo(masterJobActual->nodosUsados[i].numero)->carga);
	}

	int cargaReduccGlobal = masterJobActual->cantBloquesArchivo;
	if (cargaReduccGlobal % 2 != 0)
		cargaReduccGlobal++;
	int cantidadRestar = cargaReduccGlobal / 2;
	disminuirCargaGlobalNodo(masterJobActual->nodoReduccGlobal, cantidadRestar);

	if (nodoFallado >= 0)
		actualizarCargaGlobalNodo(nodoFallado, 0);
}

int replanificar(datosMasterJob *masterJobActual, int nroNodoRecibido, int socketConectado) {
	int i, j, k;
	int cantBloquesArchivo = masterJobActual->cantBloquesArchivo;
	//busca las copias y las envía a master
	//modifica las filas con FIN_OK a FIN_OK_REPLANIFICADO
	printf("masterJobActual->nroJob: %d\n", masterJobActual->nroJob);
	printf("masterJobActual->nroMaster: %d\n", masterJobActual->nroMaster);
	printf("nroNodoRecibido: %d\n", nroNodoRecibido);

	int cantFilasOkReplanificadas = modificarEstadoFilasTablaEstadosByJMNEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, TRANSFORMACION, FIN_OK, FIN_OK_REPLANIFICADO);
	printf("cantFilasOkReplanificadas: %d\n", cantFilasOkReplanificadas);
	//busca la cantidad de transformaciones que tenía asignadas el nodo para replanificarlas
	int lalala = getCantFilasByJMNEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, TRANSFORMACION, ERROR);
	printf("lalala: %d\n", lalala);
	int cantFilasParaReplanificar = cantFilasOkReplanificadas + lalala;

	struct filaTablaEstados filasReplanifTransf[cantFilasParaReplanificar];

	struct filaTablaEstados filaBusqueda;
	filaBusqueda.job = masterJobActual->nroJob;
	filaBusqueda.master = masterJobActual->nroMaster;
	filaBusqueda.nodo = nroNodoRecibido;
	filaBusqueda.bloque = 0;
	filaBusqueda.etapa = TRANSFORMACION;
	strcpy(filaBusqueda.temporal, "");
	filaBusqueda.estado = 0;
	filaBusqueda.siguiente = NULL;
	//busca las filas a replanificar en la tabla de estados
	int cantFilasEncontradas = buscarMuchosElemTablaEstados(filasReplanifTransf, filaBusqueda);
	printf("cantFilasEncontradas: %d\n", cantFilasEncontradas);
	printf("cantFilasParaReplanificar: %d\n", cantFilasParaReplanificar);
	if (cantFilasEncontradas == 0) {
		puts("no se encontró ninguna fila de la tabla de estados para hacer la replanificación");
		log_error(logYAMA, "No se encontró ninguna fila de la tabla de estados para hacer la replanificación");
		return -1;
	}
	if (cantFilasParaReplanificar != cantFilasEncontradas) {
		puts("hubo un error en la cantidad de filas encontradas");
		log_error(logYAMA, "Hubo un error en la cantidad de filas encontradas");
		return -1;
	}

	int nodoSuplenteEncontrado = 0, abortado = 0, cantNodosRecargados = 0;
	int largoVectorNodosAuxiliar = 50;
	int nodosRecargadosAuxiliar[largoVectorNodosAuxiliar];
	for (j = 0; j < largoVectorNodosAuxiliar; j++) {
		nodosRecargadosAuxiliar[j] = 0;
	}
	nodoParaAsignar asignacionesNodos[cantFilasParaReplanificar];
	for (j = 0; j < cantFilasParaReplanificar; j++) {
		nodoSuplenteEncontrado = 0;
		//arma el vector de asignaciones nuevas para enviar a master
		for (i = 0; i < cantBloquesArchivo; i++) {
			if (filasReplanifTransf[j].nodo == bloquesArchivoXFD[socketConectado][i].nodoUsado && filasReplanifTransf[j].bloque == bloquesArchivoXFD[socketConectado][i].bloqueUsado) {
				//tomar el nodo suplente para armar el array a enviar
				asignacionesNodos[j].nroNodo = bloquesArchivoXFD[socketConectado][i].nodoSuplente;
				asignacionesNodos[j].bloque = bloquesArchivoXFD[socketConectado][i].bloqueSuplente;
				asignacionesNodos[j].bytesOcupados = bloquesArchivoXFD[socketConectado][i].bytes;
				nodoSuplenteEncontrado = 1;
				int nodoAgregado = 0;
				for (k = 0; k < largoVectorNodosAuxiliar && !nodoAgregado;
						k++) {
					if (nodosRecargadosAuxiliar[k] == asignacionesNodos[j].nroNodo) {
						//no lo agrega
						nodoAgregado = 1;
					} else if (nodosRecargadosAuxiliar[k] == 0) {
						nodosRecargadosAuxiliar[k] = asignacionesNodos[j].nroNodo;
						nodoAgregado = 1;
						cantNodosRecargados++;
					}
				}
			}
		}
		//si no se pudo encontrar algún nodo suplente se aborta el job
		if (nodoSuplenteEncontrado) {
			puts("No se pudo encontrar un nodo suplente. Se aborta el Job");
			log_error(logYAMA, "No se pudo encontrar un nodo suplente. Se aborta el Job");
			mostrarTablaEstados();
			abortado = 1;
			//disminuye la cantidad de nodos usados por el Job debido al que se cayó
			masterJobActual->cantNodosUsados--;
			int cantNodosUsados = masterJobActual->cantNodosUsados;

			//marca la reducción local en los nodos replanificados
			int nodosRecargados[cantNodosRecargados];
			for (i = 0; i < cantNodosRecargados; i++) {
				nodosRecargados[i] = nodosRecargadosAuxiliar[i];
				//tengo que buscar las filas por master, job, nodo, etapa reduccion local, estado fin_ok y ponerle replanificado
				modificarEstadoFilasTablaEstadosByJMNEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, nodosRecargados[i], REDUCC_LOCAL, FIN_OK, FIN_OK_REPLANIFICADO);
			}
			return -2;
		}
	}
	/*
	 * lo que hace hasta acá:
	 * busca en la tabla de estados todas las transformaciones que debe replanificar y les actualiza el estado
	 * busca los nodos donde se encuentran las copias de cada uno de los bloques que se deben transformar nuevamente
	 * verifica que si no se encuentra alguna de las copias se aborte el job
	 * arma el vector de asignaciones nuevas para enviar a master
	 * disminuye la cantidad de nodos usados en el job
	 */
	//disminuye la cantidad de nodos usados por el Job debido al que se cayó
	masterJobActual->cantNodosUsados--;
	int cantNodosUsados = masterJobActual->cantNodosUsados;

	//marca la reducción local en los nodos replanificados
	int nodosRecargados[cantNodosRecargados];
	for (i = 0; i < cantNodosRecargados; i++) {
		nodosRecargados[i] = nodosRecargadosAuxiliar[i];
		//tengo que buscar las filas por master, job, nodo, etapa reduccion local, estado fin_ok y ponerle replanificado
		modificarEstadoFilasTablaEstadosByJMNEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, nodosRecargados[i], REDUCC_LOCAL, FIN_OK, FIN_OK_REPLANIFICADO);
	}
	if (!abortado) { //si no se encontró alguna copia se aborto anteriormente el Job
		struct filaTablaEstados fila;
		for (i = 0; i < cantFilasParaReplanificar; i++) {

			//le sumo 1 a la cantidad de veces que se usa el nodo reasignado
			masterJobActual->nodosUsados[asignacionesNodos[i].nroNodo].cantidadVecesUsados++;

			//genera una fila en la tabla de estados
			fila.job = masterJobActual->nroJob;
			fila.master = masterJobActual->nroMaster;
			fila.nodo = asignacionesNodos[i].nroNodo;
			fila.bloque = asignacionesNodos[i].bloque;
			fila.etapa = TRANSFORMACION;
			char* temporal = string_from_format("m%dj%dn%db%de%d", fila.master, fila.job, fila.nodo, fila.bloque, fila.etapa);
			strcpy(fila.temporal, temporal);
			fila.estado = EN_PROCESO;
			fila.siguiente = NULL;
			if (!agregarElemTablaEstados(fila)) {
				log_error(logYAMA, "Ocurrió un error al agregar un elemento la tabla de estados en la etapa de replanificación");
				perror("Error al agregar elementos a la tabla de estados");
			}
			//guarda el archivo temporal en el vector que se va a usar
			//en la tabla de transformación para el master
			strcpy(asignacionesNodos[i].temporal, temporal);
		}

		/* ****** envío de nodos para la transformación ******************* */
		//envía al master la lista de nodos donde trabajar cada bloque
		char *mensajeSerializado = serializarMensajeTransformacion(asignacionesNodos, cantFilasParaReplanificar);
		enviarMensaje(socketConectado, mensajeSerializado);
		/* **************************************************************** */
		mostrarTablaEstados();
		/*
		 * lo que hace hasta acá:
		 * genera en la tabla de estados las filas de las nuevas transformaciones producto de la replanificación
		 * serializa las asignaciones y las envía a master
		 */

		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//TODO: revisar esto
		//elegir otro nodo para reducción global si el caído era el asignado para eso
		int nroNodoReduccGlobal = (int) getDatosMasterJobByFD(socketConectado)->nodoReduccGlobal;
		if (nroNodoRecibido == nroNodoReduccGlobal) {
			//int cantNodosUsados = masterJobActual->cantNodosUsados;
			//armo un vector auxiliar para ordenar y elegir el de menor carga
			datosPropiosNodo nodosUsadosAuxiliar[cantNodosUsados];
			for (i = 0; i < cantNodosUsados; i++) {
				nodosUsadosAuxiliar[i].numero = masterJobActual->nodosUsados[i].numero;
				nodosUsadosAuxiliar[i].carga = getCargaGlobalNodo(nodosUsadosAuxiliar[i].numero);
			}
			//ordeno los nodos de menor a mayor por carga
			datosPropiosNodo temp;
			for (i = 0; i < cantNodosUsados; i++) {
				for (j = 0; j < (cantNodosUsados - 1); j++) {
					if (nodosUsadosAuxiliar[j].carga > nodosUsadosAuxiliar[j + 1].carga) {
						temp = nodosUsadosAuxiliar[j];
						nodosUsadosAuxiliar[j] = nodosUsadosAuxiliar[j + 1];
						nodosUsadosAuxiliar[j + 1] = temp;
					}
				}
			}
			asignarNodoReduccGlobal(nodosUsadosAuxiliar[0].numero, socketConectado);
		}
	}
	return 1;
}

int main(int argc, char *argv[]) {
	//manejo de señales
	signal(SIGINT, sig_handler);
//	signal(SIGUSR1, sig_handler);
	//manejo de señales con el pselect()
	struct sigaction sa;
	sigset_t emptyset, blockset;
	sigemptyset(&blockset); /* Block SIGUSR1 */
//	sigaddset(&blockset, SIGINT);
	sigaddset(&blockset, SIGUSR1);
	sigprocmask(SIG_BLOCK, &blockset, NULL);
	sa.sa_handler = sig_handler; /* Establish signal handler */
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR1, &sa, NULL);
//	sigaction(SIGINT, &sa, NULL);

	//sigset_t new_set, old_set;
	//sigemptyset(&new_set);
	//sigaddset(&new_set, SIGINT);
	//sigaddset(&new_set, SIGUSR1);
	//sigprocmask(SIG_BLOCK, &new_set, &old_set);

	crearCarpetaDeLog(carpeta_log);
	logYAMA = log_create("../log/logYAMA.log", "YAMA", false, LOG_LEVEL_TRACE); //creo el logger, sin mostrar por pantalla
	int h, i, j, k;
	char mensajeHeaderSolo[4];
	int32_t headerId;
	log_info(logYAMA, "Iniciando proceso YAMA");
	printf("\n*** Proceso Yama ***\n");
	//para el select
	fd_set socketsLecturaMaster;    // master file descriptor list
	fd_set socketsLecturaTemp;  // temp file descriptor list for select()
	int maxFD;        // maximum file descriptor number
	// clear the set ahead of time
	FD_ZERO(&socketsLecturaMaster);
	FD_ZERO(&socketsLecturaTemp);

	if (!getDatosConfiguracion()) {
		log_error(logYAMA, "No se pudieron obtener los datos del archivo de configuración. Se aborta la ejecución");
		log_info(logYAMA, "Server cerrado");
		log_destroy(logYAMA);
		return EXIT_FAILURE;
	}
	//para la planificación
	disponibBase = atoi(datosConfigYama[DISPONIBILIDAD_BASE]);
	strcpy(algoritmoPlanificacion, datosConfigYama[ALGORITMO_BALANCEO]);
	retardoPlanificacion = atoi(datosConfigYama[RETARDO_PLANIFICACION]);

	/* ************** conexión como cliente al FS *************** */
	int socketFS, preparadoFs = 0;

	int modulo = yama;

	while (preparadoFs == 0) {
		if ((socketFS = conexionAFileSystem()) < 0) {
			log_error(logYAMA, "No se pudo conectar al FileSystem. Se aborta la ejecución");
			log_info(logYAMA, "Server cerrado");
			log_destroy(logYAMA);

			puts("Abortar ejecución");
			return EXIT_FAILURE;
		}
		send(socketFS, &modulo, sizeof(int), MSG_WAITALL);
		headerId = deserializarHeader(socketFS);
		if (headerId == TIPO_MSJ_HANDSHAKE_RESPUESTA_OK) {
			preparadoFs = 1;
			log_info(logYAMA, "El Filesystem está estable. Se puede continuar la ejecución");
			puts("Conectado a FileSystem");
		} else {
			log_info(logYAMA, "Se está esperando que el Filesystem esté estable");
			puts("Esperando que el Filesystem esté estable");
			sleep(5);
		}
	}

	// add the socketFs to the master set
	//FD_SET(socketFS, &socketsLecturaMaster);
	// keep track of the biggest file descriptor
	//maxFD = socketFS; // so far, it's this one
	/* ************** inicialización como server ************ */
	int listenningSocket;
	if ((listenningSocket = inicializoComoServidor()) < 0) {
		log_error(logYAMA, "No se puede iniciar como servidor. Se aborta la ejecución");
		log_info(logYAMA, "Server cerrado");
		log_destroy(logYAMA);
		return EXIT_FAILURE;
	}
	// add the listener to the master set
	FD_SET(listenningSocket, &socketsLecturaMaster);
	// keep track of the biggest file descriptor
	maxFD = listenningSocket; // so far, it's this one

	int socketCliente, socketConectado, cantStrings, bytesRecibidos = 0,
			nroSocket, nroNodoReduccGlobal, nroNodoRecibido, nroBloqueRecibido,
			nroNodoAlmacFinal, cantNodosArchivo, cantNodosUsados,
			cantTransformaciones;
	datosMasterJob *masterJobActual;

	setSizeListaGlobalNodos(50);
	for (i = 0; i < getLargoListaGlobalNodos(); i++) {
		actualizarCargaGlobalNodo(i, 0);
		getDatosGlobalesNodo(i)->numero = 0;
	}
	for (;;) {
		//		if (select(maxFD + 1, &socketsLecturaTemp, NULL, NULL, NULL) == -1) {
		socketsLecturaTemp = socketsLecturaMaster;
		sigemptyset(&emptyset);
		if (pselect(maxFD + 1, &socketsLecturaTemp, NULL, NULL, NULL, &emptyset) == -1) { // para el manejo de signals, no borrar!
			log_error(logYAMA, "Ocurrió un error en el select() principal");
			perror("Error en select()");
			break;
		} else {
			for (nroSocket = 0; nroSocket <= maxFD; nroSocket++) {
				if (FD_ISSET(nroSocket, &socketsLecturaTemp)) {
					if (nroSocket == listenningSocket) {	//conexión nueva
						if ((socketCliente = recibirConexion(listenningSocket)) >= 0) {
							int32_t headerId = deserializarHeader(socketCliente);

							if (headerId == TIPO_MSJ_HANDSHAKE) {
								int cantidadMensajes = protocoloCantidadMensajes[headerId];
								char **arrayMensajesRHS = deserializarMensaje(socketCliente, cantidadMensajes);
								int idEmisorMensaje = atoi(arrayMensajesRHS[0]);
								free(arrayMensajesRHS);
								if (idEmisorMensaje == NUM_PROCESO_MASTER) {
									FD_SET(socketCliente, &socketsLecturaMaster); // add to master set
									if (socketCliente > maxFD) { // keep track of the max
										maxFD = socketCliente;
									}
									//se crea el elemento en la lista de datosMasterJob
									asignarDatosMasterJob(getNuevoNroMaster(), getNuevoNroJob(), socketCliente);
									enviarHeaderSolo(socketCliente, TIPO_MSJ_HANDSHAKE_RESPUESTA_OK);
									log_info(logYAMA, "Handshake verificado. Se acepta una nueva conexión de un Master y se la comienza a escuchar");

								} else {
									log_info(logYAMA, "Handshake denegado. No se acepta la nueva conexión");
									enviarHeaderSolo(socketCliente, TIPO_MSJ_HANDSHAKE_RESPUESTA_DENEGADO);
								}
							}
						}
					} else {	//conexión preexistente
						/* *************************** recepción de un mensaje ****************************/
						socketConectado = nroSocket;
						log_info(logYAMA, "Se recibió un mensaje de proceso conectado por FD %d", socketConectado);
						masterJobActual = getDatosMasterJobByFD(socketConectado);
						cantNodosArchivo = masterJobActual->cantNodosUsados;
						cantNodosUsados = masterJobActual->cantNodosUsados;

						printf("\nSocket conectado: %d\n", socketConectado);
						printf("Master actual: %d\n", masterJobActual->nroMaster);
						printf("Job actual: %d\n", masterJobActual->nroJob);
						for (i = 0; i < masterJobActual->cantNodosUsados; i++) {
							printf("\nCarga del nodo %d al recibir un pedido: %d\n", masterJobActual->nodosUsados[i].numero, getCargaGlobalNodo(masterJobActual->nodosUsados[i].numero));
						}
						int32_t headerId = deserializarHeader(socketConectado);
//						printf("\nHeader Id: %d\n", headerId);
						printf("Header mensaje: %s\n", protocoloMensajesPredefinidos[headerId]);
						if (headerId <= 0) {//error o desconexión de un cliente
							log_info(logYAMA, "Se desconectó el proceso conectado por FD %d. Se lo deja de escuchar", socketConectado);
							printf("Se desconectó el proceso conectado por FD %d. Se lo deja de escuchar\n", socketConectado);
							//disminuye la carga de los nodos asociados al master que falló
							liberarCargaJob(socketConectado, -1);

							//actualiza el estado en la tabla de estados de las tareas asociadas al master que falló
							//a las tareas que están en proceso las pone como ERROR
							modificarEstadoFilasTablaEstadosByJMEs(masterJobActual->nroJob, masterJobActual->nroMaster, EN_PROCESO, ERROR);
							mostrarTablaEstados();
							eliminarElemDatosMasterJobByFD(socketConectado);
							cerrarCliente(socketConectado); // bye!
							FD_CLR(socketConectado, &socketsLecturaMaster); // remove from master set
						}
						int cantidadMensajes = protocoloCantidadMensajes[headerId];
						char **arrayMensajes = deserializarMensaje(socketConectado, cantidadMensajes);
						switch (headerId) {

						case TIPO_MSJ_PATH_ARCHIVO_TRANSFORMAR:
							;
							char *archivo = malloc(string_length(arrayMensajes[0]) + 1);
							strcpy(archivo, arrayMensajes[0]);
							free(arrayMensajes);
							//pide la metadata del archivo al FS
							if (pedirMetadataArchivoFS(socketFS, archivo) > 0) {
								/* ************* solicitud de info del archivo al FS *************** */
								//recibir las partes del archivo
								int32_t headerId = deserializarHeader(socketFS);
//								printf("header recibido del filesystem: %d - %s\n", headerId, protocoloMensajesPredefinidos[headerId]);
								if (headerId != TIPO_MSJ_METADATA_ARCHIVO) {
									log_error(logYAMA, "El FS no mandó los bloques");
									puts("El FS no mandó los bloques");
									enviarHeaderSolo(socketConectado, TIPO_MSJ_NO_EXISTE_ARCHIVO_EN_FS);
									break;
								}
								masterJobActual->cantBloquesArchivo = getCantidadPartesArchivoFS(socketFS, protocoloCantidadMensajes[headerId]);
//								printf("masterJobActual->cantBloquesArchivo: %d\n", masterJobActual->cantBloquesArchivo);
								bloqueArchivo *bloques = recibirMetadataArchivoFS(socketFS, masterJobActual->cantBloquesArchivo);
								printf("\n ---------- Lista de bloques del archivo devuelto por FS ---------- \n");
								for (i = 0;
										i < masterJobActual->cantBloquesArchivo;
										i++) {
									printf("nodoCopia1 %d - bloqueCopia1 %d - nodoCopia2 %d - bloqueCopia2 %d - bytes %d\n", bloques[i].nodoCopia1, bloques[i].bloqueCopia1, bloques[i].nodoCopia2, bloques[i].bloqueCopia2, bloques[i].bytesBloque);
								}
								/* ********************* */
								//recibir la info de los nodos donde están esos archivos
								headerId = deserializarHeader(socketFS);
//								printf("Header Id: %d\n", headerId);
								printf("Header mensaje: %s\n", protocoloMensajesPredefinidos[headerId]);
								if (headerId != TIPO_MSJ_DATOS_CONEXION_NODOS) {
									log_error(logYAMA, "El FS no mandó los nodos");
									puts("El FS no mandó los nodos");
									enviarHeaderSolo(socketConectado, TIPO_MSJ_NO_EXISTE_ARCHIVO_EN_FS);
									break;
								}
								cantNodosArchivo = getCantidadNodosFS(socketFS, protocoloCantidadMensajes[headerId]);
								setSizeListaGlobalNodos(cantNodosArchivo);
								//guardar los nodos en la listaGlobal
								datosPropiosNodo nodosParaPlanificar[cantNodosArchivo];
								masterJobActual->cantNodosUsados = cantNodosArchivo;
								masterJobActual->nodosUsados = malloc(sizeof(struct nodosUsadosPlanificacion) * cantNodosArchivo);
								recibirNodosArchivoFS(socketFS, cantNodosArchivo, nodosParaPlanificar);
								printf("\n ---------- Lista global de nodos ---------- \n");
								for (k = 0; k < cantNodosArchivo; k++) {
									masterJobActual->nodosUsados[k].numero = nodosParaPlanificar[k].numero;
//									printf("%d\n", masterJobActual->nodosUsados[k].numero);
									masterJobActual->nodosUsados[k].cantidadVecesUsados = 0;
									masterJobActual->nodosUsados[k].cantTransformaciones = 0;
//									printf("%d\n", masterJobActual->nodosUsados[k].cantidadVecesUsados);
								}
								/* ***************************************************************** */

								/* ************* inicio planificación *************** */
								//le paso el vector donde debe ir guardando las asignaciones de nodos planificados, indexado por partes del archivo
								nodoParaAsignar asignacionesNodos[masterJobActual->cantBloquesArchivo];
								printf("\n ---------- Lista de nodos para planificación ---------- \n");
								for (i = 0; i < cantNodosArchivo; i++) {
									printf("Nro nodo %d - Carga %d\n", nodosParaPlanificar[i].numero, getDatosGlobalesNodo(nodosParaPlanificar[i].numero)->carga);
								}

								planificar(socketConectado, bloques, asignacionesNodos, masterJobActual->cantBloquesArchivo, cantNodosArchivo, nodosParaPlanificar);

//								for (i = 0; i < cantNodosArchivo; i++) {
//									printf("Carga global: Nro nodo %d - Carga %d\n", getDatosGlobalesNodo(nodosParaPlanificar[i].numero)->numero, getDatosGlobalesNodo(nodosParaPlanificar[i].numero)->carga);
//								}
								//guardo el nodo donde se va a hacer la reducción global de ese master y job
								asignarNodoReduccGlobal(nodosParaPlanificar[0].numero, socketConectado);
								printf("nodoReduccGlobal en fin de planificación: %d\n", masterJobActual->nodoReduccGlobal);

								/* ************* fin planificación *************** */

								/* ************** agregado en tabla de estados *************** */
								//guarda la info de los bloques del archivo en la tabla de estados
								struct filaTablaEstados fila;
								for (i = 0;
										i < masterJobActual->cantBloquesArchivo;
										i++) {
									for (k = 0; k < cantNodosArchivo; k++) {
										if (masterJobActual->nodosUsados[k].numero == asignacionesNodos[i].nroNodo) {
											masterJobActual->nodosUsados[k].cantidadVecesUsados++;
											masterJobActual->nodosUsados[k].cantTransformaciones++;
										}
									}
									//printf("parte de archivo %d asignado a: nodo %d - bloque %d\n", i, asignacionesNodos[i][0], asignacionesNodos[i][1]);
									//genera una fila en la tabla de estados
									fila.job = masterJobActual->nroJob;
									fila.master = masterJobActual->nroMaster;
									fila.nodo = asignacionesNodos[i].nroNodo;
									fila.bloque = asignacionesNodos[i].bloque;
									fila.etapa = TRANSFORMACION;
									char* temporal = string_from_format("m%dj%dn%db%de%d", fila.master, fila.job, fila.nodo, fila.bloque, fila.etapa);
									strcpy(fila.temporal, temporal);
									fila.estado = EN_PROCESO;
									fila.siguiente = NULL;
									if (!agregarElemTablaEstados(fila)) {
										log_error(logYAMA, "Ocurrió un error al agregar un elemento la tabla de estados en la etapa de pedido de bloques de archivo");
										perror("Error al agregar elementos a la tabla de estados");
									}

									//guarda el archivo temporal en el vector que se va a usar
									//en la tabla de transformación para el master
									strcpy(asignacionesNodos[i].temporal, temporal);
								}
								/* ************** fin agregado en tabla de estados *************** */

								/* ****** envío de nodos para la transformación ******************* */
								//envía al master la lista de nodos donde trabajar cada bloque
								char *mensajeSerializado = serializarMensajeTransformacion(asignacionesNodos, masterJobActual->cantBloquesArchivo);
								enviarMensaje(socketConectado, mensajeSerializado);
								/* **************************************************************** */
								mostrarTablaEstados();
							} else {
								perror("No se pudo pedir el archivo al FS");
							}
							break;

						case TIPO_MSJ_TRANSFORMACION_OK:
							;
							nroNodoRecibido = atoi(arrayMensajes[0]);
							nroBloqueRecibido = atoi(arrayMensajes[1]);
							log_info(logYAMA, "Se recibió mensaje de fin de transformación OK, nodo %d, bloque %d", nroNodoRecibido, nroBloqueRecibido);
							printf("Nodo recibido: %d\n", nroNodoRecibido);
							printf("Bloque recibido: %d\n", nroBloqueRecibido);
							free(arrayMensajes);

							disminuirCargaGlobalNodo(nroNodoRecibido, 1);
							for (k = 0; k < cantNodosArchivo; k++) {
								if (masterJobActual->nodosUsados[k].numero == nroNodoRecibido) {
									masterJobActual->nodosUsados[k].cantidadVecesUsados--;
								}
							}
							//pongo la fila en estado FIN_OK
							if (modificarEstadoFilasTablaEstados(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, nroBloqueRecibido, TRANSFORMACION, EN_PROCESO, FIN_OK) == 0) {
								log_error(logYAMA, "Ocurrió un error al modificar la tabla de estados en el fin de la transformación OK");
								puts("No se pudo modificar ninguna fila de la tabla de estados");
							} else {
								log_info(logYAMA, "Se modificó la tabla de estados en el fin de la transformación OK");
							}

							for (k = 0; k < cantNodosArchivo; k++) {
								if (masterJobActual->nodosUsados[k].numero == nroNodoRecibido) {
									cantTransformaciones = masterJobActual->nodosUsados[k].cantTransformaciones;
								}
							}
							int cantidadFilasTransformacionFinOk = getCantFilasByJMNEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, TRANSFORMACION, FIN_OK);
							int cantidadFilasTransformacionError = getCantFilasByJMNEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, TRANSFORMACION, ERROR);
							int cantidadFilasTransformacionEnProceso = getCantFilasByJMNEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, TRANSFORMACION, EN_PROCESO);
							//si no queda ninguna fila de ese nodo en proceso inicia la reducción local
							if (cantTransformaciones > (cantidadFilasTransformacionFinOk + cantidadFilasTransformacionError + cantidadFilasTransformacionEnProceso)) {
								puts("Hubo algún error en la asignación de estados en la Tabla de Estados");
								log_error(logYAMA, "Hubo algún error en la asignación de estados en la Tabla de Estados para las transformaciones del nodo %d", nroNodoRecibido);
							} else if (cantidadFilasTransformacionFinOk == cantTransformaciones) {
								//inicia la reducción local del nodo recibido

								/* ************** agregado de la fila de reducción local en tabla de estados *************** */
								struct filaTablaEstados fila;
								fila.job = masterJobActual->nroJob;
								fila.master = masterJobActual->nroMaster;
								fila.nodo = nroNodoRecibido;
								fila.bloque = 0;
								fila.etapa = REDUCC_LOCAL;
								char* temporalRedLocal = string_from_format("m%dj%dn%de%d", fila.master, fila.job, fila.nodo, fila.etapa);
								strcpy(fila.temporal, temporalRedLocal);
								fila.estado = EN_PROCESO;
								fila.siguiente = NULL;

								if (!agregarElemTablaEstados(fila)) {
									log_error(logYAMA, "Ocurrió un error al agregar un elemento la tabla de estados en fin de transformación OK");
									perror("Error al agregar elementos a la tabla de estados");
								}
								/* *********** enviar data para reducción local ********** */
								//obtener los registros de la tabla de estados, cantidad y temporales
								char *mensajeSerializadoRedLocal = serializarMensajeReduccLocal(nroNodoRecibido, masterJobActual, temporalRedLocal);
//								printf("\nmensaje serializado para reducción local: %s\n", mensajeSerializadoRedLocal);
								enviarMensaje(socketConectado, mensajeSerializadoRedLocal);
								aumentarCargaGlobalNodo(nroNodoRecibido, 1);
								for (k = 0; k < cantNodosArchivo; k++) {
									if (masterJobActual->nodosUsados[k].numero == nroNodoRecibido) {
										masterJobActual->nodosUsados[k].cantidadVecesUsados++;
									}
								}
								log_trace(logYAMA, "Se da inicio a la Reducción Local en el nodo %d", nroNodoRecibido);
								puts("\nlista de elementos luego de enviar la tabla de reducción local");
								mostrarTablaEstados();
							} else if (cantidadFilasTransformacionEnProceso == 0 && cantidadFilasTransformacionError > 0) {
								//replanificar
								int resultado = replanificar(masterJobActual, nroNodoRecibido, socketConectado);
								if (resultado < 0) {
									puts("error al replanificar");
									log_error(logYAMA, "Ocurrió un error al replanificar el nodo %d", nroNodoRecibido);
									if (resultado == -2) {
										enviarHeaderSolo(socketConectado, TIPO_MSJ_ABORTAR_JOB);
										liberarCargaJob(socketConectado, nroNodoRecibido);
										//elimino el elemento con el socketConectado de la lista de datosMasterJob
										eliminarElemDatosMasterJobByFD(socketConectado);
										cerrarCliente(socketConectado);
										FD_CLR(socketConectado, &socketsLecturaMaster); // remove from master set
										mostrarTablaEstados();
										break;
									}
								}

							}
							break;
						case TIPO_MSJ_TRANSFORMACION_ERROR:
							;
							nroNodoRecibido = atoi(arrayMensajes[0]);
							nroBloqueRecibido = atoi(arrayMensajes[1]);

							log_info(logYAMA, "Se recibió mensaje de fin de transformación ERROR, nodo %d, bloque %d. Se intenta replanificar.", nroNodoRecibido, nroBloqueRecibido);
							printf("Nodo recibido: %d\n", nroNodoRecibido);
							printf("Bloque recibido: %d\n", nroBloqueRecibido);
							free(arrayMensajes);

							disminuirCargaGlobalNodo(nroNodoRecibido, 1);
							for (k = 0; k < cantNodosUsados; k++) {
								if (masterJobActual->nodosUsados[k].numero == nroNodoRecibido) {
									masterJobActual->nodosUsados[k].cantidadVecesUsados -= 1;
								}
							}

							//modifica en la tabla de estados el estado de la fila del bloque que falló
							if (modificarEstadoFilasTablaEstados(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, nroBloqueRecibido, TRANSFORMACION, EN_PROCESO, ERROR) == 0) {
								log_error(logYAMA, "Ocurrió un error al modificar la tabla de estados");
								puts("No se pudo modificar ninguna fila de la tabla de estados");
							} else {
								log_info(logYAMA, "Se modificó la tabla de estados en el fin de la transformación ERROR");
							}

							/*
							 * lo que hace hasta acá:
							 * disminuye la carga del nodo en 1, global y propia del job
							 * modifica el estado en la tabla de estados a ERROR
							 */

							//si el nodo recibido como caído es la copia del bloque implica que ya se había
							//caído anteriormente y ya no hay copia disponible. Se aborta el Job
							int cantBloquesArchivo = masterJobActual->cantBloquesArchivo;
							if (esCopiaDelBloque(nroNodoRecibido, nroBloqueRecibido, cantBloquesArchivo, socketConectado)) {
								//aborta el job
								enviarHeaderSolo(socketConectado, TIPO_MSJ_ABORTAR_JOB);
								liberarCargaJob(socketConectado, nroNodoRecibido);
								eliminarElemDatosMasterJobByFD(socketConectado);
								cerrarCliente(socketConectado);
								FD_CLR(socketConectado, &socketsLecturaMaster); // remove from master set
								mostrarTablaEstados();
								log_info(logYAMA, "No se pudo replanificar el Job %d cuando se desconectó el nodo %d", masterJobActual->nroJob, nroNodoRecibido);
							} else if (getCantFilasByJMNEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, TRANSFORMACION, EN_PROCESO) == 0) {
								/*
								 * lo que hace hasta acá:
								 * verifica que el nodo recibido no sea una copia producto de una replanificación anterior
								 * si no es así verifica que no haya transformaciones en proceso
								 * en cuyo caso inicia la replanificación
								 */
								int resultado = replanificar(masterJobActual, nroNodoRecibido, socketConectado);
								if (resultado < 0) {
									puts("error al replanificar");
									log_error(logYAMA, "Ocurrió un error al replanificar el nodo %d", nroNodoRecibido);
									if (resultado == -2) {
										enviarHeaderSolo(socketConectado, TIPO_MSJ_ABORTAR_JOB);
										liberarCargaJob(socketConectado, nroNodoRecibido);
										//elimino el elemento con el socketConectado de la lista de datosMasterJob
										eliminarElemDatosMasterJobByFD(socketConectado);
										cerrarCliente(socketConectado);
										FD_CLR(socketConectado, &socketsLecturaMaster); // remove from master set
										mostrarTablaEstados();
										break;
									}
								}

							}
							break;

						case TIPO_MSJ_REDUCC_LOCAL_OK:
							;
							nroNodoRecibido = atoi(arrayMensajes[0]);
							log_info(logYAMA, "Se recibió mensaje de fin de Reducción Local OK, nodo %d.", nroNodoRecibido);
							printf("nroNodoRecibido: %d\n", nroNodoRecibido);
							free(arrayMensajes);
							disminuirCargaGlobalNodo(nroNodoRecibido, 1);
							for (k = 0; k < cantNodosArchivo; k++) {
								if (masterJobActual->nodosUsados[k].numero == nroNodoRecibido) {
									masterJobActual->nodosUsados[k].cantidadVecesUsados--;
								}
							}

							//modificar el estado en la tabla de estados
							if (modificarEstadoFilasTablaEstados(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, 0, REDUCC_LOCAL, EN_PROCESO, FIN_OK) == 0) {
								log_error(logYAMA, "Ocurrió un error al modificar la tabla de estados en el fin de la Reducción Local OK");
								puts("No se pudo modificar ninguna fila de la tabla de estados");
							} else {
								log_info(logYAMA, "Se modificó la tabla de estados en el fin de la Reducción Local OK");
							}
							//verificar que todos los nodos del job y master hayan terminado la reducción local
//							if (getCantFilasByJMEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, REDUCC_LOCAL, EN_PROCESO) == 0) {
							if (getCantFilasByJMEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, REDUCC_LOCAL, FIN_OK) == cantNodosUsados) {
								/* ************** agregado de la fila de reducción global en tabla de estados *************** */
								int nodoReduccGlobal = (int) getDatosMasterJobByFD(socketConectado)->nodoReduccGlobal;

								struct filaTablaEstados fila;
								fila.job = masterJobActual->nroJob;
								fila.master = masterJobActual->nroMaster;
								fila.nodo = nodoReduccGlobal;
								fila.bloque = 0;
								fila.etapa = REDUCC_GLOBAL;
								char* temporalRedGlobal = string_from_format("m%dj%de%d", fila.master, fila.job, fila.etapa);
								strcpy(fila.temporal, temporalRedGlobal);
								fila.estado = EN_PROCESO;
								fila.siguiente = NULL;
								if (!agregarElemTablaEstados(fila)) {
									log_error(logYAMA, "Ocurrió un error al agregar un elemento la tabla de estados en fin de Reducción Local OK");
									perror("Error al agregar elementos a la tabla de estados");
								}
								//iniciar la reducción global
								int cantNodosReduccGlobal = getCantFilasByJMEtEs(masterJobActual->nroJob, masterJobActual->nroMaster, REDUCC_LOCAL, FIN_OK);
								struct filaTablaEstados filasReduccGlobal[cantNodosReduccGlobal];

								struct filaTablaEstados filaBusqueda;
								filaBusqueda.job = masterJobActual->nroJob;
								filaBusqueda.master = masterJobActual->nroMaster;
								filaBusqueda.nodo = 0;
								filaBusqueda.bloque = 0;
								filaBusqueda.etapa = REDUCC_LOCAL;
								strcpy(filaBusqueda.temporal, "");
								filaBusqueda.estado = FIN_OK;
								filaBusqueda.siguiente = NULL;
								int cantFilasEncontradas = buscarMuchosElemTablaEstados(filasReduccGlobal, filaBusqueda);
//								printf("cantFilasEncontradas: %d\n", cantFilasEncontradas);
//								printf("job %d\n", filasReduccGlobal[0].job);
//								printf("master %d\n", filasReduccGlobal[0].master);
//								printf("nodo %d\n", filasReduccGlobal[0].nodo);
//								printf("bloque %d\n", filasReduccGlobal[0].bloque);
//								printf("etapa %d\n", filasReduccGlobal[0].etapa);
//								printf("temporal %s\n", filasReduccGlobal[0].temporal);
//								printf("estado %d\n", filasReduccGlobal[0].estado);
								if (cantFilasEncontradas == 0) {
									puts("no se encontró ninguna fila de la tabla de estados para hacer la reducción global");
								}
								if (cantFilasEncontradas != cantNodosReduccGlobal) {
									puts("hubo un error en la cantidad de filas encontradas");
								} else {
									//hacer que el nodo donde se hace la reducción global vaya primero
									//buscar el nodo asignado en el vector filasReduccGlobal e intercambiarlo por el primero
									//buscar la fila por nodo, cuando la encuentra la guarda en un temporal
									//pone la fila 0  en la fila i donde estaba la del nodo buscado
									//pone la del temporal en la fila 0
									struct filaTablaEstados temporal;
//									if (cantNodosReduccGlobal > 1) {
									for (k = 0; k < cantNodosReduccGlobal;
											k++) {
//										printf("k %d - nodoReduccGlobal %d - nodo fila encontrada %d - temporal %s\n", k, nodoReduccGlobal, filasReduccGlobal[k].nodo, filasReduccGlobal[k].temporal);
										if (filasReduccGlobal[k].nodo == nodoReduccGlobal) {
											temporal = filasReduccGlobal[k];
											filasReduccGlobal[k] = filasReduccGlobal[0];
											filasReduccGlobal[0] = temporal;
											break;
										}
									}
//									}
//									for (k = 0; k < cantNodosReduccGlobal;
//											k++) {
//										printf("k %d - nodoReduccGlobal %d - nodo fila encontrada %d - temporal %s\n", k, nodoReduccGlobal, filasReduccGlobal[k].nodo, filasReduccGlobal[k].temporal);
//									}

									/* ******* envío de la tabla para reducción global ****** */
									char *mensajeSerializadoRedGlobal = serializarMensajeReduccGlobal(cantNodosReduccGlobal, filasReduccGlobal, temporalRedGlobal);
//									printf("\nmensaje serializado para reducción global: %s\n", mensajeSerializadoRedGlobal);
									enviarMensaje(socketConectado, mensajeSerializadoRedGlobal);
									aumentarCargaGlobalNodo(nodoReduccGlobal, 1);
									for (k = 0; k < cantNodosArchivo; k++) {
										if (masterJobActual->nodosUsados[k].numero == nodoReduccGlobal) {
											masterJobActual->nodosUsados[k].cantidadVecesUsados++;
										}
									}
									log_info(logYAMA, "Se da inicio a la Reducción Global en el nodo %d", nodoReduccGlobal);
									puts("\nlista de elementos luego de enviar la tabla de reducción global");
									mostrarTablaEstados();
								}
							} else {
								puts("Esperando a que todos los nodos terminen la Reducción Local");
							}
							break;

						case TIPO_MSJ_REDUCC_LOCAL_ERROR:
							;
							nroNodoRecibido = atoi(arrayMensajes[0]);
							log_info(logYAMA, "Se recibió mensaje de fin de Reducción Local ERROR, nodo %d.", nroNodoRecibido);
							free(arrayMensajes);
							/*disminuirCargaGlobalNodo(nroNodoRecibido, 1);
							 for (k = 0; k < cantNodosArchivo; k++) {
							 if (masterJobActual->nodosUsados[k].numero == nroNodoRecibido) {
							 masterJobActual->nodosUsados[k].cantidadVecesUsados--;
							 }
							 }*/
							if (modificarEstadoFilasTablaEstados(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoRecibido, 0, REDUCC_LOCAL, EN_PROCESO, ERROR) == 0) {
								log_error(logYAMA, "Ocurrió un error al modificar la tabla de estados en el fin de la Reducción Local ERROR");
								puts("No se pudo modificar ninguna fila de la tabla de estados");
							} else {
								log_info(logYAMA, "Se modificó la tabla de estados en el fin de la Reducción Local ERROR");
							}
							//abortar el job
							enviarHeaderSolo(socketConectado, TIPO_MSJ_ABORTAR_JOB);
							//disminuir la carga de los nodos de ese job
							liberarCargaJob(socketConectado, nroNodoRecibido);
//							puts("\ncarga de cada nodo luego de un abort\n-----------------------------------------\n");
//							for (i = 0; i < cantNodosArchivo; i++) {
//								printf("carga después de restar, nodo %d - %d: %d\n", masterJobActual->nodosUsados[i].numero, getDatosGlobalesNodo(masterJobActual->nodosUsados[i].numero)->numero, getDatosGlobalesNodo(masterJobActual->nodosUsados[i].numero)->carga);
//							}
							eliminarElemDatosMasterJobByFD(socketConectado);
							cerrarCliente(socketConectado);
							FD_CLR(socketConectado, &socketsLecturaMaster); // remove from master set
							log_info(logYAMA, "Se aborta el Job %d del master %d conectado por FD %d", masterJobActual->nroJob, masterJobActual->nroMaster, socketConectado);
							mostrarTablaEstados();
							break;

						case TIPO_MSJ_REDUCC_GLOBAL_OK:
							;
							free(arrayMensajes);
							//obtengo el nodo donde se hizo la reducción global
//							int nroNodoReduccGlobal = getNodoReduccGlobal(masterJobActual->nroJob, masterJobActual->nroMaster, REDUCC_GLOBAL, EN_PROCESO);
							nroNodoReduccGlobal = (int) getDatosMasterJobByFD(socketConectado)->nodoReduccGlobal;

							log_info(logYAMA, "Se recibió mensaje de fin de Reducción Global OK, nodo %d.", nroNodoReduccGlobal);
							//disminuirCargaGlobalNodo(nroNodoReduccGlobal, 1);

							//modifica el estado de la fila
							if (modificarEstadoFilasTablaEstados(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoReduccGlobal, 0, REDUCC_GLOBAL, EN_PROCESO, FIN_OK) == 0) {
								log_error(logYAMA, "Ocurrió un error al modificar la tabla de estados en el fin de la Reducción Global OK");
								puts("No se pudo modificar ninguna fila de la tabla de estados");
							} else {
								log_info(logYAMA, "Se modificó la tabla de estados en el fin de la Reducción Global OK");
							}

							/* ************** agregado de la fila de almacenamiento final en tabla de estados *************** */
							struct filaTablaEstados fila;
							fila.job = masterJobActual->nroJob;
							fila.master = masterJobActual->nroMaster;
							fila.nodo = nroNodoReduccGlobal;
							fila.bloque = 0;
							fila.etapa = ALMAC_FINAL;
//							char* temporalAlmFinal = string_from_format("m%dj%de%d", fila.master, fila.job, fila.etapa);
							strcpy(fila.temporal, "");
							fila.estado = EN_PROCESO;
							fila.siguiente = NULL;

							if (!agregarElemTablaEstados(fila)) {
								log_error(logYAMA, "Ocurrió un error al agregar un elemento la tabla de estados en fin de Reducción Global OK");
								perror("Error al agregar elementos a la tabla de estados");
							}
							/* ******* envío de la tabla para reducción global ****** */
							char **temporales = malloc(sizeof(char*) * 1);
							getAllTemporalesByJMNEtEs(temporales, masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoReduccGlobal, REDUCC_GLOBAL, FIN_OK);
							char *mensajeSerializadoAlmFinal = serializarMensajeAlmFinal(nroNodoReduccGlobal, temporales[0]);
//							printf("\nmensaje serializado para almacenamiento final: %s\n", mensajeSerializadoAlmFinal);
							enviarMensaje(socketConectado, mensajeSerializadoAlmFinal);
							log_info(logYAMA, "Se da inicio al Almacenamiento Final en el nodo %d", nroNodoReduccGlobal);
							mostrarTablaEstados();
							break;

						case TIPO_MSJ_REDUCC_GLOBAL_ERROR:
							;
							free(arrayMensajes);
//							nroNodoReduccGlobal = getNodoReduccGlobal(masterJobActual->nroJob, masterJobActual->nroMaster, REDUCC_GLOBAL, EN_PROCESO);
							nroNodoReduccGlobal = (int) getDatosMasterJobByFD(socketConectado)->nodoReduccGlobal;

							log_info(logYAMA, "Se recibió mensaje de fin de Reducción Global ERROR, nodo %d.", nroNodoReduccGlobal);
							disminuirCargaGlobalNodo(nroNodoReduccGlobal, 1);

							if (modificarEstadoFilasTablaEstados(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoReduccGlobal, 0, REDUCC_GLOBAL, EN_PROCESO, ERROR) == 0) {
								log_error(logYAMA, "Ocurrió un error al modificar la tabla de estados en el fin de la Reducción Global ERROR");
								puts("No se pudo modificar ninguna fila de la tabla de estados");
							} else {
								log_info(logYAMA, "Se modificó la tabla de estados en el fin de la Reducción Global ERROR");
							}
							//abortar el job
							enviarHeaderSolo(socketConectado, TIPO_MSJ_ABORTAR_JOB);
							liberarCargaJob(socketConectado, nroNodoRecibido);
							eliminarElemDatosMasterJobByFD(socketConectado);
							cerrarCliente(socketConectado);
							FD_CLR(socketConectado, &socketsLecturaMaster); // remove from master set
							log_trace(logYAMA, "Se aborta el Job %d del master %d conectado por FD %d", masterJobActual->nroJob, masterJobActual->nroMaster, socketConectado);
							mostrarTablaEstados();
							break;

						case TIPO_MSJ_ALM_FINAL_OK:
							;
							free(arrayMensajes);
//							nroNodoAlmacFinal = getNodoReduccGlobal(masterJobActual->nroJob, masterJobActual->nroMaster, ALMAC_FINAL, EN_PROCESO);
							nroNodoAlmacFinal = (int) getDatosMasterJobByFD(socketConectado)->nodoReduccGlobal;

							log_info(logYAMA, "Se recibió mensaje de fin de Almacenamiento Final OK, nodo %d.", nroNodoAlmacFinal);
							disminuirCargaGlobalNodo(nroNodoAlmacFinal, 1);

							//modifica el estado de la fila
							if (modificarEstadoFilasTablaEstados(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoAlmacFinal, 0, ALMAC_FINAL, EN_PROCESO, FIN_OK) == 0) {
								log_error(logYAMA, "Ocurrió un error al modificar la tabla de estados en el fin del Almacenamiento Final OK");
								puts("No se pudo modificar ninguna fila de la tabla de estados");
							} else {
								log_info(logYAMA, "Se modificó la tabla de estados en el fin del Almacenamiento Final OK");
							}
							enviarHeaderSolo(socketConectado, TIPO_MSJ_FINALIZAR_JOB);
							liberarCargaJob(socketConectado, nroNodoRecibido);
							eliminarElemDatosMasterJobByFD(socketConectado);
							cerrarCliente(socketConectado); // bye!
							FD_CLR(socketConectado, &socketsLecturaMaster); // remove from master set
							log_trace(logYAMA, "Se da por finalizado correctamente el Job %d del master %d conectado por FD %d", masterJobActual->nroJob, masterJobActual->nroMaster, socketConectado);
							mostrarTablaEstados();
							break;

						case TIPO_MSJ_ALM_FINAL_ERROR:
							;
							free(arrayMensajes);
//							nroNodoAlmacFinal = getNodoReduccGlobal(masterJobActual->nroJob, masterJobActual->nroMaster, ALMAC_FINAL, EN_PROCESO);
							nroNodoAlmacFinal = (int) getDatosMasterJobByFD(socketConectado)->nodoReduccGlobal;

							log_info(logYAMA, "Se recibió mensaje de fin de Almacenamiento Final ERROR, nodo %d.", nroNodoAlmacFinal);
							disminuirCargaGlobalNodo(nroNodoAlmacFinal, 1);

							//modifica el estado de la fila
							if (modificarEstadoFilasTablaEstados(masterJobActual->nroJob, masterJobActual->nroMaster, nroNodoAlmacFinal, 0, ALMAC_FINAL, EN_PROCESO, ERROR) == 0) {
								log_error(logYAMA, "Ocurrió un error al modificar la tabla de estados en el fin del Almacenamiento Final ERROR");
								puts("No se pudo modificar ninguna fila de la tabla de estados");
							} else {
								log_info(logYAMA, "Se modificó la tabla de estados en el fin del Almacenamiento Final ERROR");
							}

							//abortar el job
							enviarHeaderSolo(socketConectado, TIPO_MSJ_ABORTAR_JOB);
							liberarCargaJob(socketConectado, nroNodoRecibido);
							eliminarElemDatosMasterJobByFD(socketConectado);
							cerrarCliente(socketConectado);
							FD_CLR(socketConectado, &socketsLecturaMaster); // remove from master set
							log_trace(logYAMA, "Se aborta el Job %d del master %d conectado por FD %d", masterJobActual->nroJob, masterJobActual->nroMaster, socketConectado);
							mostrarTablaEstados();
							break;
						default:
							;
							free(arrayMensajes);
							break;
						}
						for (i = 0; i < masterJobActual->cantNodosUsados; i++) {
							printf("\nCarga del nodo %d al terminar de atender un pedido: %d\n", masterJobActual->nodosUsados[i].numero, getCargaGlobalNodo(masterJobActual->nodosUsados[i].numero));
						}
					}
					// END handle data from client
				} //if (FD_ISSET(i, &socketsLecturaTemp)) END got new incoming connection
			}

		}

	}
//	if (errno == EINTR) {
//		puts("interrupted by SIGINT");
//	} else {
//		perror("pselect()");
//	}
//sigprocmask()

// END for(;;)

//sigprocmask(SIG_SETMASK, &old_set, NULL);

//cerrarServer(listenningSocket);
//cerrarServer(socketCliente);
	log_info(logYAMA, "Server cerrado");
	log_destroy(logYAMA);
	limpiarMemoria();
	return EXIT_SUCCESS;
}
