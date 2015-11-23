import mmap
import re


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
            self.line_end_re_object = re.compile( b"\x0D\x00\x0A\x00|\x0D\x00|\x0A\x00" )
        elif self.encoding=="utf-16-be":
            self.line_end_re_object = re.compile( b"\x00\x0D\x00\x0A|\x00\x0D|\x00\x0A" )
        else:
            self.line_end_re_object = re.compile( b"\x0D\x0A|\x0D|\x0A" )

        self.line_pos = [ offset ]

    def getLine( self, lineno ):

        while len(self.line_pos) <= lineno:
            next_lineend_pos, next_lineend = self._findNextLineEnd( self.line_pos[-1] )
            if next_lineend_pos>=0:
                self.line_pos.append(next_lineend_pos+len(next_lineend))
            else:
                raise IndexError

        linestart_pos = self.line_pos[lineno]

        lineend_pos, next_lineend = self._findNextLineEnd( self.line_pos[lineno] )

        if lineend_pos>=0:
            return self.mm[ linestart_pos : lineend_pos+len(next_lineend) ]
        else:
            return self.mm[ linestart_pos : ]

    def _findNextLineEnd( self, start ):

        re_result = self.line_end_re_object.search( self.mm, start )

        if re_result:
            return re_result.span()[0], re_result.group(0)
        else:
            return -1, None

    # TODO: 1行1行切り出すのではなく、re.finditer で、大き目のチャンクに対して、
    # 一度に複数行に分割する処理をしたほうが速そう



def test_HugeBinaryFile():

    fd = open("ckitcore.pdb","rb")

    huge_bin = HugeBinaryFile( fd.fileno() )

    for i in range( 0, 512, 16 ):
        data = huge_bin.getBytes( i, 16 )
        for b in data:
            print( "%02x " % b, end="" )
        print("")

    fd.close()


def test_HugeTextFile():

    fd = open("tags","rb")

    huge_text = HugeTextFile( fd.fileno(), "utf-8n", 0 )

    i = 0
    while True:
        try:
            line = huge_text.getLine(i)
        except IndexError as e:
            break
        print( "%8d: " % (i+1), line)
        i+=1

    fd.close()


test_HugeTextFile()


