from ckit import ckitcore

class TextWindow( ckitcore.Window ):
    
    def __init__( self, **args ):

        try:
            args["caret0_color"] = args["cursor0_color"]
            del args["cursor0_color"]
        except KeyError:
            pass

        try:
            args["caret1_color"] = args["cursor1_color"]
            del args["cursor1_color"]
        except KeyError:
            pass

        try:
            args["caret"] = args["cursor"]
            del args["cursor"]
        except KeyError:
            pass

        try:
            parent_window = args["parent_window"]
        except KeyError:
            parent_window = None

        try:
            width = args["width"]
        except KeyError:
            width = 80

        try:
            height = args["height"]
        except KeyError:
            height = 24

        try:
            font_name = args["font_name"]
            del args["font_name"]
        except KeyError:
            font_name = ""
        
        try:
            font_size = args["font_size"]
            del args["font_size"]
        except KeyError:
            font_size = 12

        try:
            self.__border_size = args["border_size"]
        except KeyError:
            self.__border_size = 1

        try:
            self.__size_handler = args["size_handler"]
        except KeyError:
            self.__size_handler = None

        if parent_window and not font_name:
            font = parent_window.__text.getFont()
        else:
            font = ckitcore.Font( font_name, font_size )
        
        char_size = font.getCharSize()

        args["width"] = width * char_size[0] + self.__border_size * 2
        args["height"] = height * char_size[1] + self.__border_size * 2
        args["size_handler"] = self.__onSize
        args["sizing_handler"] = self.__onSizing
        
        ckitcore.Window.__init__( self, **args )
        
        self.__text = ckitcore.TextPlane( self, (0,0), (1,1), 0.0 )
        
        self.__text.setFont(font)
        
        self.__adjustTextPlane()

    def destroy(self):
        self.__text.destroy()
        ckitcore.Window.destroy(self)

    def __onSizing( self, size ):

        char_size = self.__text.getCharSize()
        
        width = size[0]
        height = size[1]
        
        width -= self.__border_size * 2
        width -= width % char_size[0]
        width += self.__border_size * 2

        height -= self.__border_size * 2
        height -= height % char_size[1]
        height += self.__border_size * 2

        size[0] = width
        size[1] = height

    def __onSize( self, width, height ):

        self.__adjustTextPlane()

        if self.__size_handler:
            char_size = self.__text.getCharSize()
            width = (width-self.__border_size*2) // char_size[0]
            height = (height-self.__border_size*2) // char_size[1]
            self.__size_handler( width, height )

    def __adjustTextPlane(self):
        client_size = self.getClientSize()
        char_size = self.__text.getCharSize()

        width = client_size[0] - self.__border_size * 2
        height = client_size[1] - self.__border_size * 2

        width -= width % char_size[0]
        height -= height % char_size[1]
        
        x = (client_size[0]-width) // 2
        y = (client_size[1]-height) // 2

        self.__text.setPosition( (x,y) )
        self.__text.setSize( ( width, height ) )

        self.__size_in_char = ( width // char_size[0], height // char_size[1] )

    def putString( self, x, y, width, height, attr, str, offset=0 ):
        return self.__text.putString( x, y, width, height, attr, str, offset )

    def getStringWidth( self, *args ):
        return self.__text.getStringWidth( *args )

    def getStringColumns( self, *args ):
        return self.__text.getStringColumns( *args )

    def getClientRect(self):
        return (0,0) + self.getClientSize()

    def getNormalSize(self):
        size = self.getNormalClientSize()
        char_size = self.__text.getCharSize()
        width = (size[0]-self.__border_size*2) // char_size[0]
        height = (size[1]-self.__border_size*2) // char_size[1]
        return ( width, height )

    def charToClient( self, *args ):
        return self.__text.charToClient( *args )

    def charToScreen( self, *args ):
        return self.__text.charToScreen( *args )

    def setCursorPos( self, *args ):
        return self.__text.setCaretPosition( *args )

    def setCaretPosition( self, *args ):
        return self.__text.setCaretPosition( *args )

    def setCursorColor( self, *args ):
        return self.setCaretColor( *args )

    def setPosSize( self, x, y, width, height, origin ):
        char_size = self.__text.getCharSize()
        width = width * char_size[0] + self.__border_size * 2
        height = height * char_size[1] + self.__border_size * 2
        return self.setPositionAndSize( x, y, width, height, origin )

    def width(self):
        return self.__size_in_char[0]

    def height(self):
        return self.__size_in_char[1]

    def setFont( self, name, size ):
        font = ckitcore.Font( name, size )
        self.__text.setFont( font )

    def getCharSize(self):
        return self.__text.getCharSize()

