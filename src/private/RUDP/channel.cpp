//
//  channel.cpp
//  RUDP
//
//  Created by Timothy Smale on 2016/03/24.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <RUDP/channel.h>

RUDP::MessageStart *RUDP::Channel::addMessage(RUDP::Packet *first, RUDP::Packet *last)
{
    if (RUDP_BIT_HAS(first->getHeader()->m_flags, RUDP::PacketFlag_InOrder))
    {
        // ensure is behind message needed
        RUDP::MessageStart start = {};
        start.m_first = first;
        start.m_last = last;
        start.m_isAvailable = false;
        
        RUDP::MessageStart *toCheck = m_messages.peekEnd();
        while (toCheck)
        {
            if (toCheck->m_first->getHeader()->m_packetId < first->getHeader()->m_packetId)
            {
                start.m_isAvailable = toCheck == m_messages.peek();
                return m_messages.pushAfter(toCheck, &start);
            }
            
            toCheck = m_messages.prev(toCheck);
        }
        
        start.m_isAvailable = true;
        return m_messages.push(&start);
    }
    else
    {
        RUDP::MessageStart *start = m_messages.push();
        start->m_first = first;
        start->m_last = last;
        return start;
    }
}

RUDP::Packet *RUDP::Channel::findMessageStart(RUDP::Packet *newPck)
{
    RUDP::Packet *pck = m_queue.prev(newPck);
    RUDP::Packet *prevPck = newPck;
    
    while (pck)
    {
        if (prevPck->getHeader()->m_packetId != pck->getHeader()->m_packetId + 1)
        {
            break;
        }
        
        if (RUDP_BIT_HAS(pck->getHeader()->m_flags, RUDP::PacketFlag_StartOfMessage))
        {
            return pck;
        }
        else if (RUDP_BIT_HAS(pck->getHeader()->m_flags, RUDP::PacketFlag_EndOfMessage))
        {
            break;
        }
        
        prevPck = pck;
        pck = m_queue.prev(pck);
    }
    
    return NULL;
}

RUDP::Packet * RUDP::Channel::findMessageEnd(RUDP::Packet *newPck)
{
    RUDP::Packet *pck = m_queue.next(newPck);
    RUDP::Packet *prevPck = newPck;
    
    while (pck)
    {
        if (prevPck->getHeader()->m_packetId != pck->getHeader()->m_packetId - 1)
        {
            break;
        }
        
        if (RUDP_BIT_HAS(pck->getHeader()->m_flags, RUDP::PacketFlag_EndOfMessage))
        {
            return pck;
        }
        else if (RUDP_BIT_HAS(pck->getHeader()->m_flags, RUDP::PacketFlag_StartOfMessage))
        {
            break;
        }
        
        prevPck = pck;
        pck = m_queue.next(pck);
    }
    
    return NULL;
}