

#include "constantes.h"



/* ****************************** funciones para enviar mensajes ******************************/
int enviarHeader(int serverSocket,struct headerProtocolo header){
	int cantBytesEnviados;
	char *idString=intToArrayZerosLeft(header.id,LARGO_STRING_HEADER_ID);
	char *tamPayloadString=intToArrayZerosLeft(header.tamPayload,LARGO_STRING_HEADER_TAM_PAYLOAD);
	cantBytesEnviados = send(serverSocket,idString, string_length(idString)+1, 0);
	//printf("String Length del id del header: %d\n",string_length(idString));
	//printf("Bytes enviados del id del header: %d\n",cantBytesEnviados);
	if(cantBytesEnviados!=( string_length(idString)+1)){
		puts("Error. No se enviaron todos los bytes del id del header\n");
		return 0;
	}
	cantBytesEnviados = send(serverSocket,tamPayloadString, string_length(tamPayloadString)+1, 0);
	//printf("String Length del tamaño del header: %d\n",string_length(tamPayloadString));
	//printf("Bytes enviados del tamaño del header: %d\n",cantBytesEnviados);
	if(cantBytesEnviados!=(string_length(tamPayloadString) + 1)){
		puts("Error. No se enviaron todos los bytes del tamaño del header\n");
		return 0;
	}
	return 1;
}

//el send no manda siempre todos los bytes que le pongo por el protocolo IP
//leer la cantidad de bytes enviados que es lo que devuelve
int enviarMensaje(int serverSocket,char *message){
	printf("bytes de largo: %d\n",string_length(message));
	int cantBytesEnviados = send(serverSocket, message, string_length(message), 0);
	//printf("String Length del mensaje: %d\n",string_length(message));
	printf("Bytes enviados del mensaje: %d\n",cantBytesEnviados);
	if(cantBytesEnviados!=string_length(message)){
		puts("Error. No se enviaron todos los bytes del mensaje\n");
		return 0;
	}
	return 1;
}


/* ******************************** funciones para recibir mensajes ********************************/
struct headerProtocolo recibirHeader(int socketCliente){
	int idEntero,tamEntero,packageSizeId=(LARGO_STRING_HEADER_ID+1),packageSizeTam=(LARGO_STRING_HEADER_TAM_PAYLOAD+1);		// 4+1 hardcodeado a revisar
	char id[packageSizeId],tamPayload[packageSizeTam];
	if(recv(socketCliente,(void*) id, packageSizeId, 0)<0){
		perror("Recepción Id Header");
		struct headerProtocolo header=armarHeader(-1,tamEntero);
		return header;
	}
	if(recv(socketCliente,(void*) tamPayload, packageSizeTam, 0)<0){
		perror("Recepción Tamaño Payload Header");
		struct headerProtocolo header=armarHeader(-1,tamEntero);
		return header;
	}
	//printf("recepción en char *: %s - %s\n",id,tamPayload);
	sscanf(id, "%d", &idEntero);
	sscanf(tamPayload, "%d", &tamEntero);
	struct headerProtocolo header=armarHeader(idEntero,tamEntero);
	return header;
}

/*
 * recibe por socket un mensaje
 * parámetros: socket del cliente y el largo del string (string_length)
 * devuelve un mensaje como char*
 */
char* recibirMensaje(int socketCliente,int packageSize){
	char *message=malloc(packageSize+1);
	if(recv(socketCliente,(void*) message,packageSize+1, 0)<0){
		perror("Recepción Mensaje");
		return (char*) -1;
	}
	return message;
}
