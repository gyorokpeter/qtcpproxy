cls
@call config.cmd
@if not exist c.dll (
	echo copy c.dll from %KX_KDB_PATH%/w32/c.dll to .
	exit 1
)
g++ -o qtcpproxy qtcpproxy.cpp -I%KX_KDB_PATH%/c/c c.dll -lws2_32 -static
