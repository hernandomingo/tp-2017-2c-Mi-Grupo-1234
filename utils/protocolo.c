/*
 * constantes.c
 *
 *  Created on: 17/10/2017
 *      Author: utnso
 */

// Tipos de mensajes para el protocolo de comunicación
enum tiposMensajes {
	EN_DESUSO,
	TIPO_MSJ_TABLA_TRANSFORMACION,
	TIPO_MSJ_TABLA_REDUCCION_LOCAL,
	TIPO_MSJ_TABLA_REDUCCION_GLOBAL,
	TIPO_MSJ_TABLA_ALMACENAMIENTO_FINAL,
	TIPO_MSJ_ERROR,
	TIPO_MSJ_OK,
	TIPO_MSJ_HANDSHAKE,
	TIPO_MSJ_HANDSHAKE_RESPUESTA_OK,
	TIPO_MSJ_HANDSHAKE_RESPUESTA_DENEGADO,
	TIPO_MSJ_PATH_ARCHIVO,
	TIPO_MSJ_PATH_ARCHIVO_TRANSFORMAR,
	TIPO_MSJ_PEDIDO_METADATA_ARCHIVO,
	TIPO_MSJ_METADATA_ARCHIVO,
	TIPO_MSJ_DATOS_CONEXION_NODOS,
	TIPO_MSJ_TRANSFORMACION_OK,
	TIPO_MSJ_TRANSFORMACION_ERROR,
	TIPO_MSJ_REDUCC_LOCAL_OK,
	TIPO_MSJ_REDUCC_LOCAL_ERROR,
	TIPO_MSJ_REDUCC_GLOBAL_OK,
	TIPO_MSJ_REDUCC_GLOBAL_ERROR,
	TIPO_MSJ_ALM_FINAL_OK,
	TIPO_MSJ_ALM_FINAL_ERROR,
	TIPO_MSJ_FINALIZAR_JOB,
	TIPO_MSJ_ABORTAR_JOB,
	TIPO_MSJ_DATA_TRANSFORMACION_WORKER,
	TIPO_MSJ_DATANODE
};

// OJO! Si se agrega algo en el enum de arriba actualizar dejar siempre acá el último valor del array
const int protocoloCantidadMensajes[TIPO_MSJ_DATANODE + 1] = {
		0,
		1,
		1,
		1,
		1,
		0,
		0,
		1,
		0,
		0,
		1,
		1,
		1,
		1,
		1,
		2,
		2,
		1,
		1,
		1,
		1,
		0,
		0,
		0,
		0,
		4,
		4};

const char* protocoloMensajesPredefinidos[TIPO_MSJ_DATANODE + 1] = {
		"EN_DESUSO",
		"TABLA_TRANSFORMACION",
		"TABLA_REDUCCION_LOCAL",
		"TABLA_REDUCCION_GLOBAL",
		"TABLA_ALMACENAMIENTO_FINAL",
		"ERROR",
		"OK",
		"HANDSHAKE",
		"HANDSHAKE_RESPUESTA_OK",
		"HANDSHAKE_RESPUESTA_DENEGADO",
		"PATH_ARCHIVO",
		"PATH_ARCHIVO_TRANSFORMAR",
		"PEDIDO_METADATA_ARCHIVO",
		"METADATA_ARCHIVO",
		"DATOS_CONEXION_NODOS",
		"TIPO_MSJ_TRANSFORMACION_OK",
		"TIPO_MSJ_TRANSFORMACION_ERROR",
		"TIPO_MSJ_REDUCC_LOCAL_OK",
		"TIPO_MSJ_REDUCC_LOCAL_ERROR",
		"TIPO_MSJ_REDUCC_GLOBAL_OK",
		"TIPO_MSJ_REDUCC_GLOBAL_ERROR",
		"TIPO_MSJ_ALM_FINAL_OK",
		"TIPO_MSJ_ALM_FINAL_ERROR",
		"TIPO_MSJ_FINALIZAR_JOB",
		"TIPO_MSJ_ABORTAR_JOB",
		"TIPO_MSJ_DATA_TRANSFORMACION_WORKER",
		"TIPO_MSJ_DATANODE"};
