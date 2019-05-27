ClickNF
---------------------
The code published in the repository is partly described in "CliMB: Enabling Network Function Composition with 
Click Middleboxes" paper available at https://dl.acm.org/citation.cfm?id=2940152 . CliMB provides a full-fledged 
modular TCP layer supporting congestion control, TCP options, both blocking and nonblocking I/O, as well as socket 
and zero-copy APIs to applications. As a result, any TCP network function may now be realized in Click. ClickNF 
extends CliMB in several directions. For instance, ClickNF takes advantage of hardware offloading, multicore 
scalability, timing wheels, and an epoll-based API to improve performance. Finally, L7 modularity and SSL/TLS 
termination provide building blocks for novel network functions to be deployed with little effort. 

**C lickNF has been presented at Usenix ATC 2018 https://www.usenix.org/conference/atc18/presentation/gallo ** 

If you have any question/issue with the code, please use github or contact me at massimo.gallo@nokia-bell-labs.com

COPYRIGHT AND LICENSE
---------------------

ClickNF is a fork of the original Click project available under https://github.com/kohler/click The code directly 
modified from Click is distributed under the Click license, a version of the MIT License. See the 'LICENSE' file for 
details Each source file should identify its license. Source files that do not identify a specific license are covered 
by the Click license.

As for Click some parts of the code are distributed under a different license. The specific licenses are listed below.

drivers/e1000*, etc/linux-*-patch, linuxmodule/proclikefs.c: These portions of the Click software are derived from the 
Linux kernel, and are thus distributed under the GNU General Public License, version 2. The GNU General Public License 
is available via the Web at <http://www.gnu.org/licenses/gpl.html>, or in the COPYING file in this directory.

include/click/bigint.hh: This portion of the Click software derives from the GNU Multiple Precision Arithmetic Library, 
and is thus distributed under the GNU Lesser General Public License, version 3. This license is available via the Web 
at <http://www.gnu.org/licenses/lgpl.html>. The LGPL specifically permits direct linking with code with other licenses. 

elements/tcp/*, elements/userlevel/dpdk.*, elements/standard/batcher.* elements/standard/unbatcher.*, 
elements/standard/join.*, elements/app/echoserver.*, elements/app/ssl*, lib/packetqueue.*: 
These parts of the code refers to ClickNF and are distributed under the BSD 3-clause License.

Element code that uses only Click's interfaces will *not* be derived from the Linux kernel. (For instance, those 
interfaces have multiple implementations, including some that run at user level.) Thus, for element code that uses only 
Click's interfaces, the BSD-like Click license applies, not the GPL or the LGPL.

SETUP
-----------------------

ClickNF uses few external libraries that can be installed separately: 

1) DPDK: ClickNF has its own version of a DPDK module, i.e., different from the one already present in click codebase. ClickNF has been tested with 
   dpdk from version 16.11 up to 18.05. To install and configure DPDK please refer to http://dpdk.org/ 

2) OpenSSL: ClickNF provide a set of modules to interface with OpenSSL. To install it (Debian, Ubuntu) please use 

apt-get install openssl

3) libboost-1.61 context. To install libboost's context library (a coroutine library provided by libboost) please refer to 
http://www.boost.org/ for more details about installation. Make sure libboost_context.so.1.61.0 and libboost_context.so are 
installed in /usr/lib/x86_64-linux-gnu/

ClickNF automatically uses context library if it is installed (up to libboost-1.61). If context is not installed in the system 
ClickNF uses the slowest coroutine library provided in linux, ucontext. 
Please refer to http://man7.org/linux/man-pages/man3/makecontext.3.html for more details. 

CONFIGURE AND MAKE
-----------------------

To configure ClicNF run this command in its root directory:

./configure --disable-linuxmodule --enable-user-multithread --enable-dpdk --enable-epoll --enable-openssl --enable-batch --enable-aqm;

in which:

- --enable-epoll 			 	enables epoll data structure
- --enable-user-multithread --enable-dpdk 	enable DPDK input interfaces and packet wrapping (RTE_SDK and RTE_TARGET should be exported)
- --enable-openssl 				enables ssl* elements
- --enable-batch 				enables batch processing between elements (May need batcher and unbatcher elements in the graph)
- --enable-aqm					enables active queue management techniques

To compile ClickNF run this command in its root directory:

make -j32

RUN
-----------------------

To run ClickNF run this command in its root directory:

sudo bin/click --dpdk -c0x01 -n10 --  conf/conf/bulk-server.click

in which -c and -n are DPDK parameters sets the number of cpus and memory channels respectively. 

-----------------------
EXAMPLES
-----------------------

ClickNF applications using the TCP stack can be developed in two ways a)monolithic (everything inside a single module) b) modular by 
exploiting ClickNF's building blocks. In the conf folder we provide four examples to showcase ClickNF functionalities. 
The examples consists in:

- monolithic bulk transfer
- monolithic echo server
- modular echo server
- modular echo server with batching (configure with --enable-batch option )

All the examples below uses the test-tcp-layer2.click compound element that provide TCP stack to Click. Each element 
used in the configuration files has a number of parameters that can be used to change their behavior. Note that, to
run the experiments, you need to change at least IP and MAC addresses in the configurations provided. 

For some details about the usage of TCPEpollServer/Client building blocks used to develop modula apps, please read the modules' README in tcpepollserver.cc and tcpepollclient.cc .

Bulk transfer: 
------------------------

This example consists in transferring a big file from client to the server. 

First run the server with: 

sudo bin/click --dpdk -c0x1 -n10 -- conf/bulk-server.click

then run the client with:

sudo bin/click --dpdk -c0x1 -n10 -- conf/bulk-client.click

in which parameters inside --dpdk -- are dpdk parameters (-c0x1 COREMASK and -n10 MEMORY CHANNELS )

To modify the TCP flavor (NewReno, DCTCP, BBR) used by the bulk-server/bulk-client edit the TCPLayer CONGCTRL parameter found in the click file. Note that NewReno is the default congestion control methodology. 

To run bulk-server using TCPPrague: 

First run the server with:

sudo bin/click --dpdk -c0x1 -n10 -- conf/bulk-prague-server.click

then run the client with:

sudo bin/click --dpdk -c0x1 -n10 -- conf/bulk-prague-client.click  

Echo server (monolithic):
-------------------------

This example consists in a server listening for incoming connections. Clients connect to the server, send one or more messages 
(depending on the client parameters) and wait for echo reply. When the message is received, the client resets the connection and 
repeat the same operation until the test is finished (the client is configured to connect a maximum number of times). 

First run the server with:

sudo bin/click --dpdk -c0x3 -n10 -- conf/echo-server-monolithic.click

then run the client with:

sudo bin/click --dpdk -c0xf -n10 -- conf/echo-client.click

Echo server (modular):
------------------------

The test is similar to the previous one with the exception that the echo server is implemented using two elements instead of 
implementing everithing inside a single element. 

First run the server with:

sudo bin/click --dpdk -c0x3 -n10 -- conf/echo-server-modular.click

then run the client with:

sudo bin/click --dpdk -c0xf -n10 -- conf/echo-client.click

Echo server (modular with batching):
------------------------------------

The test is similar to the previous one with the exception, when possible, echo server's elements transmit packet 
batches instead of single packets. Note that the code need to be recompiled configuring with --enable-batch option.

First run the server with:

sudo bin/click --dpdk -c0x3 -n10 -- conf/echo-server-modular-batch.click

then run the client with:

sudo bin/click --dpdk -c0xf -n10 -- conf/echo-client.click
