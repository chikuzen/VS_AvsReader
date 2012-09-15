
CC = $(CROSS)gcc
LD = $(CROSS)gcc

CFLAGS = -Wall -std=gnu99 -Os -g0 -I. -march=i686 -msse -ffast-math -mfpmath=sse -fno-strict-aliasing -fexcess-precision=fast
DCFLAGS = -Wall -std=gnu99 -O0 -g3 -I.

LDFLAGS = -shared -Wl,--dll,--add-stdcall-alias -Wl,-s -L.
DLDFLAGS = -shared -Wl,--dll,--add-stdcall-alias -L.

.Phony: all debug clean

all: vsavsreader.dll

debug: vsavsreader_dbg.dll

vsavsreader.dll: vsavsreader.o
	$(LD) $(LDFLAGS) $(XLDFLAGS) -o $@ $^

vsavsreader_dbg.dll: vsavareader.d
	$(LD) $(DLDFLAGS) -o $@ $^

vsavsreader.o: vsavsreader.c avisynth_c.h VapourSynth.h
	$(CC) -c $(CFLAGS) $(XCFLAGS) -o $@ $<

vsavsreader.d: vsavsreader.c avisynth_c.h VapourSynth.h
	$(CC) -c $(DCFLAGS) $(XCFLAGS) -o $@ $<

clean:
	$(RM) *.o *.d *.dll