#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "rtp.h"

/* GIVEN Function:
 * Handles creating the client's socket and determining the correct
 * information to communicate to the remote server
 */
CONN_INFO* setup_socket(char* ip, char* port){
	struct addrinfo *connections, *conn = NULL;
	struct addrinfo info;
	memset(&info, 0, sizeof(struct addrinfo));
	int sock = 0;

	info.ai_family = AF_INET;
	info.ai_socktype = SOCK_DGRAM;
	info.ai_protocol = IPPROTO_UDP;
	getaddrinfo(ip, port, &info, &connections);

	/*for loop to determine corr addr info*/
	for(conn = connections; conn != NULL; conn = conn->ai_next){
		sock = socket(conn->ai_family, conn->ai_socktype, conn->ai_protocol);
		if(sock <0){
			if(DEBUG)
				perror("Failed to create socket\n");
			continue;
		}
		if(DEBUG)
			printf("Created a socket to use.\n");
		break;
	}
	if(conn == NULL){
		perror("Failed to find and bind a socket\n");
		return NULL;
	}
	CONN_INFO* conn_info = malloc(sizeof(CONN_INFO));
	conn_info->socket = sock;
	conn_info->remote_addr = conn->ai_addr;
	conn_info->addrlen = conn->ai_addrlen;
	return conn_info;
}

void shutdown_socket(CONN_INFO *connection){
	if(connection)
		close(connection->socket);
}

/* 
 * ===========================================================================
 *
 *			STUDENT CODE STARTS HERE. PLEASE COMPLETE ALL FIXMES
 *
 * ===========================================================================
 */


/*
 *  Returns a number computed based on the data in the buffer.
 */
static int checksum(char *buffer, int length)
{

	/*  ----  FIXME  ----
	 *
	 *  The goal is to return a number that is determined by the contents
	 *  of the buffer passed in as a parameter.  There a multitude of ways
	 *  to implement this function.  For simplicity, simply sum the ascii
	 *  values of all the characters in the buffer, and return the total.
	 */

	int sum = 0;

	for (int n = 0; n < length; ++n)
		sum += (int)buffer[n];

	return sum;
}

/*
 *  Converts the given buffer into an array of PACKETs and returns
 *  the array.  The value of (*count) should be updated so that it 
 *  contains the length of the array created.
 */
static PACKET* packetize(char *buffer, int length, int *count)
{

	/*  ----  FIXME  ----
	 *  The goal is to turn the buffer into an array of packets.
	 *  You should allocate the space for an array of packets and
	 *  return a pointer to the first element in that array.  Each 
	 *  packet's type should be set to DATA except the last, as it 
	 *  should be LAST_DATA type. The integer pointed to by 'count' 
	 *  should be updated to indicate the number of packets in the 
	 *  array.
	 */

	PACKET* packetArray = NULL;
	*count = 0;

	if (length)
	{
		int packetArrayLength = length / MAX_PAYLOAD_LENGTH;
		if (length % MAX_PAYLOAD_LENGTH)
			++packetArrayLength;

		*count = packetArrayLength;

		packetArray = (PACKET*)malloc(sizeof(PACKET) * packetArrayLength);

		int n;
		for (n = 0; n < packetArrayLength; ++n)
		{
			if (n < packetArrayLength - 1)
			{
				packetArray[n].type = DATA;
				packetArray[n].payload_length = MAX_PAYLOAD_LENGTH;
			}
			else
			{
				packetArray[n].type = LAST_DATA;
				packetArray[n].payload_length = length - (n * MAX_PAYLOAD_LENGTH);
			}
			
			for (int m = 0; m < packetArray[n].payload_length; ++m)
				packetArray[n].payload[m] = buffer[n * MAX_PAYLOAD_LENGTH + m];

			packetArray[n].checksum = checksum(packetArray[n].payload, packetArray[n].payload_length);
		}
	}

	return packetArray;
}

/*
 * Send a message via RTP using the connection information
 * given on UDP socket functions sendto() and recvfrom()
 */
