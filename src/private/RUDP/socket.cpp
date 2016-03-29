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
        
        printf("Error %s (%d: %.*S)\n", context, error, RUDP_ARRAYSIZE(buff), buff);
    }
#else
    int error = errno;
    
    if (error != 0 && error != EAGAIN)
    {
        char buff[256];
        RUDP_STRERROR(error, buff, RUDP_ARRAYSIZE(buff));
        printf("Error %s (%d: %.*s)\n", context, error, (int)RUDP_ARRAYSIZE(buff), buff);
    }
#endif
}

void RUDP::Message::prepareForReceiving(const char *messageBuffer, size_t bufferLen)
{
    m_data = messageBuffer;
    m_dataLen = bufferLen;
}

void RUDP::Message::prepareForSending(const char *dataToSend, size_t dataLen, uint32_t targetAddr, uint16_t targetPort, RUDP::ChannelId channel)
{
    sockaddr_storage target;
    sockaddr_in *targetPtr = (sockaddr_in*)&target;
    targetPtr->sin_family = AF_INET;
    targetPtr->sin_addr.s_addr = htonl(targetAddr);
    targetPtr->sin_port = htons(targetPort);
    
    prepareForSending(dataToSend, dataLen, &target, channel);
}

void RUDP::Message::prepareForSending(const char *dataToSend, size_t dataLen, sockaddr_storage *target, RUDP::ChannelId channel)
{
    m_data = dataToSend;
    m_dataLen = dataLen;
    m_channel = channel;
    m_peer = *target;
}

RUDP::PacketId RUDP::Socket::reservePacketsOnChannel(RUDP::ChannelId channel, RUDP::PacketId numNeeded)
{
    return std::atomic_fetch_add(&m_ChannelPacketIds[channel], numNeeded);
}

RUDP::Socket::Socket() :
m_ackTimeout(1000),
m_port(0),
m_handle(0),
m_ChannelPacketIds(NULL)
{
#ifdef _WIN32
    WSADATA wsaData;
    int error = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (error != 0)
    {
        PrintLastSocketError("WinSock Startup");
    }
#endif
    
    m_ChannelPacketIds = new std::atomic<RUDP::PacketId>[RUDP::MaxChannels];
    m_inQueueChannels.resize(RUDP::MaxChannels);
}

RUDP::Socket::~Socket()
{
    delete[] m_ChannelPacketIds;
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
    
    return true;
}

RUDP::SocketHandle RUDP::Socket::getHandle()
{
    return m_handle;
}

uint16_t RUDP::Socket::getPort()
{
    return m_port;
}

