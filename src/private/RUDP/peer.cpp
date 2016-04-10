//
//  peer.cpp
//  RUDP
//
//  Created by Timothy Smale on 2016/03/24.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//
#include <RUDP/peer.h>
#include <RUDP/socket.h>

void RUDP::PeerMessage::prepareForReceiving(char *messageBuffer, size_t bufferLen)
{
    m_data = messageBuffer;
    m_dataLen = bufferLen;
    m_channel = 0;
    m_peer = NULL;
}

void RUDP::PeerMessage::prepareForSending(char *dataToSend, size_t dataLen, RUDP::Peer *target, RUDP::ChannelId channel)
{
    m_data = dataToSend;
    m_dataLen = dataLen;
    m_channel = channel;
    m_peer = target;
}

RUDP::PacketId RUDP::Peer::reservePacketsOnChannel(RUDP::ChannelId channel, RUDP::PacketId numNeeded)
{
    return std::atomic_fetch_add(&m_ChannelPacketIds[channel], numNeeded);
}

RUDP::Peer::Peer() : Peer(NULL, NULL)
{
    
}

RUDP::Peer::Peer(RUDP::Socket *socket, sockaddr_storage *addr) :
m_ChannelPacketIds(new std::atomic<RUDP::PacketId>[RUDP::MaxChannels]),
m_socket(socket),
m_inQueueChannels(std::vector<RUDP::Channel>(RUDP::MaxChannels)),
m_addr(addr == NULL ? sockaddr_storage() : *addr)
{
    
}

RUDP::Peer::~Peer()
{
    delete[] m_ChannelPacketIds;
}

// generic hash
uint32_t RUDP::Peer::hash()
{
    if (!m_hash)
    {
        m_hash = 31;
        const char *data = (const char*)&m_addr;
        
        for (size_t i = 0; i < sizeof(sockaddr_storage); i++)
        {
            m_hash = (m_hash * 54059) ^ (data[i] * 76963);
        }
    }
    
    return m_hash;
}

bool RUDP::Peer::equals(RUDP::Peer *peer)
{
    return memcmp(&peer->m_addr, &m_addr, sizeof(sockaddr_storage)) == 0;
}

sockaddr_storage *RUDP::Peer::getAddress()
{
    return &m_addr;
}

bool RUDP::Peer::peekMessage(size_t &msgSize)
{
    msgSize = 0;
    RUDP::Channel **queue = m_inQueue.peek();
    
    if (queue)
    {
        RUDP::MessageStart *msg = (*queue)->m_messages.peek();
        
        if (msg)
        {
            RUDP::Packet *pck = msg->m_first;
            RUDP::Channel *channel = &m_inQueueChannels[pck->getHeader()->m_channelId];
            
            while (pck)
            {
                msgSize += pck->getUserDataSize();
                
                if (RUDP_BIT_HAS(pck->getHeader()->m_flags, RUDP::PacketFlag::PacketFlag_EndOfMessage))
                {
                    break;
                }
                
                pck = channel->m_queue.next(pck);
            }
            
            return true;
        }
    }
    
    return false;
}

RUDP::EnqueueMessageResult RUDP::Peer::enqueueMessage(RUDP::PeerMessage *message, RUDP::EnqueueMessageOption options)
{
    PacketHeader header = {};
    header.m_channelId = message->m_channel;
    
    if (RUDP_BIT_HAS(options, RUDP::EnqueueMessageOption_ConfirmDelivery))
    {
        RUDP_BIT_SET(header.m_flags, RUDP::PacketFlag_ConfirmDelivery);
    }
    
    if (RUDP_BIT_HAS(options, RUDP::EnqueueMessageOption_InOrder))
    {
        RUDP_BIT_SET(header.m_flags, RUDP::PacketFlag_InOrder);
    }
    
    size_t sizeofHeader = sizeof(RUDP::PacketHeader);
    size_t spaceForMessage = RUDP::PacketSize - sizeofHeader;
    const char *toWrite = message->m_data;
    
    uint16_t numPacketsNeeded = message->m_dataLen / spaceForMessage;
    numPacketsNeeded += (message->m_dataLen % spaceForMessage) != 0;
    
    RUDP::PacketId packetId = reservePacketsOnChannel(message->m_channel, numPacketsNeeded);
    bool start = true;
    
    uint16_t numPacketsEnqueued = 0;
    for (size_t dataLeft = message->m_dataLen; dataLeft > 0; /* nada */)
    {
        size_t toWriteLen = dataLeft;
        header.m_packetId = packetId++;
        
        if (start)
        {
            RUDP_BIT_SET(header.m_flags, RUDP::PacketFlag_StartOfMessage);
            start = false;
        }
        
        if (spaceForMessage > dataLeft)
        {
            RUDP_BIT_SET(header.m_flags, RUDP::PacketFlag_EndOfMessage);
        }
        else
        {
            toWriteLen = spaceForMessage;
            RUDP_BIT_UNSET(header.m_flags, RUDP::PacketFlag_EndOfMessage);
        }
        
        RUDP::Packet *writeBuffer = m_outQueue.push();
        if (writeBuffer)
        {
            writeBuffer->setWritePosition(0);
            writeBuffer->setHeader(&header);
            writeBuffer->write(toWrite, toWriteLen);
            writeBuffer->setTargetAddr(message->m_peer->getAddress());
            
            dataLeft -= toWriteLen;
            toWrite += toWriteLen;
            numPacketsEnqueued++;
        }
        else
        {
            for (uint16_t i = 0; i < numPacketsEnqueued; i++)
            {
                m_outQueue.pop();
            }
        }
    }
    
    return RUDP::EnqueueMessageResult_Success;
}

