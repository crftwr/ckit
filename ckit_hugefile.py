import mmap


class HugeFile:

    def __init__( self, fileno ):
        self.mm = mmap.mmap( fileno, 0, access=mmap.ACCESS_READ )

    def close(self):
        self.mm.close()


class HugeBinaryFile(HugeFile):

    def __init__( self, fileno ):
        HugeFile.__init__( self, fileno )

    def getBytes( self, offset, length ):
        return self.mm[ offset : offset+length ]


class HugeTextFile(HugeFile):

    def __init__( self, fileno, encoding, offset ):
        
        HugeFile.__init__( self, fileno )
        
        self.encoding = encoding
        
        if self.encoding=="utf-16-le":
            self.line_end_list = ( , b"\x0D\x00\x0A\x00", b"\x0D\x00", b"\x0A\x00" )
        elif self.encoding=="utf-16-be":
            self.line_end_list = ( b"\x00\x0D\x00\x0A", b"\x00\x0D", b"\x00\x0A" )
        else:
            self.line_end_list = ( b"\x0D\x0A", b"\x0D", b"\x0A" )

        self.line_pos = [ offset ]

    def getLine( self, lineno ):
        
        while len(self.line_pos) <= lineno:
            next_line_pos = self._findNextLine( self.line_pos[-1] )
            if next_line_pos>=0:
                self.line_pos.append(next_line_pos)
            else:
                raise IndexError
        
        linestart_pos = self.line_pos[lineno]
        
        lineend_pos, next_lineend = self._findNextLineEnd( self.line_pos[lineno] )
        if lineend_pos>=0:
            return self.mm[ linestart_pos : lineend_pos+len(next_lineend) ]
        else:
            return self.mm[ linestart_pos : ]
    
    def _findNextLineEnd( self, start ):
        
        next_lineend_pos = None
        next_lineend = None
        
        for line_end in self.line_end_list:
        
            pos = self.mm.find( line_end, start )
            
            if pos>=0 and ( next_lineend_pos==None or pos<next_lineend_pos ):
                next_lineend_pos = pos
                next_lineend = line_end
        
        if next_lineend_pos:
            return next_lineend_pos, next_lineend
        else:
            return -1, None


    def _findNextLine( self, start ):
        
        next_lineend_pos, next_lineend = self._findNextLineEnd( start )

        if next_lineend_pos>=0:
            return next_lineend_pos + len(next_lineend)
        else:
            return -1

