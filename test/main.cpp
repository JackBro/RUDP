//
//  main.cpp
//  RUDP
//
//  Created by Timothy Smale on 2016/03/19.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <iostream>
#include <vector>
#include <RUDP/RUDP.h>
#include <thread>

// todo: prevent packet ID overflow (use timestamp?)
// todo: deal with timestamp overflow
// todo: proper flow control
// todo: appending packets together when there is space
// todo: error event callbacks

const char *dataToSend = "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.At vero eos et accusam et justo duo dolores et ea rebum.Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.At vero eos et accusam et justo duo dolores et ea rebum.Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.At vero eos et accusam et justo duo dolores et ea rebum.Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.\
Duis autem vel eum iriure dolor in hendrerit in vulputate velit esse molestie consequat, vel illum dolore eu feugiat nulla facilisis at vero eros et accumsan et iusto odio dignissim qui blandit praesent luptatum zzril delenit augue duis dolore te feugait nulla facilisi.Lorem ipsum dolor sit amet, consectetuer adipiscing elit, sed diam nonummy nibh euismod tincidunt ut laoreet dolore magna aliquam erat volutpat.\
Ut wisi enim ad minim veniam, quis nostrud exerci tation ullamcorper suscipit lobortis nisl ut aliquip ex ea commodo consequat.Duis autem vel eum iriure dolor in hendrerit in vulputate velit esse molestie consequat, vel illum dolore eu feugiat nulla facilisis at vero eros et accumsan et iusto odio dignissim qui blandit praesent luptatum zzril delenit augue duis dolore te feugait nulla facilisi.\
Nam liber tempor cum soluta nobis eleifend option congue nihil imperdiet doming id quod mazim placerat facer possim assum.Lorem ipsum dolor sit amet, consectetuer adipiscing elit, sed diam nonummy nibh euismod tincidunt ut laoreet dolore magna aliquam erat volutpat.Ut wisi enim ad minim veniam, quis nostrud exerci tation ullamcorper suscipit lobortis nisl ut aliquip ex ea commodo consequat.\
Duis autem vel eum iriure dolor in hendrerit in vulputate velit esse molestie consequat, vel illum dolore eu feugiat nulla facilisis.\
At vero eos et accusam et justo duo dolores et ea rebum.Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.At vero eos et accusam et justo duo dolores et ea rebum.Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.Lorem ipsum dolor sit amet, consetetur sadipscing elitr, At accusam aliquyam diam diam dolore dolores duo eirmod eos erat, et nonumy sed tempor et et invidunt justo labore Stet clita ea et gubergren, kasd magna no rebum.sanctus sea sed takimata ut vero voluptua.est Lorem ipsum dolor sit amet.Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat.\
Consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.At vero eos et accusam et justo duo dolores et ea rebum.Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.At vero eos et accusam et justo duo dolores et ea rebum.Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.At vero eos et accusam et justo duo dolores et ea rebum.Stet clita kasd gubergren, no sea takimata sanctus";

RUDP::Socket sck;

void listenThread()
{
    for (size_t timeout = 0; timeout < 60; timeout++)
    {
        if (sck.update(500))
        {
            timeout = 0;
        }
    }
}

int main(int argc, const char * argv[])
{
    uint32_t serverIP = 127 << 24 | 1;
    uint16_t serverPort = 6112;
    
    if(!sck.open(serverPort))
    {
        return EXIT_FAILURE;
    }
    
    std::thread thread(listenThread);
    
    RUDP::Peer *peer = sck.getPeer(serverIP, serverPort);
    if (!peer)
    {
        return EXIT_FAILURE;
    }
    
    RUDP::PeerMessage message = {};
    message.prepareForSending((char*)dataToSend, strlen(dataToSend), peer, 5);
    
    peer->enqueueMessage(&message, RUDP::EnqueueMessageOption_ConfirmDelivery);
    peer->flushToSocket();
    
    std::vector<char> readBuffer;
    for(size_t bytesRead = 1, timeout = 0; (bytesRead > 0 || timeout < 6000); timeout++ )
    {
        sck.updatePeers();
        bytesRead = 0;
        
        while(peer->peekMessage(bytesRead))
        {
            readBuffer.resize(bytesRead);
            
            message.prepareForReceiving(readBuffer.data(), bytesRead);
            if(peer->receiveMessage(&message))
            {
                RUDP::Print::f("peer received message (%d) on channel %d -> %.*s (%d)\n\n",
                               strncmp(dataToSend, message.m_data, strlen(dataToSend)),
                               message.m_channel,
                               (int)message.m_dataLen,
                               message.m_data,
                               (int)message.m_dataLen);
            }
        }
    }
    
    thread.join();
    
    return EXIT_SUCCESS;
}
