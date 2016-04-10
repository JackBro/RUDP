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
#include <RUDP/list.h>
#include <RUDP/map.h>
#include <RUDP/peer.h>
#include <limits.h>
#include <mutex>

namespace RUDP
{
    class Socket
    {
    private:
        RUDP::Map<RUDP::Peer> m_peerList;
        RUDP::List<RUDP::Packet> m_ackQueue;
        RUDP::List<RUDP::Packet> m_outQueue;
        RUDP::List<RUDP::Packet> m_inQueue;
        std::mutex m_inQueueLock;
        std::mutex m_outQueueLock;
        std::mutex m_ackQueueLock;
        
        sockaddr_storage m_address;
        uint64_t m_ackTimeout;
        RUDP::SocketHandle m_handle;
        uint16_t m_port;
        
        bool acknowledge();
        bool listen(uint32_t attempts);
        bool flush();
        
        void setAckTimeout(uint64_t ms);
        static void PrintLastSocketError(const char *context);
        
        bool receivePacket(RUDP::Packet *pck);
        bool sendPacket(RUDP::Packet *pck);
        
    public:
        Socket();
        ~Socket();
        
        bool open(uint16_t port, uint32_t addr = 0);
        bool open(sockaddr *target, socklen_t targetSize);
        bool open(sockaddr_in *target);
        bool open(sockaddr_in6 *target);
        
        uint16_t getPort();
        RUDP::SocketHandle getHandle();
        sockaddr_storage *getAddress();
        
        void updatePeers();
        RUDP::Peer *getPeer(uint32_t ipv4, uint16_t port);
        RUDP::Peer *getPeer(sockaddr_storage *addr);
        
        void enqueueOutgoingPackets(RUDP::List<RUDP::Packet> *packets);
        void enqueueAcknowledgments(RUDP::List<RUDP::Packet> *packets);
        
        uint64_t update(uint64_t msTimeout);
    };
}

#endif