void RUDP::Peer::flushToSocket()
{
    m_socket->enqueueOutgoingPackets(&m_outQueue);
    m_socket->enqueueAcknowledgments(&m_ackQueue);
}

bool RUDP::Peer::enqueueIncomingPacket(RUDP::Packet *newPck)
{
    // look for our channel's queue
    RUDP::PacketHeader *header = newPck->getHeader();
    RUDP::Channel *channel = &m_inQueueChannels[header->m_channelId];
    bool addChannel = channel->m_messages.peek() == NULL;
    
    if (RUDP_BIT_HAS(header->m_flags, RUDP::PacketFlag_IsAck))
    {
        for (RUDP::Packet *pck = m_ackQueue.peek(); pck != NULL; pck = m_ackQueue.next(pck))
        {
            if (memcmp(pck->getHeader(), header, sizeof(RUDP::PacketHeader)) == 0)
            {
                m_ackQueue.remove(pck);
                break;
            }
        }
        
        return false;
    }
    
    RUDP::Packet *pck = channel->m_queue.peekEnd();
    RUDP::MessageStart *msgAdded = NULL;
    
    if (!pck)
    {
        newPck = channel->m_queue.push(newPck);
    }
    else
    {
        // attempt to insert in-order
        while (pck && pck->getHeader()->m_packetId > newPck->getHeader()->m_packetId)
        {
            pck = channel->m_queue.prev(pck);
        }
        
        if (pck)
        {
            newPck = channel->m_queue.pushAfter(pck, newPck);
        }
        else
        {
            newPck = channel->m_queue.pushBefore(channel->m_queue.peek(), newPck);
        }
    }
    
    if (RUDP_BIT_HAS(header->m_flags, RUDP::PacketFlag_EndOfMessage | RUDP::PacketFlag_StartOfMessage))
    {
        msgAdded = channel->addMessage(newPck, newPck);
    }
    
    else if (RUDP_BIT_HAS(header->m_flags, RUDP::PacketFlag_StartOfMessage))
    {
        RUDP::Packet *end = channel->findMessageEnd(newPck);
        if (end)
        {
            msgAdded = channel->addMessage(newPck, end);
        }
    }
    
    else if (RUDP_BIT_HAS(header->m_flags, RUDP::PacketFlag_EndOfMessage))
    {
        RUDP::Packet *start = channel->findMessageStart(newPck);
        if (start)
        {
            msgAdded = channel->addMessage(start, newPck);
        }
    }
    
    else
    {
        // look for the start
        RUDP::Packet *start = channel->findMessageStart(newPck);
        if (start)
        {
            RUDP::Packet *end = channel->findMessageEnd(newPck);
            
            if (end)
            {
                msgAdded = channel->addMessage(start, end);
            }
        }
    }
    
    if (addChannel)
    {
        m_inQueue.push(&channel);
    }
    
    return true;
}

bool RUDP::Peer::receiveMessage(RUDP::PeerMessage *message)
{
    bool ret = false;
    RUDP::Channel **channel = m_inQueue.peek();
    
    if (channel)
    {
        RUDP::MessageStart *start = (*channel)->m_messages.peek();
        
        if (start)
        {
            RUDP::Packet *pck = start->m_first;
            RUDP::Channel *channel = &m_inQueueChannels[pck->getHeader()->m_channelId];
            char *buffer = message->m_data;
            size_t bufferLen = message->m_dataLen;
            
            while (pck && bufferLen)
            {
                size_t toCopy = bufferLen > pck->getUserDataSize() ? pck->getUserDataSize() : bufferLen;
                memcpy(buffer, pck->getUserDataPtr(), toCopy);
                buffer += toCopy;
                bufferLen -= toCopy;
                
                RUDP::Packet *toRemove = pck;
                pck = channel->m_queue.next(pck);
                channel->m_queue.remove(toRemove);
            }
            
            ret = true;
            channel->m_messages.pop();
        }
        
        if ((*channel)->m_queue.peek() == NULL)
        {
            m_inQueue.pop();
        }
    }
    
    return ret;
}