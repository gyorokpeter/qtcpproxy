# qtcpproxy

This is a small application that can open raw TCP connections for a q process to communicate through.

See tcpclient.q for example usage. The functions labelled as "API" can be called from your application, and the functions labelled as "callback" could be redefined to provide custom behavior. You can directly load this file and it will start a qtcpproxy with the correct parameters.

# build

Windows only: run b.cmd (requires k.h and c.dll from the Kx website)

# API

qtcpproxy connects to your q process and calls async functions on it. You also communicate with the proxy using async messages.

## async message
Send the following as async messages to qtcpproxy (tcpclient.q provides wrappers).
* (`.tcp.connect;alias;host;port): connect to the given host:port. The alias can be used to identify the connection in the success/failure callbacks.
* (`.tcp.listen;alias;port): listen on the given port. The alias can be used to identify the connection in the success/failure callbacks.
* (`.tcp.send;handle;msg): send message on socket
* (`.tcp.close;handle): close socket

## callback
* .tcp.proxy: called when the qtcpproxy starts up, useful for storing the connection handle
* .tcp.connFailed[alias;msg]: called when the connection fails, gets the alias from the .tcp.connect call and the error message
* .tcp.connSuccess[alias;handle]: called when a connection succeeds, gets the alias and the handle (the handle is inside qtcpproxy, NOT the q process)
* .tcp.listenFailed[alias;msg]: called when listening fails, gets the alias from the .tcp.connect call and the error message
* .tcp.listenSuccess[alias;handle]: called when listening succeeds, gets the alias and the handle (the handle is inside qtcpproxy, NOT the q process)
* .tcp.disconnect[handle]: called when a connection is disconnected
* .tcp.receive:[handle;msg]: called when socket receives a message
* .tcp.clientConnect:[listenHandle;handle;address]: called when a client connects to the listening server
