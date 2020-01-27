from ckit import ckitcore

fonts = {}

def getStockedFont( name, size ):
    
    key = (name,size)
    if key in fonts:
        return fonts[key]
    else:
        font = ckitcore.Font( name, size )
        fonts[key] = font
        return font

