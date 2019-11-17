#!/bin/bash

[ ! -d build ] && mkdir build

#python3 compile.py main.cpp build/rcon -w -lresolv
g++ -g main.cpp rcon.cpp dns.cpp -o build/rcon -w -lresolv
