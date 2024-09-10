#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "amcom.h"

/// Start of packet character
const uint8_t  AMCOM_SOP         = 0xA1;
const uint16_t AMCOM_INITIAL_CRC = 0xFFFF;

static uint16_t AMCOM_UpdateCRC(uint8_t byte, uint16_t crc)
{
	byte ^= (uint8_t)(crc & 0x00ff);
	byte ^= (uint8_t)(byte << 4);
	return ((((uint16_t)byte << 8) | (uint8_t)(crc >> 8)) ^ (uint8_t)(byte >> 4) ^ ((uint16_t)byte << 3));
}


void AMCOM_InitReceiver(AMCOM_Receiver* receiver, AMCOM_PacketHandler packetHandlerCallback, void* userContext) {
	receiver->packetHandler = packetHandlerCallback;
	receiver->userContext   = userContext;
}

size_t AMCOM_Serialize(uint8_t packetType, const void* payload, size_t payloadSize, uint8_t* destinationBuffer) {
	((AMCOM_Packet*)destinationBuffer)->header.sop = 0xA1;
	((AMCOM_Packet*)destinationBuffer)->header.type = packetType;
	((AMCOM_Packet*)destinationBuffer)->header.length = payloadSize;

	uint16_t crc = 0xFFFF;

	crc = AMCOM_UpdateCRC(packetType, crc);
	crc = AMCOM_UpdateCRC(payloadSize, crc);
	
	for(size_t idx = 0; idx < payloadSize; idx++) {
		crc = AMCOM_UpdateCRC( ((uint8_t*)payload)[idx], crc);
		((AMCOM_Packet*)destinationBuffer)->payload[idx] = ((uint8_t*)payload)[idx];
	}
	((AMCOM_Packet*)destinationBuffer)->header.crc = crc;

	return 5 + payloadSize;
}

static AMCOM_Packet recv_packet;

uint16_t AMCOM_calculate_CRC(AMCOM_Packet* packet) {
	uint16_t crc = 0xFFFF; 

	crc = AMCOM_UpdateCRC(packet->header.type, crc);
	crc = AMCOM_UpdateCRC(packet->header.length, crc);

	for(size_t idx = 0; idx < packet->header.length; idx++) {
		crc = AMCOM_UpdateCRC(packet->payload[idx], crc);
	}

	return crc;
}

void AMCOM_Deserialize(AMCOM_Receiver* receiver, const void* data, size_t dataSize) {
	// printf("AMCOM_Deserialize(receiver = %p, data = %p, dataSize = %d)", receiver, data, dataSize);
	// (receiver->packetHandler)(((AMCOM_Packet*)data), receiver->userContext);
	// return;
	// receiver->receivedPacketState = AMCOM_PacketState.
	uint8_t* udata = (uint8_t*)data; // so that i don't have to cast constantly

	uint16_t crc = 0x0000;
	size_t idx = 0;

	while(idx < dataSize) {
		// printf("\npayloadCounter = %d\n", receiver->payloadCounter);
		// printf("idx = %d\n", idx);
		// printf("data[%d] = %02x\n", idx, udata[idx]);
		// printf("State = %d\n", receiver->receivedPacketState);
		switch(receiver->receivedPacketState) {
			case AMCOM_PACKET_STATE_GOT_WHOLE_PACKET:
			case AMCOM_PACKET_STATE_EMPTY:
				receiver->receivedPacket.header.sop = udata[idx];
				receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_SOP;
			break;
			case AMCOM_PACKET_STATE_GOT_SOP:
				receiver->receivedPacket.header.type = udata[idx];
				receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_TYPE;
			break;
			case AMCOM_PACKET_STATE_GOT_TYPE:
			 	if(udata[idx] >= 0U && udata[idx] <= 200U) {
					receiver->receivedPacket.header.length = udata[idx];
					receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_LENGTH;
					// printf("udata[idx] = %d\n", udata[idx]);
				} else {
					receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
				}
			break;
			case AMCOM_PACKET_STATE_GOT_LENGTH:
				crc = (udata[idx] );
				receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_CRC_LO;
			break;
			case AMCOM_PACKET_STATE_GOT_CRC_LO:
				crc |= udata[idx] << 8;
				receiver->receivedPacket.header.crc = crc;
				receiver->receivedPacketState = AMCOM_PACKET_STATE_GETTING_PAYLOAD;
				if(receiver->receivedPacket.header.length == 0) {
					// (receiver->packetHandler)(&receiver->receivedPacket, receiver->userContext);
					receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;
				}
			break;
			case AMCOM_PACKET_STATE_GETTING_PAYLOAD:
			 	if(receiver->payloadCounter < receiver->receivedPacket.header.length - 1
			    	&& receiver->payloadCounter < 200-1) {
					receiver->receivedPacket.payload[receiver->payloadCounter++] = udata[idx];
					// printf("jeden\n");
				} else if(receiver->payloadCounter == receiver->receivedPacket.header.length - 1
					|| receiver->payloadCounter == 200 - 1){
					receiver->receivedPacket.payload[receiver->payloadCounter++] = udata[idx];
					receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;
					// printf("dwa\n");
				}
				
			break;
		}
		idx++;

		if(receiver->receivedPacketState == AMCOM_PACKET_STATE_GOT_WHOLE_PACKET) {
			uint16_t crc = AMCOM_calculate_CRC(&receiver->receivedPacket);
			// printf("header.crc: 0x%04x, calc_crc: 0x%04x\n", receiver->receivedPacket.header.crc, crc);
			if(crc == receiver->receivedPacket.header.crc) {
				(receiver->packetHandler)(&receiver->receivedPacket, receiver->userContext);
			} 
			receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
			receiver->payloadCounter = 0;
		}
	}

}
