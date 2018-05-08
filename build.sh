nasm -felf64 src/decode.asm -o obj/decode.o     # hex decoder
nasm -felf64 src/encode.asm -o obj/encode.o     # hex encoder
# nasm -felf64 src/decode58.asm -o obj/decode58.o
g++ -std=c++14 -O3 -c src/main.cpp -o obj/main.o
g++ -std=c++14 -O3 obj/main.o obj/decode.o obj/encode.o -o codec_test
