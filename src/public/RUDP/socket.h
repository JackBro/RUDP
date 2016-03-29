//
//  socket.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/25.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_socket_h
#define RUDP_socket_h

#include <stdint.h>
#include <RUDP/packet.h>
#include <RUDP/platform.h>
#include <RUDP/queue.h>
#include <limits.h>
#include <atomic>
#include <vector>

namespace RUDP
{
    enum EnqueueMessageOption : uint8_t
    {
        EnqueueMessageOption_None            = 0,
        EnqueueMessageOption_ConfirmDelivery = 1,
        EnqueueMessageOption_InOrder         = 1 << 2
    };
    
    enum EnqueueMessageResult
    {
        EnqueueMessageResult_Success            = 1,
        EnqueueMessageResult_OutQueueFull	    = 0,
		EnqueueMEssageResult_
    };
    
    enum AckStatus : uint8_t
    {
        AckStatus_Waiting   = 0,
        AckStatus_Received  = 1
    };
    
    /*
     when an ack is needed, the packet is stored in a fixed-size buffer associated with the socket
     when an ack is received, it looks up the original packet, compares the header, and if they match, throws the packet away
     when an ack is not received after a certain amount of time, the original message is tried again
     if the fixed-size buffer becomes full, we assume that the receiver is not there and kill the socket
     */
    
    struct Message
    {
        const char *m_data;
        size_t m_dataLen;
        sockaddr_storage m_peer;
        RUDP::ChannelId m_channel;
        
		void prepareForSending(const char *dataToSend, size_t dataLen, uint32_t targetAddr, uint16_t targetPort, RUDP::ChannelId channel);
        void prepareForSending(const char *dataToSend, size_t dataLen, sockaddr_storage *target, RUDP::ChannelId channel);
        void prepareForReceiving(const char *messageBuffer, size_t bufferLen);
    };
    
    class Socket
    {
    private:
        std::atomic<RUDP::PacketId> *m_ChannelPacketIds;

		RUDP::NodeStore<RUDP::Packet> m_nodeStore;
        RUDP::Queue<RUDP::Packet> m_outQueue;
		RUDP::Queue<RUDP::Packet> m_ackQueue;

		std::vector<RUDP::Queue<RUDP::Packet>> m_inQueueChannels;
		RUDP::Queue<RUDP::Queue<RUDP::Packet>> m_inQueue;

        RUDP::SocketHandle m_handle;
        uint16_t m_port;
		uint32_t m_ackTimeout;

        RUDP::PacketId reservePacketsOnChannel(RUDP::ChannelId channel, RUDP::PacketId numNeeded);
		bool receivePacket(RUDP::Packet *userBuffer);
		void enqueuePacket(RUDP::Packet *pck);
        bool sendPacket(RUDP::Packet *toWrite);

		void enqueueAcknowledgement(RUDP::Packet *ack);

		bool acknowledge();
		bool listen(uint32_t attempts);
		bool flush();

		void setAckTimeout(uint32_t ms);

		static void PrintLastSocketError(const char *context);
        
    public:
		Socket();
        ~Socket();
        
		bool open(uint16_t port, uint32_t addr = 0);
        bool open(sockaddr *target, socklen_t targetSize);
		bool open(sockaddr_in *target);
		bool open(sockaddr_in6 *target);
        
        uint16_t getPort();
        RUDP::SocketHandle getHandle();
        
		uint32_t update(uint32_t msTimeout);
		
        
        bool peekMessage(size_t &msgSize);
        bool receiveMessage(RUDP::Message *message);
        RUDP::EnqueueMessageResult enqueueMessage(RUDP::Message *message, RUDP::EnqueueMessageOption options);
    };
}

#endif
