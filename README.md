This project builds a DLL with some utility functions for decompressing some YKCMP-compressed Disgaea files and swizzling textures.

This was intended to be used from Python since Python is too slow at decompression stuff. You can add a main and use it as an exe instead if needed.

You can call these functions from Python like this:
```py
from pathlib import Path
from ctypes import *

decompDLL = CDLL(str(Path("utils.dll").resolve()))

def decompress(fd):
    class YKCMP_HDR(Structure):
        _pack_ = 1
        _fields_ = [
            ("magic",       c_char * 8),
            ("type",        c_uint32),
            ("compSize",    c_uint32),
            ("decompSize",  c_uint32),
        ]

    hdr = YKCMP_HDR.from_buffer(fd)
    fd = (c_uint8 * len(fd)).from_buffer(fd)
    new_fd = (c_uint8 * hdr.decompSize)()
    decompDLL.decompress(byref(fd), hdr.compSize, byref(new_fd), hdr.decompSize)
    return new_fd

def unswizzle(fd, hdr):
    new_fd = (c_uint8 * hdr.decompSize)()
    decompDLL.UnswizzleImage(byref(fd), byref(new_fd), 
                             hdr.width, hdr.height, 
                             1, # depth
                             hdr.mipmaps,
                             SURFACE_FORMATS[hdr.type], # an enum of formats defined in util.h
                             hdr.tile_spacing, hdr.block_height)
    return new_fd
```

Only YKCMP types 4 and 8/9 are supported for decompression currently.
