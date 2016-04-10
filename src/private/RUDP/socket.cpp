//
//  socket.cpp
//  RUDP
//
//  Created by Timothy Smale on 2016/03/25.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <RUDP/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <atomic>

void RUDP::Socket::PrintLastSocketError(const char *context)
{
#ifdef _WIN32
    int error = WSAGetLastError();
    if (error != 0 && error != WSAEWOULDBLOCK)
    {
        wchar_t buff[256];
        FormatMessage(
                      FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL,
                      error,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR)&buff, RUDP_ARRAYSIZE(buff), NULL);
        
        RUDP::Print::f("Error %s (%d: %.*S)\n", context, error, RUDP_ARRAYSIZE(buff), buff);
    }
#else
    int error = errno;
    
    if (error != 0 && error != EAGAIN)
    {
        char buff[256];
        RUDP_STRERROR(error, buff, RUDP_ARRAYSIZE(buff));
        RUDP::Print::f("Error %s (%d: %.*s)\n", context, error, (int)RUDP_ARRAYSIZE(buff), buff);
    }
#endif
}

RUDP::Socket::Socket() :
m_ackTimeout(1000),
m_port(0),
m_handle(0),
m_peerList(RUDP::Map<RUDP::Peer>(256))
{
#ifdef _WIN32
    WSADATA wsaData;
    int error = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (error != 0)
    {
        PrintLastSocketError("WinSock Startup");
    }
#endif
}

RUDP::Socket::~Socket()
{
    RUDP_CLOSESOCKET(m_handle);
}

bool RUDP::Socket::open(uint16_t port, uint32_t addr)
{
    sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    target.sin_addr.s_addr = addr == 0 ? INADDR_ANY : htonl(addr);
    return open(&target);
}

bool RUDP::Socket::open(sockaddr_in *target)
{
    m_port = target->sin_port;
    return open((sockaddr*)target, sizeof(*target));
}

bool RUDP::Socket::open(sockaddr_in6 *target)
{
    m_port = target->sin6_port;
    return open((sockaddr*)target, sizeof(*target));
}

bool RUDP::Socket::open(sockaddr *target, socklen_t targetSize)
{
    m_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(m_handle <= 0)
    {
        PrintLastSocketError("Creating socket");
        return false;
    }
    
    int bindResult = bind(m_handle, (const sockaddr*)target, targetSize);
    if(bindResult < 0)
    {
        PrintLastSocketError("Binding Socket");
        return false;
    }
    
    // non-blocking
#ifdef _WIN32
    u_long iMode = 1;
    if (ioctlsocket(m_handle, FIONBIO, &iMode) != NO_ERROR)
#else
        if(fcntl(m_handle, F_SETFL, O_NONBLOCK, 1) == -1)
#endif
        {
            PrintLastSocketError("Setting Socket to Non-Blocking");
            return false;
        }
    
    memcpy(&m_address, target, targetSize > sizeof(sockaddr_storage) ? sizeof(sockaddr_storage) : targetSize);
    
    return true;
}

RUDP::SocketHandle RUDP::Socket::getHandle()
{
    return m_handle;
}

sockaddr_storage *RUDP::Socket::getAddress()
{
    return &m_address;
}

uint16_t RUDP::Socket::getPort()
{
    return m_port;
}

bool RUDP::Socket::flush()
{
    bool sent = false;
    RUDP::List<RUDP::Packet> toSend = {};
    m_outQueueLock.lock();
    toSend.inheritFrom(&m_outQueue);
    m_outQueueLock.unlock();
    
    for (RUDP::Packet *packet = toSend.peek(); packet != NULL; packet = toSend.peek())
    {
        sent = true;
        if(sendPacket(packet))
        {
            toSend.pop();
        }
        else
        {
            break;
        }
    }
    
    return sent;
}

bool RUDP::Socket::listen(uint32_t attempts)
{
    RUDP::List<RUDP::Packet> receivedPackets = {};
    RUDP::Packet packet = {};
    
    for (uint32_t i = 0; i < attempts; i++)
    {
        if(receivePacket(&packet))
        {
            receivedPackets.push(&packet);
        }
    }
    
    bool received = receivedPackets.peek() != NULL;
    
    m_inQueueLock.lock();
    m_inQueue.inheritFrom(&receivedPackets);
    m_inQueueLock.unlock();
    
    return received;
}

void RUDP::Socket::setAckTimeout(uint64_t ms)
{
    m_ackTimeout = ms;
}

bool RUDP::Socket::acknowledge()
{
    bool sentAny = false;
    
    RUDP::List<RUDP::Packet> toSend = {};
    m_ackQueueLock.lock();
    toSend.inheritFrom(&m_ackQueue);
    m_ackQueueLock.unlock();
    
    for (RUDP::Packet *pck = toSend.peek(); pck != NULL; pck = toSend.next(pck))
    {
        uint64_t time = RUDP_GETTIMEMS_LOCAL();
        uint64_t pckTime = pck->getTimestamp();
        uint64_t diff = time - pckTime;
        
        //RUDP_PRINTF("check ack: %lld %lld %lld\n", time, pckTime, diff);
        
        if (diff > m_ackTimeout)
        {
            pck->setTimestamp(time);
            sentAny = true;
            sendPacket(pck);
        }
    }
    
    return sentAny;
}

