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

// todo: prevent packet ID overflow (use timestamp?)
// todo: deal with timestamp overflow
// todo: proper flow control
// todo: concurrency-friendly when reading / writing to queues & securing packet nodes
// todo: appending packets together when there is space
// todo: error event callbacks

int main(int argc, const char * argv[])
{
	uint32_t serverIP = 127 << 24 | 1;
	uint16_t serverPort = 6112;

    RUDP::Socket socket;
    if(!socket.open(serverPort))
    {
        return EXIT_FAILURE;
    }
    
	RUDP::Message message = {};
	const char *dataToSend = "Hello, World!";
	message.prepareForSending(dataToSend, strlen(dataToSend), serverIP, serverPort, 0);
	socket.enqueueMessage(&message, RUDP::EnqueueMessageOption_ConfirmDelivery);

    std::vector<char> readBuffer;
    
    for(size_t bytesRead = 1, timeout = 0; (bytesRead > 0 || timeout < 60); timeout++ )
    {
		socket.update(500);
        
        for(socket.peekMessage(bytesRead); bytesRead > 0; socket.peekMessage(bytesRead))
        {
            readBuffer.resize(bytesRead);
            
            message.prepareForReceiving(readBuffer.data(), bytesRead);
            if(socket.receiveMessage(&message))
            {
                printf("user received message on channel %d -> %.*s (%d)\n\n",
                       message.m_channel,
                       (int)message.m_dataLen,
                       message.m_data,
                       (int)message.m_dataLen);
            }
        }
    }
    
    return EXIT_SUCCESS;
}
