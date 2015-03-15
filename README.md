# TcpBroadcastEchoServer
My first windows application: A simple TCP server broadcasting messages to all connected clients and servers using async IO (IOCP)

## Concept
A client is run with a list of available servers. It will connect to any (but only one) random server out of this list and any time the connection get's closed it will try all configured servers until it manages to connect to any.

A client can send messages to the connected server. A message currently has a 8 Byte header with 4 Byte for the message length and 4 Byte for a message number (mainly for debugging purposes). Messages received by a server this way are distributed to all clients and servers connected to this server (Broadcast). If a message is received coming from another server it will only be distributed to all clients connected. This way message loops are prevented.

If a server is running without the "--noecho" flag it will send messages back to the source of the message if it is a client. This can be used to run round trip time measurements.

## More detailed descriptions
Please see the wiki of this project: https://github.com/JonasKunze/TcpBroadcastEchoServer/wiki
