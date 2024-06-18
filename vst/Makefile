PLUGIN_PREFIX = rana
PLUGIN_VENDOR = wermi

SDK = $(wildcard vstsdk2.4/public.sdk/source/vst2.x/*.cpp)
SRC = $(wildcard *.cpp)
HEADERS = $(wildcard *.hpp)
BIN = $(SRC:.cpp=.dll)

CFLAGS = -shared -static-libgcc -static \
         -Wno-narrowing -Wno-write-strings \
         -DPLUGIN_VENDOR=\""$(PLUGIN_VENDOR)"\" \

all: $(BIN)

%.dll: %.cpp $(HEADERS) Makefile
	g++ $< -Ivstsdk2.4 -Ivstsdk2.4/public.sdk/source/vst2.x $(SDK) $(CFLAGS) -DPLUGIN_NAME=\""$(PLUGIN_PREFIX)$*"\" -o $@

clean:
	rm $(BIN)
