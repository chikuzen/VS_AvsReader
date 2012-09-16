vsavsreader.dll -- AviSynth Script Reader plugin for VapourSynth

 Author: Oka Motofumi (chikuzen.mo at gmail dot com)
-----------------------------------------------------------------

examples:


#preparation

>>> import vapoursynth as vs
>>> core = vs.Core()
>>> core.std.LoadPlugin('vsavsreader.dll')


#case 'Import'
>>> clip = core.avsr.Import('C:/foo/bar/script.avs')


#case 'Eval'

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


sourcecode:
https://github.com/chikuzen/VS_AvsReader
