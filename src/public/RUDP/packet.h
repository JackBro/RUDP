//
//  packet.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/24.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_packet_h
#define RUDP_packet_h

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <RUDP/util.h>
#include <RUDP/platform.h>

namespace RUDP
{
    typedef uint8_t ChannelId;
    typedef uint16_t PacketId;
    
    const size_t PacketSize = 1400;
    const size_t MaxChannels = UINT8_MAX;
    
    enum PacketFlag : uint8_t
    {
        PacketFlag_None            = 0,
        PacketFlag_ConfirmDelivery = 1,        // Will send X number of times at intervals of Y ms, until target sends ack or limit X is reached
        PacketFlag_IsAck           = 1 << 2,   // This packet is an acknowledgement of delivery - besides this bit, the header will match perfectly but not include message contents
        PacketFlag_InOrder         = 1 << 3,   // This packet cannot be accessed by the application until the prior packet on this channel has been processed or assumed lost
        PacketFlag_EndOfMessage    = 1 << 4    // Indicates that this is the end of the message - if unset, this is part of a fragmented message and should be reassembled with neighbouring packets
                                                // - If any of the packets are presumed lost, the entire message is discarded.
                                                // - If ack is missing on one packet, only that packet is resent
    };
    
    inline const char *PacketFlag_ToString(PacketFlag flag)
    {
        switch(flag)
        {
                RUDP_STRINGIFY_CASE(PacketFlag_None);
                RUDP_STRINGIFY_CASE(PacketFlag_IsAck);
                RUDP_STRINGIFY_CASE(PacketFlag_ConfirmDelivery);
                RUDP_STRINGIFY_CASE(PacketFlag_EndOfMessage);
                RUDP_STRINGIFY_CASE(PacketFlag_InOrder);
        }
        
        return "UNKNOWN";
    }
    
    inline void operator |=(PacketFlag &a, const PacketFlag &b)
    {
        a = (PacketFlag)(((uint8_t)a) | ((uint8_t)b));
    }
    
    inline void operator &=(PacketFlag &a, const PacketFlag &b)
    {
        a = (PacketFlag)(((uint8_t)a) & ((uint8_t)b));
    }
    
    inline PacketFlag operator ~(const PacketFlag &a)
    {
        return (PacketFlag)(~((uint8_t)a));
    }
    
    RUDP_PACKEDSTRUCT(struct PacketHeader
                      {
                          RUDP::PacketId m_packetId;
                          RUDP::ChannelId m_channelId;
                          PacketFlag m_flags;
                      });
    
    class Packet
    {
    private:
        char m_buffer[RUDP::PacketSize];
        sockaddr_storage m_targetAddr;
        uint32_t m_timestamp;
        uint16_t m_readPosition;
        uint16_t m_writePosition;
        
    public:
        Packet() : m_readPosition(0), m_writePosition(0), m_timestamp(0)
        {
            memset(&m_targetAddr, 0, sizeof(m_targetAddr));
        }
        
        void setTimestamp(uint32_t ms);
        uint32_t getTimestamp();
        
        void setHeader(RUDP::PacketHeader *header);
        RUDP::PacketHeader *getHeader();
        
        void setTargetAddr(sockaddr_storage *addr);
        sockaddr_storage * getTargetAddr();
        
        uint16_t read(RUDP::Packet *buffer, size_t len);
        uint16_t write(RUDP::Packet *buffer, size_t len);
        
        template <typename T>
        inline uint16_t read(const T *buffer, size_t amount)
        {
            size_t numAvailable = RUDP::PacketSize - m_readPosition - sizeof(RUDP::PacketHeader);
            amount *= sizeof(T);
            
            if(amount > numAvailable)
            {
                amount = numAvailable;
            }
            
            memcpy((void*)buffer, m_buffer + m_readPosition + sizeof(RUDP::PacketHeader), amount);
            m_readPosition += amount;
            
            return amount / sizeof(T);
        }
        
        template <typename T>
        inline uint16_t write(const T *buffer, size_t amount)
        {
            size_t numAvailable = RUDP::PacketSize - m_writePosition - sizeof(RUDP::PacketHeader);
            amount *= sizeof(T);
            
            if(amount > numAvailable)
            {
                amount = numAvailable;
            }
            
            memcpy(m_buffer + m_writePosition + sizeof(RUDP::PacketHeader), buffer, amount);
            m_writePosition += amount;
            
            return amount / sizeof(T);
        }
        
        const char *getDataPtr();
        const char *getUserDataPtr();
        
        uint16_t getTotalSize();
        uint16_t getUserDataSize();
        
        void resetReadPosition();
        void resetWritePosition();
        uint16_t getReadPosition();
        uint16_t getWritePosition();
        bool setWritePosition(uint16_t len);
        bool setReadPosition(uint16_t len);
    };
}

#endif
