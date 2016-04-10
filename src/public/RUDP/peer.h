//
//  peer.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/25.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_peer_h
#define RUDP_peer_h

#include <RUDP/list.h>
#include <RUDP/util.h>
#include <RUDP/platform.h>
#include <RUDP/packet.h>
#include <RUDP/map.h>
#include <RUDP/channel.h>
#include <vector>
#include <atomic>

namespace RUDP
{
    enum EnqueueMessageOption : uint8_t
    {
        EnqueueMessageOption_None = 0,
        EnqueueMessageOption_ConfirmDelivery = 1,
        EnqueueMessageOption_InOrder = 1 << 2
    };
    
    enum EnqueueMessageResult
    {
        EnqueueMessageResult_Success = 1,
        EnqueueMessageResult_OutQueueFull = 0
    };
    
    class Peer;
    
    struct PeerMessage
    {
        char *m_data;
        size_t m_dataLen;
        RUDP::Peer *m_peer;
        RUDP::ChannelId m_channel;
        
        void prepareForSending(char *dataToSend, size_t dataLen, RUDP::Peer *target, RUDP::ChannelId channel);
        void prepareForReceiving(char *messageBuffer, size_t bufferLen);
    };
    
    class Socket;
    
    class Peer
    {
        friend class Socket;
        
    private:
        sockaddr_storage m_addr;
        uint32_t m_hash;
        std::vector<RUDP::Channel> m_inQueueChannels;
        RUDP::List<RUDP::Channel*> m_inQueue;
        RUDP::List<RUDP::Packet> m_outQueue;
        RUDP::List<RUDP::Packet> m_ackQueue;
        
        RUDP::Socket *m_socket;
        std::atomic<RUDP::PacketId> *m_ChannelPacketIds;
        
        bool sendPacket(RUDP::Packet *toWrite);
        RUDP::PacketId reservePacketsOnChannel(RUDP::ChannelId channel, RUDP::PacketId numNeeded);
        
        bool enqueueIncomingPacket(RUDP::Packet *pck);
        
    public:
        Peer();
        Peer(RUDP::Socket *socket, sockaddr_storage *addr);
        ~Peer();
        
        sockaddr_storage *getAddress();
        
        void enqueueAcknowledgement(RUDP::Packet *ack);
        RUDP::EnqueueMessageResult enqueueMessage(RUDP::PeerMessage *message, RUDP::EnqueueMessageOption options);
        bool peekMessage(size_t &msgSize);
        bool receiveMessage(RUDP::PeerMessage *message);
        
        void flushToSocket();
        
        uint32_t hash();
        bool equals(Peer *peer);
    };
}

#endif