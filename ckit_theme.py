import os
import sys
import re
import struct
import configparser

from PIL import Image

from ckit import ckitcore
from ckit import ckit_misc


## @addtogroup theme テーマ機能
## @{


default_theme_name = "black"
theme_name = default_theme_name
ini = None


def getThemeAssetFullpath(filename):
    return os.path.join( ckit_misc.getAppExePath(), 'theme', theme_name, filename )

## テーマを設定する
#
def setTheme( name, default_color ):
    
    global theme_name
    global ini
    
    theme_name = name
    
    filename = getThemeAssetFullpath("theme.ini")

    ini = configparser.RawConfigParser()
    
    ini.add_section("COLOR")

    for k in default_color.keys():
        ini.set( "COLOR", k, str(default_color[k]) )
    
    ini.read(filename)

    if not ini.has_option("COLOR","bg") : ini.set( "COLOR", "bg", "(0,0,0)" )
    if not ini.has_option("COLOR","fg") : ini.set( "COLOR", "fg", "(255,255,255)" )

    if not ini.has_option("COLOR","caret0") : ini.set( "COLOR", "caret0", "(255,255,255)" )
    if not ini.has_option("COLOR","caret1") : ini.set( "COLOR", "caret1", "(255,0,0)" )

    if not ini.has_option("COLOR","bar_fg") : ini.set( "COLOR", "bar_fg", "(255,255,255)" )
    if not ini.has_option("COLOR","bar_error_fg") : ini.set( "COLOR", "bar_error_fg", "(255,130,130)" )

    if not ini.has_option("COLOR","select_bg") : ini.set( "COLOR", "select_bg", "(130,130,130)" )
    if not ini.has_option("COLOR","select_fg") : ini.set( "COLOR", "select_fg", "(255,255,255)" )


## テーマをデフォルトの設定にする
#
def setThemeDefault():
    setTheme( default_theme_name, {} )


## テーマカラーを取得する
#
def getColor( name, section="COLOR" ):
    try:
        s = ini.get( section, name )
    except:
        print( "color not found :", name )
        return (0,0,0)

    match_result = re.match( r"[ ]*\([ ]*([0-9]+)[ ]*,[ ]*([0-9]+)[ ]*,[ ]*([0-9]+)[ ]*\)[ ]*", s )
    if match_result:
        r = int(match_result.group(1))
        g = int(match_result.group(2))
        b = int(match_result.group(3))
        return (r,g,b)


## テーマ画像を取得する
#
def createThemeImage(filename):

    try:
        path = getThemeAssetFullpath(filename)
        pil_img = Image.open(path)
        pil_img = pil_img.convert( "RGBA" )
        return ckitcore.Image.fromString( pil_img.size, pil_img.tostring(), (0,0,0) )
    except:
        print( "ERROR : image file cannot be loaded.", filename )
        return ckitcore.Image.fromString( (1,1), struct.pack( "BBBB", 0x10, 0x10, 0x10, 0xff ) ) 


## テーマ画像を 3x3 に分割したものを取得する
#
def createThemeImage3x3(filename):

    try:
        path = getThemeAssetFullpath(filename)
        pil_img = Image.open(path)
        pil_img = pil_img.convert( "RGBA" )
    except IOError:
        print( "ERROR : image file cannot be loaded.", filename )
        img = ckitcore.Image.fromString( (1,1), struct.pack( "BBBB", 0x10, 0x10, 0x10, 0xff ) )
        img = [ [img,img,img], [img,img,img], [img,img,img] ]
        return img, 1, 1, 1, 1

    img = [ [None,None,None], [None,None,None], [None,None,None] ]

    frame_width = pil_img.size[0] // 3
    frame_height = pil_img.size[1] // 3
    center_width = pil_img.size[0]-frame_width*2
    center_height = pil_img.size[1]-frame_height*2

    def crop( y, x, rect ):
        crop_img = pil_img.crop( rect )
        img[y][x] = ckitcore.Image.fromString( crop_img.size, crop_img.tostring(), (0,0,0) )

    crop( 0, 0, ( 0,                        0,                          frame_width,                frame_height ) )
    crop( 0, 1, ( frame_width,              0,                          frame_width+center_width,   frame_height ) )
    crop( 0, 2, ( frame_width+center_width, 0,                          frame_width*2+center_width, frame_height ) )

    crop( 1, 0, ( 0,                        frame_height,               frame_width,                frame_height+center_height ) )
    crop( 1, 1, ( frame_width,              frame_height,               frame_width+center_width,   frame_height+center_height ) )
    crop( 1, 2, ( frame_width+center_width, frame_height,               frame_width*2+center_width, frame_height+center_height ) )

    crop( 2, 0, ( 0,                        frame_height+center_height, frame_width,                frame_height*2+center_height ) )
    crop( 2, 1, ( frame_width,              frame_height+center_height, frame_width+center_width,   frame_height*2+center_height ) )
    crop( 2, 2, ( frame_width+center_width, frame_height+center_height, frame_width*2+center_width, frame_height*2+center_height ) )

    return img, frame_width, frame_height, center_width, center_height


