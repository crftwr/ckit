import os
import sys

sys.path[0:0] = [
    os.path.join( os.path.split(sys.argv[0])[0], '../..' ),
    ]

import ckit
from ckit.ckit_const import *

ckit.registerWindowClass( "CkitTest" )

ckit.setTheme( "black", {} )

class Test1( ckit.Window ):

    def __init__(self):
        
        ckit.Window.__init__(
            self,
            x=20, 
            y=10, 
            width=80, 
            height=32,
            title_bar = True,
            title = "Ckit Test",
            sysmenu=True,
            activate_handler = self.onActivate,
            size_handler = self.onSize,
            close_handler = self.onClose,
            keydown_handler = self.onKeyDown,
            keyup_handler = self.onKeyUp,
            char_handler = self.onChar,
            )

    def onActivate( self, active ):
        print( "onActivate", active )

    def onSize( self, width, height ):
        print( "onSize" )
        self.putString( 0, 0, 10, 1, ckit.Attribute( fg=(255,255,255), bg=(0,0,0) ), " %d, %d " % (width, height) )
        self.putString( 0, 1, 10, 1, ckit.Attribute( fg=(50,50,50), bg=(200,200,200) ), " 幅  高 " )

    def onClose(self):
        print( "onClose" )
        self.quit()

    def onKeyDown( self, vk, mod ):
        print( "onKeyDown", vk, mod )
        if vk==VK_RETURN:
            rect = self.getWindowRect()
            test2 = Test2( (rect[0]+rect[2])//2, (rect[1]+rect[3])//2, self.getHWND() )
            self.enable(False)
            test2.messageLoop()
            self.enable(True)
            self.activate()
            test2.destroy()
            
        elif vk==VK_D:
            dialog = ckit.Dialog( self, "DialogTest", items=[
                ckit.Dialog.Edit( "dialog_edit1", "Edit1", "default value", 40 ),
                ckit.Dialog.Edit( "dialog_edit2", "Edit2", "", 40 ),
                ckit.Dialog.CheckBox( "dialog_checkbox1", "CheckBox1", False ),
                ckit.Dialog.CheckBox( "dialog_checkbox2", "CheckBox2", True ),
                ckit.Dialog.Choice( "dialog_choice1", "Choice1", [ "Option1", "Option2", "Option3" ], 1 ),
                ckit.Dialog.Choice( "dialog_choice2", "Choice2", [ "選択１", "選択２", "選択３" ], 1 ),
            ])
            dialog.messageLoop()
            dialog.destroy()

    def onKeyUp( self, vk, mod ):
        print( "onKeyUp", vk, mod )

    def onChar( self, ch, mod ):
        print( "onChar", ch, mod )


class Test2( ckit.Window ):

    def __init__(self,x,y,parent_window):
        
        ckit.Window.__init__(
            self,
            x=x, 
            y=y, 
            width=80, 
            height=24,
            origin= ORIGIN_X_CENTER | ORIGIN_Y_CENTER,
            parent_window=parent_window,
            resizable = False,
            minimizebox = False,
            maximizebox = False,
            activate_handler = self.onActivate,
            close_handler = self.onClose,
            keydown_handler = self.onKeyDown,
            keyup_handler = self.onKeyUp,
            char_handler = self.onChar,
            )

    def onActivate( self, active ):
        print( "onActivate", active )

    def onClose(self):
        print( "onClose" )
        self.quit()

    def onKeyDown( self, vk, mod ):
        print( "onKeyDown", vk, mod )
        if vk==VK_ESCAPE:
            self.quit()

    def onKeyUp( self, vk, mod ):
        print( "onKeyUp", vk, mod )

    def onChar( self, ch, mod ):
        print( "onChar", ch, mod )


test1 = Test1()

print( "before messageLoop" )

test1.messageLoop()

print( "after messageLoop" )

