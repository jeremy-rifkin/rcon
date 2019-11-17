REM python compile.py main.cpp build/build.exe -lws2_32 -ldnsapi -w
g++ -g main.cpp rcon.cpp dns.cpp -o build/rcon.exe -lws2_32 -ldnsapi -w
