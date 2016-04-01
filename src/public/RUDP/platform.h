//
//  platform.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/24.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_platform_h
#define RUDP_platform_h

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2ipdef.h>

typedef int socklen_t;
typedef long ssize_t;
typedef char *sockdataptr_t;

namespace RUDP
{
    typedef ::SOCKET SocketHandle;
}

#define RUDP_CLOSESOCKET(x) ::closesocket(x)

#define RUDP_STRERROR(err, buff, len) ::strerror_s(buff, len, err)

#define RUDP_PACKEDSTRUCT(x) __pragma( pack(push, 1) ) x __pragma( pack(pop) )

#define RUDP_GETTIMEMS ::timeGetTime()

#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <cmath>
#include <stdio.h>

typedef void *sockdataptr_t;

namespace RUDP
{
    typedef int SocketHandle;
    
    inline uint64_t getUnixMS()
    {
        struct timeval te;
        gettimeofday(&te, NULL); // get current time
        uint64_t milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // caculate milliseconds
        //printf("milliseconds: %lld\n", milliseconds);
        return milliseconds;
    }
}

#define RUDP_CLOSESOCKET(x) ::close(x)

#define RUDP_STRERROR(err, buff, len) ::strerror_r(err, buff, len)

#define RUDP_PACKEDSTRUCT(x) x __attribute__((__packed__))

#define RUDP_GETTIMEMS RUDP::getUnixMS()

#endif

#endif