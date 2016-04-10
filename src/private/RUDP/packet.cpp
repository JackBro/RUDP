//
//  packet.cpp
//  RUDP
//
//  Created by Timothy Smale on 2016/03/24.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <RUDP/packet.h>
#include <RUDP/platform.h>

void RUDP::Packet::setHeader(RUDP::PacketHeader *header)
{
    // it better be a packed struct!
    memcpy(m_buffer, header, sizeof(RUDP::PacketHeader));
}

RUDP::PacketHeader *RUDP::Packet::getHeader()
{
    return (PacketHeader*)m_buffer;
}

sockaddr_storage *RUDP::Packet::getTargetAddr()
{
    return &m_targetAddr;
}

void RUDP::Packet::setTargetAddr(sockaddr_storage *addr)
{
    m_targetAddr = *addr;
}

void RUDP::Packet::resetReadPosition()
{
    m_readPosition = 0;
}

void RUDP::Packet::resetWritePosition()
{
    m_writePosition = 0;
}

uint16_t RUDP::Packet::read(RUDP::Packet *buffer, size_t len)
{
    buffer->setHeader(getHeader());
    return read(buffer->getUserDataPtr(), len);
}

uint16_t RUDP::Packet::write(RUDP::Packet *buffer, size_t len)
{
    setHeader(buffer->getHeader());
    return write(buffer->getUserDataPtr(), len);
}

void RUDP::Packet::setTimestamp(uint64_t time)
{
    m_timestamp = time;
}

uint64_t RUDP::Packet::getTimestamp()
{
    return m_timestamp;
}

const char *RUDP::Packet::getDataPtr()
{
    return m_buffer;
}

const char *RUDP::Packet::getUserDataPtr()
{
    return m_buffer + sizeof(PacketHeader);
}

uint16_t RUDP::Packet::getTotalSize()
{
    return getWritePosition() + sizeof(RUDP::PacketHeader);
}

uint16_t RUDP::Packet::getUserDataSize()
{
    return getWritePosition();
}

uint16_t RUDP::Packet::getReadPosition()
{
    return m_readPosition;
}

uint16_t RUDP::Packet::getWritePosition()
{
    return m_writePosition;
}

bool RUDP::Packet::setWritePosition(uint16_t len)
{
    if(len <= RUDP::PacketSize - sizeof(RUDP::PacketHeader))
    {
        m_writePosition = len;
        return true;
    }
    
    return false;
}

bool RUDP::Packet::setReadPosition(uint16_t len)
{
    if(len <= RUDP::PacketSize - sizeof(RUDP::PacketHeader))
    {
        m_readPosition = len;
        return true;
    }
    
    return false;
}