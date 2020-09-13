cls
@call config.cmd
g++ -o qtcpproxy qtcpproxy.cpp -I%KX_KDB_PATH%/c/c %KX_KDB_PATH%/w32/c.dll -lws2_32
