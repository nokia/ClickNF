// /*
//  * tcpeventqueue.{cc,hh} -- the TCP event queue class
//  * Massimo Gallo
//  *
//  * Copyright (c) 2017 Nokia Bell Labs
//  *
//  * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
//  * 
//  * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//  * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
//  *    in the documentation and/or other materials provided with the distribution.
//  * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived
//  *    from this software without specific prior written permission.
//  *
//  *
//  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
//  * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
//  * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
//  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  *
//  *
//  */
// 
// #include <click/config.h>
// #include "tcpeventqueue.hh"
// CLICK_DECLS
// 
// static thread_local TCPHashAllocator pool(sizeof(TCPEvent));
// 
// TCPEvent *
// TCPEventQueue::allocate()
// {
// 
// 	return reinterpret_cast<TCPEvent *>(pool.allocate());
// }
// 
// TCPEvent *
// TCPEventQueue::allocate(TCPState *s)
// {
// 	TCPEvent *e = allocate();
// 	if (e)
// 		e->state = s;
// 
// 	return e;
// }
// 	
// void
// TCPEventQueue::deallocate(TCPEvent *e)
// {
// 	pool.deallocate(e);
// }
// 
// 
// CLICK_ENDDECLS
// ELEMENT_PROVIDES(TCPEvent)
// ELEMENT_PROVIDES(TCPEventQueue)
