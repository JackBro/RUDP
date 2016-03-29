//
//  util.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/25.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_util_h
#define RUDP_util_h

#define RUDP_BIT_SET(a, b) (a) |= (b)
#define RUDP_BIT_UNSET(a, b) (a) &= (~(b))
#define RUDP_BIT_HAS(a, b) (((a) & (b)) == (b))

#define RUDP_STRINGIFY_CASE(x) case x: return #x
#define RUDP_ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))

#endif
