//
//  util.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/25.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_util_h
#define RUDP_util_h

#include <mutex>
#include <cstdarg>

#define RUDP_BIT_SET(a, b) (a) |= (b)
#define RUDP_BIT_UNSET(a, b) (a) &= (~(b))
#define RUDP_BIT_HAS(a, b) (((a) & (b)) == (b))
#define RUDP_BIT_HAS_ANY(a, b) (((a) & (b)) != 0)

#define RUDP_STRINGIFY_CASE(x) case x: return #x
#define RUDP_ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))

namespace RUDP
{
    class Print
    {
    private:
        static std::mutex s_lock;
        
    public:
        static void f(const char *format, ...)
        {
            va_list args;
            va_start(args, format);
            
            s_lock.lock();
            ::vprintf(format, args);
            s_lock.unlock();
            
            va_end(args);
        }
    };
}

#endif
