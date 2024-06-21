PLUGIN_PREFIX = rana
PLUGIN_VENDOR = wermi

SDK = $(wildcard vstsdk2.4/public.sdk/source/vst2.x/*.cpp)
SRC = $(wildcard *.cpp)
HEADERS = $(wildcard *.hpp)
BIN = $(addprefix bin/rana,$(SRC:.cpp=.dll))

CFLAGS = -shared -static-libgcc -static -Ofast \
         -Wall -Wno-narrowing -Wno-write-strings \
         -DPLUGIN_VENDOR=\""$(PLUGIN_VENDOR)"\" \

all: $(BIN)

freeverb.o: ../src/freeverb/freeverb.c
	gcc $^ -Ofast -c -o freeverb.o

bin/rana%.dll: %.cpp $(HEADERS) Makefile freeverb.o
	mkdir -p bin
	g++ $< $(SRC_AUX) freeverb.o -Ivstsdk2.4 -Ivstsdk2.4/public.sdk/source/vst2.x $(SDK) $(CFLAGS) -DPLUGIN_NAME=\""$(PLUGIN_PREFIX)$*"\" -o $@

clean:
	rm $(BIN) freeverb.o
