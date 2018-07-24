#include <w32api.h>
#define _WIN32_WINNT Windows7
#define WINVER Windows7
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
//#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#define KXVER 3
#include "k.h"

using namespace std;

const bool debug = false;

int kdbhandle = 0;
S command_tcpConnect;
S command_tcpSend;
S command_tcpClose;
S command_tcpListen;
S command_udpListen;
S command_udpSend;
bool kdbConnActive;

enum ProxySocketMode {PROXY_DATA, PROXY_LISTENING, PROXY_UDP};

vector<WSAPOLLFD> handlesForPoll;
vector<ProxySocketMode> handleMode;

template<class ... Ts>
string cat(Ts ... args) {
    ostringstream os;
    auto a = { (os << args, 0)... };
    if (sizeof(a) != sizeof(a)){}   //swallow the "unused variable" warning
    return os.str();
}

string niceWSAGetLastError() {
    int errcode = WSAGetLastError();
    char buffer[256];
    memset(buffer, 0, 256);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 255, 0);
    return cat("(", errcode,") ",buffer);
}

string k2str(K kobj) {
    return kobj->t == -11?string(kobj->s):string((char*)kC(kobj),kobj->n);
}

int k2int(K kobj) {
    return kobj->t == -6?kobj->i:kobj->j;
}

void socketDisconnected(int handle) {
    if (handle == kdbhandle) {
        kdbConnActive = false;
    } else {
        if (debug) cout << "handle disconnected: " << handle << endl;
        if (kdbConnActive) {
            k(-kdbhandle, (char*)".tcp.disconnect", ki(handle), K(0));
        }
        for (auto &rec : handlesForPoll) {
            if (rec.fd == handle) rec.fd = -1;
        }
    }
}

void placeHandle(unsigned handle, ProxySocketMode mode) {
    bool placed = false;
    for (size_t i=0; i<handlesForPoll.size(); ++i) {
        if (handlesForPoll[i].fd == -1) {
            handlesForPoll[i].fd = unsigned(handle);
            handleMode[i] = mode;
            placed = true;
            break;
        }
    }
    if (!placed) {
        handlesForPoll.push_back({unsigned(handle),POLLRDNORM,0});
        handleMode.push_back(mode);
    }
}