int rtp_send_message(CONN_INFO *connection, MESSAGE*msg)
{
	/* ---- FIXME ----
	 * The goal of this function is to turn the message buffer
	 * into packets and then, using stop-n-wait RTP protocol,
	 * send the packets and re-send if the response is a NACK.
	 * If the response is an ACK, then you may send the next one
	 */

	PACKET* packetArray;
	int packetArrayLength;

	char recvBuffer[4 * 8 + MAX_PAYLOAD_LENGTH];
	PACKET* recvPacketPointer;

	packetArray = packetize(msg->buffer, msg->length, &packetArrayLength);

	for (int n = 0; n < packetArrayLength; ++n)
	{
		if (packetArray[n].payload_length < MAX_PAYLOAD_LENGTH)
			for (int m = packetArray[n].payload_length; m < MAX_PAYLOAD_LENGTH; ++m)
				packetArray[n].payload[m] = '\0';

		do
		{
			sendto(connection->socket, &(packetArray[n]), sizeof(PACKET), 0, connection->remote_addr, connection->addrlen);

			recvfrom(connection->socket, &recvBuffer, sizeof(char) * (4 * 8 + MAX_PAYLOAD_LENGTH), 0, connection->remote_addr, &(connection->addrlen));

			recvPacketPointer = (PACKET*)recvBuffer;
		} while (ACK != recvPacketPointer->type);
	}

	free(packetArray);

	return packetArrayLength;
}

/*
 * Receive a message via RTP using the connection information
 * given on UDP socket functions sendto() and recvfrom()
 */
MESSAGE* rtp_receive_message(CONN_INFO *connection)
{
	/* ---- FIXME ----
	 * The goal of this function is to handle 
	 * receiving a message from the remote server using
	 * recvfrom and the connection info given. You must 
	 * dynamically resize a buffer as you receive a packet
	 * and only add it to the message if the data is considered
	 * valid. The function should return the full message, so it
	 * must continue receiving packets and sending response 
	 * ACK/NACK packets until a LAST_DATA packet is successfully 
	 * received.
	 */

	char recvBuffer[4 * 8 + MAX_PAYLOAD_LENGTH];

	MESSAGE* recvMessage = (MESSAGE*)malloc(sizeof(MESSAGE));	
	recvMessage->length = 0;

	PACKET* recvPacketPointer;

	do
	{
		//ssize_t messageLength;
		PACKET acknowledgePacket;
		acknowledgePacket.type = NACK;
		acknowledgePacket.checksum = -1;
		acknowledgePacket.payload_length = 0;

		do
		{
			/*messageLength = */recvfrom(connection->socket, &recvBuffer, sizeof(char) * (4 * 8 + MAX_PAYLOAD_LENGTH), 0, connection->remote_addr, &(connection->addrlen));
			recvPacketPointer = (PACKET*)recvBuffer;

			if (checksum(recvPacketPointer->payload, recvPacketPointer->payload_length) == recvPacketPointer->checksum)
				acknowledgePacket.type = ACK;
			else
				acknowledgePacket.type = NACK;

			sendto(connection->socket, &acknowledgePacket, sizeof(PACKET), 0, connection->remote_addr, connection->addrlen);

		} while (NACK == acknowledgePacket.type);

		if (recvMessage->length)
		{
			int tempBufferLength;
			if (LAST_DATA == recvPacketPointer->type)
				tempBufferLength = recvMessage->length + recvPacketPointer->payload_length + 1;
			else
				tempBufferLength = recvMessage->length + recvPacketPointer->payload_length;

			char* tempBuffer = (char*)malloc(sizeof(char) * tempBufferLength);

			if (LAST_DATA == recvPacketPointer->type)
				tempBuffer[tempBufferLength - 1] = '\0';

			int n;
			for (n = 0; n < recvMessage->length; ++n)
				tempBuffer[n] = recvMessage->buffer[n];
			for (int m = 0, n = recvMessage->length; n < tempBufferLength; ++n, ++m)
				tempBuffer[n] = recvPacketPointer->payload[m];

			free(recvMessage->buffer);

			recvMessage->buffer = tempBuffer;
			recvMessage->length = tempBufferLength;
		}
		else
		{
			recvMessage->length = recvPacketPointer->payload_length;
			recvMessage->buffer = (char*)malloc(sizeof(char) * recvMessage->length);

			for (int n = 0; n < recvMessage->length; ++n)
				recvMessage->buffer[n] = recvPacketPointer->payload[n];
		}
	} while (LAST_DATA != recvPacketPointer->type);

	return recvMessage;
}
