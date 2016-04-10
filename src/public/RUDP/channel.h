//
//  channel.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/28.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_channel_h
#define RUDP_channel_h

#include <RUDP/list.h>
#include <RUDP/packet.h>

namespace RUDP
{
    struct Channel
    {
        RUDP::List<RUDP::Packet> m_queue;
        RUDP::List<RUDP::MessageStart> m_messages;
        RUDP::PacketId m_lastAcknowledged;
        
        Channel() : m_lastAcknowledged(0) {}
        
        RUDP::MessageStart *addMessage(RUDP::Packet *start, RUDP::Packet *end);
        
        RUDP::Packet *findMessageEnd(RUDP::Packet *newPck);
        RUDP::Packet *findMessageStart(RUDP::Packet *newPck);
    };
}

#endif