void processCommand(S cmd, K kobj) {
    if (cmd == command_tcpConnect) {
        if (kobj->n < 4) { cerr << ".tcp.connect: too few args" << endl; return; }
        K alias = kK(kobj)[1];
        K host = kK(kobj)[2];
        K port = kK(kobj)[3];
        if (! (alias->t == -11 || alias->t == 10)) {cerr << ".tcp.connect: wrong type for alias" << endl; return;}
        if (! (host->t == -11 || host->t == 10)) {cerr << ".tcp.connect: wrong type for host" << endl; return;}
        if (! (port->t == -6 || port->t == -7)) {cerr << ".tcp.connect: wrong type for port" << endl; return;}
        string aliasstr = k2str(alias);
        string hoststr = host->t == -11?string(host->s):string((char*)kC(host),host->n);
        hostent* hostResolved = gethostbyname(hoststr.c_str());
        if (hostResolved == 0) {
            string msg = niceWSAGetLastError();
            k(-kdbhandle, (char*)".tcp.connFailed", kp((char*)aliasstr.c_str()), kp((char*)msg.c_str()), K(0));
            //cerr << ".tcp.connect: " << msg << endl;
            return;
        }
        char *ip = inet_ntoa (*(struct in_addr *)*hostResolved->h_addr_list);
        int handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip);
        addr.sin_port = htons(port->t == -6?port->i:port->j);
        int connectResult = connect(handle, (sockaddr *)&addr, sizeof(addr));
        if (0 != connectResult) {
            string msg = niceWSAGetLastError();
            k(-kdbhandle, (char*)".tcp.connFailed", kp((char*)aliasstr.c_str()), kp((char*)msg.c_str()), K(0));
            closesocket(handle);
            //cerr << ".tcp.connect: " << niceWSAGetLastError() << endl;
        } else {
            k(-kdbhandle, (char*)".tcp.connSuccess", kp((char*)aliasstr.c_str()), ki(handle), K(0));
            if (debug) cout << ".tcp.connect: handle=" << handle << endl;
            placeHandle(handle, PROXY_DATA);
        }
    } else if (cmd == command_tcpListen) {
        if (kobj->n < 3) { cerr << ".tcp.listen: too few args" << endl; return; }
        K alias = kK(kobj)[1];
        K port = kK(kobj)[2];
        if (! (alias->t == -11 || alias->t == 10)) {cerr << ".tcp.listen: wrong type for alias" << endl; return;}
        if (! (port->t == -6 || port->t == -7)) {cerr << ".tcp.listen: wrong type for port" << endl; return;}
        string aliasstr = k2str(alias);
        int handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        int iOptval = 1;
        int iResult = setsockopt(handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                         (char *) &iOptval, sizeof (iOptval));
        if (iResult == SOCKET_ERROR) {
            cerr << "in setsockopt: " << niceWSAGetLastError() << endl;
        }
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(k2int(port));
        int bindResult = bind(handle, (sockaddr *)&addr, sizeof(addr));
        if (0 != bindResult) {
            string msg = niceWSAGetLastError();
            k(-kdbhandle, (char*)".tcp.listenFailed", kp((char*)aliasstr.c_str()), kp((char*)msg.c_str()), K(0));
            closesocket(handle);
            //cerr << ".tcp.listen: " << niceWSAGetLastError() << endl;
        } else {
            int listenResult = listen(handle, SOMAXCONN);
            if (0 != listenResult) {
                string msg = niceWSAGetLastError();
                k(-kdbhandle, (char*)".tcp.listenFailed", kp((char*)aliasstr.c_str()), kp((char*)msg.c_str()), K(0));
                closesocket(handle);
            } else {
                k(-kdbhandle, (char*)".tcp.listenSuccess", kp((char*)aliasstr.c_str()), ki(handle), K(0));
                if (debug) cout << ".tcp.listen: handle=" << handle << endl;
                placeHandle(handle, PROXY_LISTENING);
            }
        }
    } else if (cmd == command_tcpSend) {
        if (kobj->n < 3) { cerr << ".tcp.send: too few args" << endl; return; }
        K handlek = kK(kobj)[1];
        K data = kK(kobj)[2];
        if (! (handlek->t == -6 || handlek->t == -7)) {cerr << ".tcp.send: wrong type for handle" << endl; return;}
        if (data->t != 4) {cerr << ".tcp.send: wrong type for data" << endl; return;}
        int handle = k2int(handlek);
        int result = send(handle, (S)kG(data), data->n, 0);
        if (result < 0) {
            cerr << ".tcp.send: " << niceWSAGetLastError() << endl;
        }
    } else if (cmd == command_tcpClose) {
        if (kobj->n < 2) { cerr << ".tcp.close: too few args" << endl; return; }
        K handlek = kK(kobj)[1];
        if (! (handlek->t == -6 || handlek->t == -7)) {cerr << ".tcp.send: wrong type for handle" << endl; return;}
        int handle = k2int(handlek);
        closesocket(handle);
        if (debug) cout << ".tcp.close: handle closed " << handle << endl;
        socketDisconnected(handle);
    } else if (cmd == command_udpListen) {
        if (kobj->n < 3) { cerr << ".udp.listen: too few args" << endl; return; }
        K alias = kK(kobj)[1];
        K port = kK(kobj)[2];
        if (! (alias->t == -11 || alias->t == 10)) {cerr << ".udp.listen: wrong type for alias" << endl; return;}
        if (! (port->t == -6 || port->t == -7)) {cerr << ".udp.listen: wrong type for port" << endl; return;}
        string aliasstr = k2str(alias);
        int handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int iOptval = 1;
        int iResult = setsockopt(handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                         (char *) &iOptval, sizeof (iOptval));
        if (iResult == SOCKET_ERROR) {
            cerr << "in setsockopt: " << niceWSAGetLastError() << endl;
        }
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(k2int(port));
        int bindResult = bind(handle, (sockaddr *)&addr, sizeof(addr));
        if (0 != bindResult) {
            string msg = niceWSAGetLastError();
            k(-kdbhandle, (char*)".udp.listenFailed", kp((char*)aliasstr.c_str()), kp((char*)msg.c_str()), K(0));
            closesocket(handle);
            //cerr << ".udp.listen: " << niceWSAGetLastError() << endl;
        } else {
            k(-kdbhandle, (char*)".udp.listenSuccess", kp((char*)aliasstr.c_str()), ki(handle), K(0));
            if (debug) cout << ".udp.listen: handle=" << handle << endl;
            placeHandle(handle, PROXY_UDP);
        }
    } else if (cmd == command_udpSend) {
        if (kobj->n < 5) { cerr << ".tcp.send: too few args" << endl; return; }
        K handlek = kK(kobj)[1];
        K host = kK(kobj)[2];
        K port = kK(kobj)[3];
        K data = kK(kobj)[4];
        if (! (handlek->t == -6 || handlek->t == -7)) {cerr << ".udp.send: wrong type for handle" << endl; return;}
        if (! (host->t == -11 || host->t == 10)) {cerr << ".udp.send: wrong type for host" << endl; return;}
        if (! (port->t == -6 || port->t == -7)) {cerr << ".udp.send: wrong type for port" << endl; return;}
        if (data->t != 4) {cerr << ".send.send: wrong type for data" << endl; return;}
        int handle = k2int(handlek);
        string hoststr = host->t == -11?string(host->s):string((char*)kC(host),host->n);
        hostent* hostResolved = gethostbyname(hoststr.c_str());
        if (hostResolved == 0) {
            string msg = niceWSAGetLastError();
            k(-kdbhandle, (char*)".udp.sendFailed", kp((char*)hoststr.c_str()), kp((char*)msg.c_str()), K(0));
            return;
        }
        char *ip = inet_ntoa (*(struct in_addr *)*hostResolved->h_addr_list);
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip);
        addr.sin_port = htons(port->t == -6?port->i:port->j);

        int result = sendto(handle, (S)kG(data), data->n, 0, (sockaddr *)&addr, sizeof(addr));
        if (result < 0) {
            string msg = niceWSAGetLastError();
            cerr << ".udp.send: " << msg << endl;
            k(-kdbhandle, (char*)".udp.sendFailed", ki(handle), kp((char*)msg.c_str()), K(0));
        }
    } else {
        cerr << "unknown command: " << cmd << endl;
    }
}