## テーマ画像表示クラス
#
class ThemePlane:

    def __init__( self, main_window, imgfile, priority=2 ):
        img = createThemeImage(imgfile)
        self.plane = ckitcore.ImagePlane( main_window, (0,0), (1,1), priority )
        self.plane.setImage(img)

    def destroy(self):
        self.plane.destroy()

    def setPosSize( self, x, y, width, height ):
        self.plane.setPosition( (x,y) )
        self.plane.setSize( (width,height) )

    def setPosSizeByChar( self, main_window, x, y, width, height ):

        client_rect = main_window.getClientRect()
        offset_x, offset_y = main_window.charToClient( 0, 0 )
        char_w, char_h = main_window.getCharSize()
        
        px1 = x * char_w + offset_x
        px2 = (x+width) * char_w + offset_x
        py1 = y * char_h + offset_y
        py2 = (y+height) * char_h + offset_y
        
        if x==0 : px1=0
        if y==0 : py1=0
        if x+width==main_window.width() : px2=client_rect[2]
        if y+height==main_window.height() : py2=client_rect[3]
        
        self.setPosSize( px1, py1, px2-px1, py2-py1 )

    def show( self, _show ):
        self.plane.show( _show )


## 3x3に分割したテーマ画像表示クラス
#
class ThemePlane3x3:

    def __init__( self, main_window, imgfile, priority=2 ):
    
        self.img, self.frame_width, self.frame_height, self.center_width, self.center_height = createThemeImage3x3(imgfile)
        self.plane = [ [None,None,None], [None,None,None], [None,None,None] ]
        for y in (0,1,2):
            for x in (0,1,2):
                self.plane[y][x] = ckitcore.ImagePlane( main_window, (0,0), (1,1), priority )
                self.plane[y][x].setImage(self.img[y][x])

    def destroy(self):
        self.plane[0][0].destroy()
        self.plane[0][1].destroy()
        self.plane[0][2].destroy()
        self.plane[1][0].destroy()
        self.plane[1][1].destroy()
        self.plane[1][2].destroy()
        self.plane[2][0].destroy()
        self.plane[2][1].destroy()
        self.plane[2][2].destroy()

    def setPosSize( self, x, y, width, height ):

        if width > self.frame_width*2+self.center_width:
            plane_frame_width = self.frame_width
        else:
            plane_frame_width = width//3
        plane_center_width = width-plane_frame_width*2

        if height > self.frame_height*2+self.center_height:
            plane_frame_height = self.frame_height
        else:
            plane_frame_height = height//3
        plane_center_height = height-plane_frame_height*2


        self.plane[0][0].setPosition( (x,y) )
        self.plane[0][0].setSize( (plane_frame_width,plane_frame_height) )

        self.plane[0][1].setPosition( (x+plane_frame_width,y) )
        self.plane[0][1].setSize( (plane_center_width,plane_frame_height) )

        self.plane[0][2].setPosition( (x+plane_frame_width+plane_center_width,y) )
        self.plane[0][2].setSize( (plane_frame_width,plane_frame_height) )


        self.plane[1][0].setPosition( (x,y+plane_frame_height) )
        self.plane[1][0].setSize( (plane_frame_width,plane_center_height) )

        self.plane[1][1].setPosition( (x+plane_frame_width,y+plane_frame_height) )
        self.plane[1][1].setSize( (plane_center_width,plane_center_height) )

        self.plane[1][2].setPosition( (x+plane_frame_width+plane_center_width,y+plane_frame_height) )
        self.plane[1][2].setSize( (plane_frame_width,plane_center_height) )


        self.plane[2][0].setPosition( (x,y+plane_frame_height+plane_center_height) )
        self.plane[2][0].setSize( (plane_frame_width,plane_frame_height) )

        self.plane[2][1].setPosition( (x+plane_frame_width,y+plane_frame_height+plane_center_height) )
        self.plane[2][1].setSize( (plane_center_width,plane_frame_height) )

        self.plane[2][2].setPosition( (x+plane_frame_width+plane_center_width,y+plane_frame_height+plane_center_height) )
        self.plane[2][2].setSize( (plane_frame_width,plane_frame_height) )

    def setPosSizeByChar( self, main_window, x, y, width, height ):

        client_rect = main_window.getClientRect()
        offset_x, offset_y = main_window.charToClient( 0, 0 )
        char_w, char_h = main_window.getCharSize()
        
        px1 = x * char_w + offset_x
        px2 = (x+width) * char_w + offset_x
        py1 = y * char_h + offset_y
        py2 = (y+height) * char_h + offset_y
        
        if x==0 : px1=0
        if y==0 : py1=0
        if x+width==main_window.width() : px2=client_rect[2]
        if y+height==main_window.height() : py2=client_rect[3]
        
        self.setPosSize( px1, py1, px2-px1, py2-py1 )

    def show( self, _show ):
        self.plane[0][0].show( _show )
        self.plane[0][1].show( _show )
        self.plane[0][2].show( _show )
        self.plane[1][0].show( _show )
        self.plane[1][1].show( _show )
        self.plane[1][2].show( _show )
        self.plane[2][0].show( _show )
        self.plane[2][1].show( _show )
        self.plane[2][2].show( _show )

## @} theme

