import os
import ctypes

class Migemo:

    DICTID_INVALID    = 0
    DICTID_MIGEMO     = 1
    DICTID_ROMA2HIRA  = 2
    DICTID_HIRA2KATA  = 3
    DICTID_HAN2ZEN    = 4
    DICTID_ZEN2HAN    = 5

    def __init__( self, dict_dirname ):
        self.dll = ctypes.WinDLL('migemo.dll')
        self.handle = self.dll.migemo_open(0)
        self.dll.migemo_load( self.handle, Migemo.DICTID_MIGEMO,    os.path.join(dict_dirname,"migemo-dict").encode("mbcs") )
        self.dll.migemo_load( self.handle, Migemo.DICTID_ROMA2HIRA, os.path.join(dict_dirname,"roma2hira.dat").encode("mbcs") )
        self.dll.migemo_load( self.handle, Migemo.DICTID_HIRA2KATA, os.path.join(dict_dirname,"hira2kata.dat").encode("mbcs") )
        self.dll.migemo_load( self.handle, Migemo.DICTID_HAN2ZEN,   os.path.join(dict_dirname,"han2zen.dat").encode("mbcs") )
        self.dll.migemo_load( self.handle, Migemo.DICTID_ZEN2HAN,   os.path.join(dict_dirname,"zen2han.dat").encode("mbcs") )

    def query( self, q ):
        p = self.dll.migemo_query( self.handle, q.encode("utf-8") )
        s = ctypes.c_char_p(p).value.decode("utf-8")
        self.dll.migemo_release( self.handle, p )
        return s