void receiveMsg(int handle, ProxySocketMode mode) {
    if (handle == kdbhandle) {
        K r = k(kdbhandle, S(0));
        if (r == 0) {
            cerr << "read returned null" << endl;
            kdbConnActive = false;
            return;
        }
        else { 
            if (debug) cout << "type: " << int(r->t) << endl;
            if (r->t == 0) {
                if (debug) cout << "item count: " << r->n << endl;
                for (int i=0; i<r->n; ++i) {
                    K item = kK(r)[i];
                    if (debug) cout << "item " << i << ": " << item << endl;
                    if (debug) cout << " type: " << int(item->t) << endl;
                }
                if (0<r->n) {
                    K cmd = kK(r)[0];
                    if (cmd->t==-11) {
                        if (debug) cout << "cmd: " << cmd->s << endl;
                        processCommand(cmd->s,r);
                    } else {
                        cerr << "invalid type for command" << endl;
                    }
                }
            }
        }
        r0(r);
    } else {
        if (kdbConnActive) {
            switch(mode) {
            case PROXY_DATA:{
                const int bufferSize = 65536;
                char buffer[bufferSize];
                int res = recv(handle, buffer, bufferSize, 0);
                if (res < 0) {
                    cerr << ".tcp.receive: " << niceWSAGetLastError() << endl;
                } else {
                    K msg = ktn(4, res);
                    memcpy(kG(msg),buffer, res);
                    if (debug) cout << ".tcp.receive: handle " << handle << " length " << res << endl;
                    k(-kdbhandle, (char*)".tcp.receive", ki(handle), msg, K(0));
                }
                break;
            }
            case PROXY_LISTENING:{
                sockaddr_in addr;
                int addrsize = sizeof(addr);
                int acceptResult = accept(handle, (sockaddr*)&addr, &addrsize);
                if (INVALID_SOCKET == acceptResult) {
                    cerr << ".tcp.accept: " << niceWSAGetLastError() << endl;
                } else {
                    placeHandle(acceptResult, PROXY_DATA);
                    k(-kdbhandle, (char*)".tcp.clientConnect", ki(handle), ki(acceptResult), ki(ntohl(addr.sin_addr.s_addr)), K(0));
                }
                break;
            }
            case PROXY_UDP:{
                sockaddr_in addr;
                int addrsize = sizeof(addr);
                const int bufferSize = 65536;
                char buffer[bufferSize];
                int res = recvfrom(handle, buffer, bufferSize, 0, (sockaddr *) &addr, &addrsize);
                if (SOCKET_ERROR == res) {
                    cerr << ".udp.accept: " << niceWSAGetLastError() << endl;
                } else {
                    K msg = ktn(4, res);
                    memcpy(kG(msg),buffer, res);
                    if (debug) cout << ".udp.receive: handle " << handle << " length " << res << endl;
                    k(-kdbhandle, (char*)".udp.receive", ki(handle), ki(ntohl(addr.sin_addr.s_addr)), ki(ntohs(addr.sin_port)), msg, K(0));
                }
                break;
            }
            }
        }
    }
}