uint64_t RUDP::Socket::update(uint64_t msTimeout)
{
    uint64_t time = RUDP_GETTIMEMS_LOCAL();
    uint64_t target = msTimeout + time;
    
    do
    {
        listen(256);
        acknowledge();
        flush();
        
        time = RUDP_GETTIMEMS_LOCAL();
        //RUDP_PRINTF("check socket: %lld %lld\n", time, target);
    }
    while (target > time);
    
    return time >= target? 0 : target - time;
}

bool RUDP::Socket::sendPacket(RUDP::Packet *toWrite)
{
    sockdataptr_t data = (sockdataptr_t)toWrite->getDataPtr();
    size_t dataLen = toWrite->getTotalSize();
    sockaddr *target = (sockaddr*)toWrite->getTargetAddr();
    
    toWrite->getHeader()->m_packetId = htons(toWrite->getHeader()->m_packetId);
    
    ssize_t sentBytes = 0;
    
    switch(target->sa_family)
    {
        case AF_INET:
            sentBytes = sendto(m_handle, data, dataLen, 0, target, sizeof(sockaddr_in));
            break;
            
        case AF_INET6:
            sentBytes = sendto(m_handle, data, dataLen, 0, target, sizeof(sockaddr_in6));
            break;
            
        default:
            sentBytes = sendto(m_handle, data, dataLen, 0, target, sizeof(sockaddr_storage));
            break;
    }
    
    toWrite->getHeader()->m_packetId = ntohs(toWrite->getHeader()->m_packetId);
    
    if(sentBytes != dataLen)
    {
        PrintLastSocketError("Sending Packet");
        return false;
    }
    else
    {
        RUDP::PacketHeader *header = toWrite->getHeader();
        uint32_t size = toWrite->getUserDataSize();
        
        RUDP::ChannelId channelId = header->m_channelId;
        RUDP::PacketId packetId = header->m_packetId;
        RUDP::PacketFlag flags = header->m_flags;
        
        RUDP::Print::f("Sent packet on channel %d:%d:%d -> (%d)\n\n",
                       channelId,
                       packetId,
                       flags,
                       size
                       //toWrite->getUserDataPtr(),
                       //toWrite->getUserDataSize()
                       );
        
        return true;
    }
}

bool RUDP::Socket::receivePacket(RUDP::Packet *userBuffer)
{
    sockaddr_storage sender;
    socklen_t senderSize = sizeof(sender);
    memset(&sender, 0, senderSize);
    
    ssize_t bytesRead = recvfrom(m_handle, (char*)userBuffer->getDataPtr(), RUDP::PacketSize, 0, (sockaddr*)&sender, &senderSize);
    
    if (bytesRead == -1)
    {
        PrintLastSocketError("Receiving Packet");
    }
    else if(bytesRead > 0)
    {
        userBuffer->setWritePosition((uint16_t)(bytesRead - sizeof(RUDP::PacketHeader)));
        userBuffer->setTargetAddr(&sender);
        userBuffer->getHeader()->m_packetId = ntohs(userBuffer->getHeader()->m_packetId);
        
        RUDP::PacketHeader *header = userBuffer->getHeader();
        
        RUDP::Print::f("received packet on channel %d:%d:%d -> (%d, %d)\n\n",
                       header->m_channelId,
                       header->m_packetId,
                       header->m_flags,
                       //(int)userBuffer->getUserDataSize(), userBuffer->getUserDataPtr(),
                       (int)userBuffer->getUserDataSize(),
                       (int)bytesRead);
        
        /*
         RUDP_PRINTF("flags:\n");
         
         for(uint8_t i = 0; i < 8; i++)
         {
         if (RUDP_BIT_HAS(header->m_flags, 1 << i))
         {
         RUDP_PRINTF("%d %s\n", i, RUDP::PacketFlag_ToString((RUDP::PacketFlag)(1 << i)));
         }
         }
         
         RUDP_PRINTF("\n");*/
    }
    
    return bytesRead > 0;
}

void RUDP::Socket::updatePeers()
{
    RUDP::List<RUDP::Packet> packetsToSort = {};
    
    m_inQueueLock.lock();
    packetsToSort.inheritFrom(&m_inQueue);
    m_inQueueLock.unlock();
    
    for (RUDP::Packet *packet = packetsToSort.peek(); packet != NULL; packet = packetsToSort.peek())
    {
        RUDP::Peer *peer = getPeer(packet->getTargetAddr());
        if (peer)
        {
            peer->enqueueIncomingPacket(packet);
        }
        
        packetsToSort.pop();
    }
}

RUDP::Peer *RUDP::Socket::getPeer(uint32_t ipv4, uint16_t port)
{
    sockaddr_storage addr = {};
    sockaddr_in *targetAddr = (sockaddr_in*)&addr;
    targetAddr->sin_family = AF_INET;
    targetAddr->sin_port = htons(port);
    targetAddr->sin_addr.s_addr = htonl(ipv4);
    
    return getPeer(&addr);
}

RUDP::Peer *RUDP::Socket::getPeer(sockaddr_storage *addr)
{
    RUDP::Peer key(this, addr);
    RUDP::Peer *result = m_peerList.find(&key);
    if (!result)
    {
        result = m_peerList.insert(&key);
    }
    
    return result;
}

void RUDP::Socket::enqueueAcknowledgments(RUDP::List<RUDP::Packet> *list)
{
    m_ackQueueLock.lock();
    m_ackQueue.inheritFrom(list);
    m_ackQueueLock.unlock();
}

void RUDP::Socket::enqueueOutgoingPackets(RUDP::List<RUDP::Packet> *list)
{
    m_outQueueLock.lock();
    m_outQueue.inheritFrom(list);
    m_outQueueLock.unlock();
}