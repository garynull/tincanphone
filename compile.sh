#!/bin/bash

# Build the miniupnpc static library
mkdir -p obj
cd obj
gcc -c `ls ../src/miniupnpc/*.c` -DMINIUPNP_STATICLIB -DNDEBUG -Wall -s -O2
cd ..
ar rcs miniupnpc.a `ls obj/*.o` 

# Build tincanphone
mkdir -p bin
g++ -o bin/tincanphone `ls src/*.cpp` `ls src/Gtk/*.cpp` -Isrc/ miniupnpc.a `pkg-config --cflags --libs gtk+-3.0 opus portaudio-2.0` -DMINIUPNP_STATICLIB -DNDEBUG -Wall -fexceptions -s -O2

# Clean up
rm obj/*.o
rm miniupnpc.a
rmdir obj
