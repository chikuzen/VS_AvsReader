vsavsreader.dll -- AviSynth Script Reader plugin for VapourSynth

 Author: Oka Motofumi (chikuzen.mo at gmail dot com)
-----------------------------------------------------------------


requirement:
VapourSynth r9 or later
avisynth2.6alpha3 or later

sourcecode:
https://github.com/chikuzen/VS_AvsReader


******************************************************************************
 how to use:
******************************************************************************

# Preparation
>>> import vapoursynth as vs
>>> core = vs.Core()
>>> core.std.LoadPlugin('vsavsreader.dll')

# Case 'Import'
>>> clip = core.avsr.Import('C:/foo/bar/script.avs')

# Case 'Eval'
>>> clip = core.avsr.Eval('ColorBars(320, 240, "YV12")')
or
>>> lines = '''
... LoadPlugin("C:/foo/bar/RawSource.dll")
... v1 = RawSource("D:/fizz/buzz/video.y4m")
... v1.ConvertToYV24().Spline64Resize(1280, 720)
... v2 = AVISource("E:/herp/derp/video2.avi").ConvertToYV24()
... return v1 + v2
... '''
>>> clip = core.avsr.Eval(lines=lines)



****************************************************************************
 how to use(advanced):
****************************************************************************

VsAvsReader is able to convert Dither's interleaved MSB/LSB format into a
compatible Vapoursynth YUV4xxP9/10/16 format.
Dither's MSB/LSB must be interleaved, stacked format is not supported.
Only YUV planar formats are allowed.


example 1:
Use VsAvsReader's Import function to load external Avisynth script.

Vapoursynth script
# Core
>>> import vapoursynth as vs
>>> core = vs.Core(accept_lowercase=True)

# Import plugins
>>> core.std.LoadPlugin('C:/vsavsreader.dll')

# Use "Import" to load interleaved MSB/LSB Avisynth script.
>>> clip = core.avsr.Import('C:/script.avs', bitdepth=16)

External Avsiynth script being imported.

#script.avs
LoadPlugin("C:/plugins/DGDecode.dll")
LoadPlugin("C:/plugins/Dither.dll")
Import("C:/scripts/Dither.avsi")
MPEG2Source("D:/source.d2v")
Dither_convert_8_to_16()
Dither_resize16(1280, 720)
Dither_convey_yuv4xxp16_on_yvxx()


Example 2:
Use VsAvsReader's Eval function to create Avisynth script inside a Vapoursynth script.

Vapoursynth script

# Core
>>> import vapoursynth as vs
>>> core = vs.Core(accept_lowercase=True)

# Import plugins
>>> core.std.LoadPlugin('C:/vsavsreader.dll')

# Use "Eval" to load interleaved MSB/LSB Avisynth script.
>>> lines = '''
... LoadPlugin("C:/plugins/DGDecode.dll")
... LoadPlugin("C:/plugins/Dither.dll")
... Import("C:/scripts/Dither.avsi")
... MPEG2Source("D:/source.d2v")
... Dither_convert_8_to_16()
... Dither_resize16(1280, 720)
... Dither_convey_yuv4xxp16_on_yvxx()
... '''
>>> video = core.avsr.Eval(lines=lines, bitdepth=16)