char username[] = "";

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "usage: $0 host port" << endl;
        return 1;
    }
    kdbhandle = khpu(argv[1], stoi(argv[2]), username);
    if (kdbhandle == -1) {
        cerr << "failed to connect" << endl;
        return 1;
    }
    kdbConnActive = true;
    handlesForPoll.push_back({unsigned(kdbhandle),POLLRDNORM,0});
    handleMode.push_back(PROXY_DATA);
    if (debug) cout << kdbhandle << " connected" << endl;
    command_tcpConnect = ss(S(".tcp.connect"));
    command_tcpSend = ss(S(".tcp.send"));
    command_tcpClose = ss(S(".tcp.close"));
    command_tcpListen = ss(S(".tcp.listen"));
    command_udpListen = ss(S(".udp.listen"));
    command_udpSend = ss(S(".udp.send"));
    K r = k(-kdbhandle,(char*)".tcp.proxy", ki(0), (K)0);
    if (debug) cout << int(r->t) << endl;
    while(kdbConnActive) {
        int ret;
        if (SOCKET_ERROR == (ret = WSAPoll(&handlesForPoll[0],
                                               handlesForPoll.size(),
                                               30000
                                               )))
            { cerr << "socket error" << endl; }
        if (debug) cout << "epoll finished" << endl;
        for (size_t i = 0; i<handlesForPoll.size(); ++i) {
            WSAPOLLFD item = handlesForPoll[i];
            if (handlesForPoll[i].fd != -1) {
                if (debug) cout << "epoll " << item.fd << ": " << item.revents << endl;
                if (item.revents & POLLRDNORM) {
                    receiveMsg(item.fd, handleMode[i]);
                }
                if (item.revents & POLLHUP) {
                    if (debug) cout << "handle " << item.fd << " has POLLHUP" << endl;
                    socketDisconnected(item.fd);
                }
            }
        }
    }
    kclose(kdbhandle);
    return 0;
}