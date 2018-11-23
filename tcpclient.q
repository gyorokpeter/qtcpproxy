system"chcp 1250"

if[0=system"p"; system"p 0W"];
.tcp.priv.path:"/"sv -1_"/"vs ssr[;"\\";"/"]first -3#value {};

//private callback
.tcp.proxy:{
    if[.z.w=0; '"client only"];
    .tcp.handle:neg .z.w;
    .tcp.proxyCB[.tcp.handle];
    };

//callback
.tcp.proxyCB:{[handle]
    -1".tcp.handle: ",string .z.w;
    };

//API
.tcp.connect:{[alias;host;port]
    .tcp.handle(`.tcp.connect;alias;host;port);
    .tcp.handle(::);
    };

//callback
.tcp.connFailed:{[alias;msg]
    -1".tcp.connFailed: ",alias," - ",msg;
    };

//callback
.tcp.connSuccess:{[alias;handle]
    -1".tcp.connSuccess: ",alias," - ",string handle;
    set[`$alias;handle];
    };

//API
.tcp.listen:{[alias;port]
    .tcp.handle(`.tcp.listen;alias;port);
    .tcp.handle(::);
    };

//callback
.tcp.listenFailed:{[alias;msg]
    -1".tcp.connFailed: ",alias," - ",msg;
    };

//callback
.tcp.listenSuccess:{[alias;handle]
    -1".tcp.listenSuccess: ",alias," - ",string handle;
    };

//callback
.tcp.disconnect:{[handle]
    -1".tcp.disconnect: ",string handle;
    };

//API
.tcp.send:{[handle;msg]
    .tcp.handle(`.tcp.send;handle;msg);
    .tcp.handle(::);
    };

//callback
.tcp.receive:{[handle;msg]
    cmsg:`char$msg;
    -1".tcp.receive: ",string[handle]," - ",cmsg;
    if[cmsg like "GET / HTTP/1.1*";
        .tcp.handle(`.tcp.send;handle;`byte$.h.hy[`txt]"nothing to see here");
    ];
    if[cmsg like "GET /favicon.ico HTTP/1.1*";
        .tcp.handle(`.tcp.send;handle;`byte$.h.hn["404 Not Found";`txt;"The requested object was not found on this server."]);
    ];
    };

//callback
.tcp.clientConnect:{[listenHandle;handle;address]
    -1".tcp.clientConnect: ph ",string[listenHandle]," h ",string[handle]," host ",string .Q.host address;
    };

//API
.tcp.close:{[handle]
    .tcp.handle(`.tcp.close;handle);
    .tcp.handle(::);
    };

//API
.tcp.start:{
    path:"/"sv -1_"/"vs ssr[;"\\";"/"]first -3#value .z.s;
    system"start ",path,"/qtcpproxy.exe localhost ",string system"p";
    };

//API
.tcp.exit:{hclose abs .tcp.handle};

//API
.udp.listen:{[alias;port]
    .tcp.handle(`.udp.listen;alias;port);
    .tcp.handle(::);
    };

//callback
.udp.listenFailed:{[alias;msg]
    -1".udp.connFailed: ",alias," - ",msg;
    };

//callback
.udp.listenSuccess:{[alias;handle]
    -1".udp.listenSuccess: ",alias," - ",string handle;
    };

//callback
.udp.receive:{[handle;host;port;msg]
    -1".udp.receive: h ",string[handle]," host ",("."sv string`int$0x00 vs host)," port ",string[port]," msg=",.Q.s1 msg;
    };

//API
.udp.send:{[handle;host;port;msg]
    .tcp.handle(`.udp.send;handle;host;port;msg);
    .tcp.handle(::);
    };

//callback
.udp.sendFailed:{[handle;msg]
    -1".udp.sendFailed: ",string[handle]," - ",msg;
    };

//.tcp.handle(`.tcp.connect;"indexHandle";"index.hu";80)
//.tcp.handle(`.tcp.send;indexHandle;`byte$"\r\n"sv("GET / HTTP/1.1";"Host: index.hu";"Connection: close";"";""))
//.tcp.handle(`.tcp.close;indexHandle)

//.tcp.handle(`.tcp.listen;"listener";9996)
