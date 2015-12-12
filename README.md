Netcat
==============================================

Re-implementation of the Unix networking tool Netcat

To build the programs:
<code>
Make
</code>


Two binaries will be produced: ncP & ncTh

ncP is the implementation using the Unix poll() method for file descriptor multiplexing.
ncTh uses pthreads for multiplexing.


My version of Netcat has the following usage:
<code>
Usage: ./ncP (or ./ncTh) [-k] [-l] [-v] [-r]  [-p source_port] [-w timeout] [hostname] [port[s]]
     -k Forces nc to stay listening for another connection after its
        current connection is completed.
     -p source_port   Specifies the source port nc should use,
        subject to privilege restrictions and availability.
     -v Print more verbose output of what the program is doing to stdout.
     -w timeout  If a connection and stdin are idle for more than timeout
        seconds, then the connection is silently closed.  The -w flag has
        no effect on the -l option.
     -r This option can only be used with the -l option. When this option is
        specified the server is to work with up to 10 connections simultaneously.
        any data read from a network connection is resent to all the other connections
</code>

Normal Netcat (from the netcat-openbsd package) has the following usage:
<code>
usage: nc [-46DdhklnrStUuvzC] [-i interval] [-P proxy_username] [-p source_port]
          [-s source_ip_address] [-T ToS] [-w timeout] [-X proxy_protocol]
          [-x proxy_address[:port]] [hostname] [port[s]]
</code>