bool RUDP::Socket::flush()
{
    bool sent = false;
    
    for (RUDP::Packet *packet = m_outQueue.peek(); packet != NULL; packet = m_outQueue.peek())
    {
        sent = true;
        if(sendPacket(packet))
        {
            m_outQueue.pop();
            m_nodeStore.free(packet);
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
    bool received = false;
    
    for (uint32_t i = 0; i < attempts; i++)
    {
        RUDP::Packet *newPck = m_nodeStore.secure();
        if (!newPck)
        {
            break;
        }
        
        if (receivePacket(newPck))
        {
            received = true;
            
            // look for our channel's queue
            RUDP::PacketHeader *header = newPck->getHeader();
            RUDP::Queue<RUDP::Packet> *channelQueue = &m_inQueueChannels[header->m_channelId];
            
            bool addToQueue = channelQueue->peek() == NULL;
            
            if (channelQueue->peek() == NULL)
            {
                channelQueue->push(newPck);
            }
            else
            {
                if (RUDP_BIT_HAS(header->m_flags, RUDP::PacketFlag_InOrder))
                {
                    for (RUDP::Packet *pck = channelQueue->peek(); pck != NULL; pck = channelQueue->next(pck))
                    {
                        RUDP::Packet *nxt = channelQueue->next(pck);
                        if ((nxt == NULL || nxt->getHeader()->m_packetId > header->m_packetId) && pck->getHeader()->m_packetId < header->m_packetId)
                        {
                            channelQueue->pushAfter(pck, newPck);
                        }
                    }
                }
            }
            
            if (addToQueue)
            {
                m_inQueue.push(channelQueue);
            }
            
            if (RUDP_BIT_HAS(header->m_flags, RUDP::PacketFlag_IsAck))
            {
                for (RUDP::Packet *pck = m_ackQueue.peek(); pck != NULL; pck = m_ackQueue.next(pck))
                {
                    if (memcmp(pck->getHeader(), header, sizeof(RUDP::PacketHeader)) == 0)
                    {
                        pck = m_ackQueue.remove(pck);
                    }
                }
            }
        }
        else
        {
            break;
        }
    }
    
    return received;
}

void RUDP::Socket::setAckTimeout(uint32_t ms)
{
    m_ackTimeout = ms;
}

bool RUDP::Socket::acknowledge()
{
    bool sentAny = false;
    
    for (RUDP::Packet *pck = m_ackQueue.peek(); pck != NULL; pck = m_ackQueue.next(pck))
    {
        if (!m_nodeStore.hasSpace())
        {
            break;
        }
        
        uint32_t time = RUDP_GETTIMEMS;
        uint32_t pckTime = pck->getTimestamp();
        uint32_t diff = time - pckTime;
        
        if (diff > m_ackTimeout)
        {
            pck->setTimestamp(time);
            sentAny = true;
            enqueuePacket(pck);
        }
    }
    
    return sentAny;
}

uint32_t RUDP::Socket::update(uint32_t msTimeout)
{
    msTimeout += RUDP_GETTIMEMS;
    
    do
    {
        listen(256);
        acknowledge();
        flush();
    }
    while (msTimeout > RUDP_GETTIMEMS);
    
    return 0;
}

bool RUDP::Socket::sendPacket(RUDP::Packet *toWrite)
{
    size_t toSendLen = toWrite->getTotalSize();
    
    size_t sentBytes = sendto(m_handle, toWrite->getDataPtr(), toSendLen, 0, (sockaddr*)toWrite->getTargetAddr(), sizeof(sockaddr_storage));
    if(sentBytes != toSendLen)
    {
        PrintLastSocketError("Sending Packet");
        return false;
    }
    else
    {
        printf("Sent packet on channel %d:%d:%d -> (%d)\n\n",
               toWrite->getHeader()->m_channelId,
               toWrite->getHeader()->m_packetId,
               toWrite->getHeader()->m_flags,
               toWrite->getUserDataSize()
               //toWrite->getUserDataPtr(),
               //toWrite->getUserDataSize()
               );
        
        return true;
    }
}

bool RUDP::Socket::peekMessage(size_t &msgSize)
{
    RUDP::Queue<RUDP::Packet> *queue = m_inQueue.peek();
    if (queue)
    {
        RUDP::Packet *msg = queue->peek();
        
        if (msg)
        {
            msgSize = msg->getUserDataSize();
            return true;
        }
    }
    
    msgSize = 0;
    return false;
}

bool RUDP::Socket::receiveMessage(RUDP::Message *message)
{
    bool ret = false;
    RUDP::Queue<RUDP::Packet> *queue = m_inQueue.peek();
    
    if (queue)
    {
        RUDP::Packet *msg = queue->pop();
        
        if (msg)
        {
            RUDP::PacketHeader *header = msg->getHeader();
            
            memcpy((void*)message->m_data, msg->getUserDataPtr(), message->m_dataLen >= msg->getUserDataSize() ? msg->getUserDataSize() : message->m_dataLen);
            message->m_peer = *msg->getTargetAddr();
            message->m_channel = header->m_channelId;
            
            if (RUDP_BIT_HAS(header->m_flags, RUDP::PacketFlag_ConfirmDelivery))
            {
                enqueueAcknowledgement(msg);
            }
            
            msg = NULL;
            ret = true;
        }
        
        if (queue->peek() == NULL)
        {
            m_inQueue.pop();
        }
    }
    
    return false;
}

void RUDP::Socket::enqueuePacket(RUDP::Packet *pck)
{
    m_outQueue.push(pck);
}

RUDP::EnqueueMessageResult RUDP::Socket::enqueueMessage(RUDP::Message *message, RUDP::EnqueueMessageOption options)
{
    PacketHeader header = {};
    header.m_channelId = message->m_channel;
    
    if(RUDP_BIT_HAS(options, RUDP::EnqueueMessageOption_ConfirmDelivery))
    {
        RUDP_BIT_SET(header.m_flags, RUDP::PacketFlag_ConfirmDelivery);
    }
    
    if(RUDP_BIT_HAS(options, RUDP::EnqueueMessageOption_InOrder))
    {
        RUDP_BIT_SET(header.m_flags, RUDP::PacketFlag_InOrder);
    }
    
    size_t spaceForMessage = RUDP::PacketSize - sizeof(PacketHeader);
    const char *toWrite = message->m_data;
    
    uint16_t numPacketsNeeded = message->m_dataLen / spaceForMessage;
    numPacketsNeeded += (message->m_dataLen % spaceForMessage) != 0;
    
    if(numPacketsNeeded + m_nodeStore.getNumSecured() > m_nodeStore.getNumTotal())
    {
        return RUDP::EnqueueMessageResult_OutQueueFull;
    }
    
    RUDP::PacketId packetId = reservePacketsOnChannel(message->m_channel, numPacketsNeeded);
    
    for(size_t dataLeft = message->m_dataLen; dataLeft > 0; /* nada */)
    {
        size_t toWriteLen = dataLeft;
        header.m_packetId = packetId++;
        
        if(spaceForMessage > dataLeft)
        {
            RUDP_BIT_SET(header.m_flags, RUDP::PacketFlag_EndOfMessage);
        }
        else
        {
            toWriteLen = spaceForMessage;
            RUDP_BIT_UNSET(header.m_flags, RUDP::PacketFlag_EndOfMessage);
        }
        
        RUDP::Packet *writeBuffer = m_nodeStore.secure();
        writeBuffer->setWritePosition(0);
        writeBuffer->setHeader(&header);
        writeBuffer->write(toWrite, toWriteLen);
        writeBuffer->setTargetAddr(&message->m_peer);
        
        m_outQueue.push(writeBuffer);
        dataLeft -= toWriteLen;
        toWrite += toWriteLen;
    }
    
    return RUDP::EnqueueMessageResult_Success;
}

void RUDP::Socket::enqueueAcknowledgement(RUDP::Packet *ack)
{
    RUDP_BIT_SET(ack->getHeader()->m_flags, RUDP::PacketFlag_IsAck);
    m_ackQueue.push(ack);
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
        
        RUDP::PacketHeader *header = userBuffer->getHeader();
        
        printf("received packet on channel %d:%d:%d -> (%d, %d)\n\n",
               header->m_channelId,
               header->m_packetId,
               header->m_flags,
               //(int)userBuffer->getUserDataSize(), userBuffer->getUserDataPtr(), 
               (int)userBuffer->getUserDataSize(),
               (int)bytesRead);
        
        /*
         printf("flags:\n");
         
         for(uint8_t i = 0; i < 8; i++)
         {
         if (RUDP_BIT_HAS(header->m_flags, 1 << i))
         {
         printf("%d %s\n", i, RUDP::PacketFlag_ToString((RUDP::PacketFlag)(1 << i)));
         }
         }
         
         printf("\n");*/
    }
    
    return bytesRead > 0;
}