import os
import sys
import math
import struct

from PIL import Image

if os.name=="nt":
    import pyauto

from ckit.ckit_const import *

from ckit import ckit_textwindow
from ckit import ckit_theme
from ckit import ckit_misc
from ckit import ckitcore

## @addtogroup widget ウィジェット機能
## @{

# ----------------------------------------------------------------------------------------------------------------------------
#
# ----------------------------------------------------------------------------------------------------------------------------

## ウィジェットの基底クラス
#
#  各種ウィジェットの基本機能を提供するクラスです。
#
class Widget:

    def __init__( self, window, x, y, width, height ):
        self.window = window
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self.visible = True
        self.enable_cursor = False
        self.enable_ime = False
        self.ime_rect = None

    def show(self,visible):
        self.visible = visible

    def setPosSize( self, x, y, width, height ):
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self._updateImeRect()

    def enableCursor( self, enable ):
        if not self.enable_cursor and enable:
            self.window.enableIme( self.enable_ime )
        self.enable_cursor = enable
        self._updateImeRect()

    def enableIme( self, enable ):
        self.window.enableIme( self.enable_cursor )
        self.enable_ime = enable
        self._updateImeRect()

    def setImeRect( self, rect ):
        self.ime_rect = rect
        self._updateImeRect()
    
    def _updateImeRect(self):

        if self.enable_cursor and self.enable_ime:
            
            if self.ime_rect==None:
                left, top = self.window.charToClient( self.x, self.y )
                right, bottom = self.window.charToClient( self.x+self.width, self.y+self.height )
            else:
                left, top = self.window.charToClient( self.x+self.ime_rect[0], self.y+self.ime_rect[1] )
                right, bottom = self.window.charToClient( self.x+self.ime_rect[2], self.y+self.ime_rect[3] )
        
            rect = ( left, top, right, bottom )
            self.window.setImeRect(rect)
        

# ----------------------------------------------------------------------------------------------------------------------------
#
# ----------------------------------------------------------------------------------------------------------------------------

## ボタンウィジェット
#
class ButtonWidget(Widget):

    def __init__( self, window, x, y, width, height, message, handler=None ):

        Widget.__init__( self, window, x, y, width, height )

        self.message = message
        self.handler = handler

        self.paint()

    def onKeyDown( self, vk, mod ):

        #print( "onKeyDown", vk )

        if vk==VK_SPACE:
            self.onPush()

        elif vk==VK_RETURN:
            self.onPush()

        self.paint()

    def onPush(self):
        if self.handler:
            self.handler()

    def paint(self):

        if self.enable_cursor:
            attr = ckitcore.Attribute( fg=ckit_theme.getColor("select_fg"), bg=ckit_theme.getColor("select_bg") )
            self.window.setCursorPos( -1, -1 )
        else:
            attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg") )

        self.window.putString( self.x, self.y, self.width, 1, attr, self.message )


# ----------------------------------------------------------------------------------------------------------------------------
#
# ----------------------------------------------------------------------------------------------------------------------------

## チェックボックスウィジェット
#
class CheckBoxWidget(Widget):

    def __init__( self, window, x, y, width, height, message, check ):

        Widget.__init__( self, window, x, y, width, height )

        self.img0 = ckit_theme.createThemeImage('check0.png')
        self.img1 = ckit_theme.createThemeImage('check1.png')

        self.plane_size = self.window.getCharSize()
        self.plane_size = ( self.plane_size[0]*2-1, self.plane_size[1]-1 )

        self.plane = ckitcore.ImagePlane( self.window, (0,0), self.plane_size, 0 )
        self.plane.setImage(self.img0)

        self.message = message
        self.check = check

        self.paint()

    def destroy(self):
        if self.plane:
            self.plane.destroy()

    def onKeyDown( self, vk, mod ):

        #print( "onKeyDown", vk )

        if vk==VK_SPACE:
            self.check = not self.check
            self.paint()
            return True

    def paint(self):

        pos1 = self.window.charToClient( self.x, self.y )
        pos2 = self.window.charToClient( self.x+2, self.y+1 )

        self.plane.setPosition( ( (pos1[0]+pos2[0]-self.plane_size[0])//2, (pos1[1]+pos2[1]-self.plane_size[1])//2 ) )

        if self.check:
            self.plane.setImage(self.img1)
        else:
            self.plane.setImage(self.img0)

        if self.enable_cursor:
            attr = ckitcore.Attribute( fg=ckit_theme.getColor("select_fg"), bg=ckit_theme.getColor("select_bg") )
            self.window.setCursorPos( -1, -1 )
        else:
            attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg") )

        self.window.putString( self.x+3, self.y, self.width-3, 1, attr, self.message )

    def getValue(self):
        return self.check



# ----------------------------------------------------------------------------------------------------------------------------
#
# ----------------------------------------------------------------------------------------------------------------------------

## 択一ウィジェット
#
#  複数の選択肢から一つを選択するために使用するウィジェットです。
#
class ChoiceWidget(Widget):

    def __init__( self, window, x, y, width, height, message, item_list, initial_select ):

        Widget.__init__( self, window, x, y, width, height )

        self.message = message
        self.item_list = item_list
        self.select = initial_select

        self.paint()

    def onKeyDown( self, vk, mod ):

        #print( "onKeyDown", vk )

        if vk==VK_LEFT:
            self.select -= 1
            if self.select<0 : self.select=0
            self.paint()
            return True
        elif vk==VK_RIGHT:
            self.select += 1
            if self.select>len(self.item_list)-1 : self.select=len(self.item_list)-1
            self.paint()
            return True

    def paint(self):

        x = self.x

        message_width = self.window.getStringWidth(self.message)
        self.window.putString( x, self.y, message_width, 1, ckitcore.Attribute( fg=ckit_theme.getColor("fg") ), self.message )
        x += message_width + 2

        for i in range(len(self.item_list)):

            item = self.item_list[i]

            if self.select==i :
                if self.enable_cursor :
                    attr = ckitcore.Attribute( fg=ckit_theme.getColor("select_fg"), bg=ckit_theme.getColor("select_bg") )
                    self.window.setCursorPos( -1, -1 )
                else:
                    attr = ckitcore.Attribute( fg=ckit_theme.getColor("choice_fg"), bg=ckit_theme.getColor("choice_bg") )
            else:
                attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg") )

            item_width = self.window.getStringWidth(item)
            self.window.putString( x, self.y, item_width, 1, attr, item )
            x += item_width + 2

    def getValue(self):
        return self.select


# ----------------------------------------------------------------------------------------------------------------------------
#
# ----------------------------------------------------------------------------------------------------------------------------

## 色選択ウィジェット
#
class ColorWidget(Widget):

    color_table = ( (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0), (0,0,0) )

    def __init__( self, window, x, y, width, height, message, color ):

        Widget.__init__( self, window, x, y, width, height )

        self.frame_plane_size = self.window.getCharSize()
        self.frame_plane_size = ( self.frame_plane_size[0]*4-1, self.frame_plane_size[1]-1 )
        self.frame_plane = ckitcore.ImagePlane( self.window, (0,0), self.frame_plane_size, 1 )
        self.color_plane_size = ( self.frame_plane_size[0]-2, self.frame_plane_size[1]-2 )
        self.color_plane = ckitcore.ImagePlane( self.window, (0,0), self.color_plane_size, 0 )

        self.message = message
        self.color = color

        self.paint()

    def onKeyDown( self, vk, mod ):

        #print( "onKeyDown", vk )

        if vk==VK_SPACE or vk==VK_RETURN:
            result, color, ColorWidget.color_table = ckitcore.chooseColor( self.window.getHWND(), self.color, ColorWidget.color_table )
            if result:
                self.color = color
        self.paint()

    def paint(self):

        pos1 = self.window.charToClient( self.x+0, self.y )
        pos2 = self.window.charToClient( self.x+4, self.y+1 )

        self.frame_plane.setPosition( ( (pos1[0]+pos2[0]-self.color_plane_size[0])//2, (pos1[1]+pos2[1]-self.color_plane_size[1])//2 ) )
        bg_color = ckit_theme.getColor("bg")
        self.frame_plane.setImage( ckitcore.Image.fromString( (1,1), struct.pack( "BBBB", bg_color[0]^0xff, bg_color[1]^0xff, bg_color[2]^0xff, 0xff ) ) )

        self.color_plane.setPosition( ( (pos1[0]+pos2[0]-self.color_plane_size[0])//2+1, (pos1[1]+pos2[1]-self.color_plane_size[1])//2+1 ) )
        self.color_plane.setImage( ckitcore.Image.fromString( (1,1), struct.pack( "BBBB", self.color[0], self.color[1], self.color[2], 0xff ) ) )

        if self.enable_cursor:
            attr = ckitcore.Attribute( fg=ckit_theme.getColor("select_fg"), bg=ckit_theme.getColor("select_bg") )
            self.window.setCursorPos( -1, -1 )
        else:
            attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg") )

        self.window.putString( self.x+5, self.y, self.width-3, 1, attr, self.message )

    def getValue(self):
        return self.color


# ----------------------------------------------------------------------------------------------------------------------------
#
# ----------------------------------------------------------------------------------------------------------------------------

## ホットキー設定ウィジェット
#
class HotKeyWidget(Widget):

    def __init__( self, window, x, y, width, height, vk, mod ):

        Widget.__init__( self, window, x, y, width, height )

        self.vk = vk
        self.mod = mod

        self.paint()

    def onKeyDown( self, vk, mod ):

        #print( "onKeyDown", vk, mod )

        if vk==VK_LCONTROL or vk==VK_RCONTROL or vk==VK_CONTROL:
            mod &= ~MODKEY_CTRL
        elif vk==VK_LMENU or vk==VK_RMENU or vk==VK_MENU:
            mod &= ~MODKEY_ALT
        elif vk==VK_LSHIFT or vk==VK_RSHIFT or vk==VK_SHIFT:
            mod &= ~MODKEY_SHIFT
        elif vk==VK_LWIN or vk==VK_RWIN:
            mod &= ~MODKEY_WIN

        self.vk = vk
        self.mod = mod

        self.paint()

    def paint(self):

        if self.enable_cursor:
            attr = ckitcore.Attribute( fg=ckit_theme.getColor("select_fg"), bg=ckit_theme.getColor("select_bg") )
            self.window.setCursorPos( -1, -1 )
        else:
            attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg") )

        _vk_name_table = {

            VK_A : "A",
            VK_B : "B",
            VK_C : "C",
            VK_D : "D",
            VK_E : "E",
            VK_F : "F",
            VK_G : "G",
            VK_H : "H",
            VK_I : "I",
            VK_J : "J",
            VK_K : "K",
            VK_L : "L",
            VK_M : "M",
            VK_N : "N",
            VK_O : "O",
            VK_P : "P",
            VK_Q : "Q",
            VK_R : "R",
            VK_S : "S",
            VK_T : "T",
            VK_U : "U",
            VK_V : "V",
            VK_W : "W",
            VK_X : "X",
            VK_Y : "Y",
            VK_Z : "Z",

            VK_0 : "0",
            VK_1 : "1",
            VK_2 : "2",
            VK_3 : "3",
            VK_4 : "4",
            VK_5 : "5",
            VK_6 : "6",
            VK_7 : "7",
            VK_8 : "8",
            VK_9 : "9",

            VK_OEM_MINUS  : "Minus",
            VK_OEM_PLUS   : "Plus",
            VK_OEM_COMMA  : "Comma",
            VK_OEM_PERIOD : "Period",

            VK_NUMLOCK  : "NumLock",
            VK_DIVIDE   : "Divide",
            VK_MULTIPLY : "Multiply",
            VK_SUBTRACT : "Subtract",
            VK_ADD      : "Add",
            VK_DECIMAL  : "Decimal",

            VK_NUMPAD0 : "Num0",
            VK_NUMPAD1 : "Num1",
            VK_NUMPAD2 : "Num2",
            VK_NUMPAD3 : "Num3",
            VK_NUMPAD4 : "Num4",
            VK_NUMPAD5 : "Num5",
            VK_NUMPAD6 : "Num6",
            VK_NUMPAD7 : "Num7",
            VK_NUMPAD8 : "Num8",
            VK_NUMPAD9 : "Num9",
    
            VK_F1  : "F1",
            VK_F2  : "F2",
            VK_F3  : "F3",
            VK_F4  : "F4",
            VK_F5  : "F5",
            VK_F6  : "F6",
            VK_F7  : "F7",
            VK_F8  : "F8",
            VK_F9  : "F9",
            VK_F10 : "F10",
            VK_F11 : "F11",
            VK_F12 : "F12",

            VK_LEFT     : "Left",
            VK_RIGHT    : "Right",
            VK_UP       : "Up",
            VK_DOWN     : "Down",
            VK_SPACE    : "Space",
            VK_TAB      : "Tab",
            VK_BACK     : "Back",
            VK_RETURN   : "Return",
            VK_ESCAPE   : "Escape",
            VK_CAPITAL  : "CapsLock",
            VK_APPS     : "Apps",
    
            VK_INSERT   : "Insert",
            VK_DELETE   : "Delete",
            VK_HOME     : "Home",
            VK_END      : "End",
            VK_NEXT     : "PageDown",
            VK_PRIOR    : "PageUp",

            VK_MENU     : "Alt",
            VK_LMENU    : "LAlt",
            VK_RMENU    : "RAlt",
            VK_CONTROL  : "Ctrl",
            VK_LCONTROL : "LCtrl",
            VK_RCONTROL : "RCtrl",
            VK_SHIFT    : "Shift",
            VK_LSHIFT   : "LShift",
            VK_RSHIFT   : "RShift",
            VK_LWIN     : "LWin",
            VK_RWIN     : "RWin",

            VK_SNAPSHOT : "PrintScreen",
            VK_SCROLL   : "ScrollLock",
            VK_PAUSE    : "Pause",
        }

        str_mod = ""
        if self.mod & MODKEY_ALT:
            str_mod += "A-"
        if self.mod & MODKEY_CTRL:
            str_mod += "C-"
        if self.mod & MODKEY_SHIFT:
            str_mod += "S-"
        if self.mod & MODKEY_WIN:
            str_mod += "W-"

        try:
            str_vk = _vk_name_table[self.vk]
        except KeyError:
            str_vk = "(%d)" % self.vk

        s = str_mod + str_vk
        
        s = ckit_misc.adjustStringWidth( self.window, s, self.width, ckit_misc.ALIGN_CENTER )

        self.window.putString( self.x, self.y, self.width, 1, attr, s )

    def getValue(self):
        return [ self.vk, self.mod ]

# ----------------------------------------------------------------------------------------------------------------------------
#
# ----------------------------------------------------------------------------------------------------------------------------

class action_Insert:
    def __init__( self, pos, text ):
        #print( "action_Insert", pos, text )
        self.pos = pos
        self.text = text

class action_Delete:
    def __init__( self, pos, text ):
        #print( "action_Delete", pos, text )
        self.pos = pos
        self.text = text

class action_Back:
    def __init__( self, pos, text ):
        #print( "action_Back", pos, text )
        self.pos = pos
        self.text = text

class action_Replace:
    def __init__( self, pos, old, new ):
        #print( "action_Replace", pos, old, new )
        self.pos = pos
        self.old = old
        self.new = new

## テキスト編集ウィジェット
#
class EditWidget(Widget):

    candidate_list_min_width = 10
    candidate_list_min_height = 1
    candidate_list_max_width = 80
    candidate_list_max_height = 16

    class UpdateInfo:

        def __init__( self, text, selection ):
            self.text = text
            self.selection = selection

        def selectionLeft(self):
            return min( self.selection[0], self.selection[1] )

        def selectionRight(self):
            return max( self.selection[0], self.selection[1] )

    def __init__( self, window, x, y, width, height, text, selection=None, margin1=1, margin2=12, auto_complete=False, no_bg=False, bg_margin=2, autofix_list=None, update_handler=None, candidate_handler=None, candidate_remove_handler=None, word_break_handler=None ):

        Widget.__init__( self, window, x, y, width, height )

        self.enableIme(True)

        self.text = text
        if selection==None : selection=[ len(text), len(text) ]
        self.selection = selection
        if margin1 > self.width//2 : margin1 = self.width//2
        if margin2 > self.width//2 : margin2 = self.width//2
        self.margin1 = margin1
        self.margin2 = margin2
        self.auto_complete = auto_complete
        self.vk_complete = [ VK_SPACE ]
        self.scroll_pos = -self.margin1
        self.bg_margin = bg_margin
        self.autofix_list = autofix_list
        self.update_handler = update_handler
        self.candidate_handler = candidate_handler
        self.candidate_remove_handler = candidate_remove_handler
        if word_break_handler:
            self.word_break_handler = word_break_handler
        else:
            self.word_break_handler = ckit_misc.wordbreak_Filename

        self.undo_list = []
        self.redo_list = []
        self.last_action = None

        if not no_bg:
            self.plane_edit = ckit_theme.ThemePlane3x3( self.window, 'edit.png', 1 )
            self._updatePlanePosSize(x,y,width,height)
        else:
            self.plane_edit = None

        if candidate_handler:
            self.list_window = self.createCandidateWindow()
        else:
            self.list_window = None
        self.list_window_base_len = 0
        self.list_window_prepared_info = None
        self.list_window_possize_info = None

        self.makeVisible( self.selection[1] )

        self.paint()

    def destroy(self):
        if self.plane_edit:
            self.plane_edit.destroy()
        if self.list_window:
            self.list_window.destroy()

    def createCandidateWindow(self):
        return CandidateWindow( 0, 0, EditWidget.candidate_list_min_width, EditWidget.candidate_list_min_height, EditWidget.candidate_list_max_width, EditWidget.candidate_list_max_height, self.window, selchange_handler=self.onListSelChange )

    def _updatePlanePosSize( self, x, y, width, height ):
        if self.plane_edit:
            pos1 = self.window.charToClient(x,y)
            pos2 = self.window.charToClient(x+width,y+1)
            self.plane_edit.setPosSize( pos1[0]-self.bg_margin, pos1[1]-self.bg_margin, pos2[0]-pos1[0]+self.bg_margin*2, pos2[1]-pos1[1]+self.bg_margin*2 )

    def setPosSize( self, x, y, width, height ):
        Widget.setPosSize( self, x, y, width, height )
        self._updatePlanePosSize(x,y,width,height)

    def setAutoComplete( self, auto_complete ):
        self.auto_complete = auto_complete

    def getAutoComplete(self):
        return self.auto_complete

    def appendUndo( self, action ):
        if self.last_action != None :
            self.undo_list.append(self.last_action)
            self.redo_list = []
        self.last_action = action

    def undo(self):

        self.appendUndo(None)

        if len(self.undo_list)==0 : return

        action = self.undo_list[-1]

        if isinstance( action, action_Insert ):

            #print( "undo : insert", action.pos, action.text )

            update_info = EditWidget.UpdateInfo(
                self.text[ : action.pos ] + self.text[ action.pos + len(action.text) : ],
                [ action.pos, action.pos ]
                )

        elif isinstance( action, action_Delete ) or isinstance( action, action_Back ):

            #print( "undo : del/back", action.pos, action.text )

            update_info = EditWidget.UpdateInfo(
                self.text[ : action.pos ] + action.text + self.text[ action.pos : ],
                [ action.pos+len(action.text), action.pos+len(action.text) ]
                )

        elif isinstance( action, action_Replace ):

            #print( "undo : replace : ", action.pos, action.old, action.new )
        
            update_info = EditWidget.UpdateInfo(
                self.text[ : action.pos ] + action.old + self.text[ action.pos + len(action.new) : ],
                [ action.pos+len(action.old), action.pos+len(action.old) ]
                )

        else:
            assert(0)

        self.closeList()

        if self.update_handler:
            if self.update_handler(update_info)==False:
                return

        self.undo_list = self.undo_list[:-1]
        self.redo_list.append(action)

        self.text = update_info.text
        self.selection = update_info.selection
        self.makeVisible( self.selection[1] )
        self.paint()

    def redo(self):

        if len(self.redo_list)==0 : return

        action = self.redo_list[-1]

        if isinstance( action, action_Insert ):

            update_info = EditWidget.UpdateInfo(
                self.text[ : action.pos ] + action.text + self.text[ action.pos : ],
                [ action.pos + len(action.text), action.pos + len(action.text) ]
                )

        elif isinstance( action, action_Delete ) or isinstance( action, action_Back ):

            update_info = EditWidget.UpdateInfo(
                self.text[ : action.pos ] + self.text[ action.pos + len(action.text) : ],
                [ action.pos, action.pos ]
                )

        elif isinstance( action, action_Replace ):

            update_info = EditWidget.UpdateInfo(
                self.text[ : action.pos ] + action.new + self.text[ action.pos + len(action.old) : ],
                [ action.pos + len(action.new), action.pos + len(action.new) ]
                )

        else:
            assert(0)

        self.closeList()

        if self.update_handler:
            if self.update_handler(update_info)==False:
                return

        self.undo_list.append(action)
        self.redo_list = self.redo_list[:-1]

        self.text = update_info.text
        self.selection = update_info.selection
        self.makeVisible( self.selection[1] )
        self.paint()

    def onKeyDown( self, vk, mod ):

        #print( "onKeyDown", vk, mod )

        if mod==MODKEY_SHIFT:

            if vk in (self.vk_complete):
                if self.candidate_handler:
                    self.window.removeKeyMessage()
                    if self.list_window.numItems()==0:
                        self.insertText("")
                        return True
                    return self.list_window.onKeyDown( vk, mod )

            elif vk==VK_LEFT:
                self.closeList()
                self.appendUndo(None)
                self.selection[1] = max( self.selection[1]-1, 0 )
            elif vk==VK_RIGHT:
                self.closeList()
                self.appendUndo(None)
                self.selection[1] = min( self.selection[1]+1, len(self.text) )
            elif vk==VK_HOME:
                self.closeList()
                self.appendUndo(None)
                self.selection[1] = 0
            elif vk==VK_END:
                self.closeList()
                self.appendUndo(None)
                self.selection[1] = len(self.text)
            self.makeVisible( self.selection[1] )
            self.paint()

        elif mod==MODKEY_CTRL:

            if vk==VK_LEFT:
                self.closeList()
                self.appendUndo(None)
                new_cursor_pos = self.word_break_handler( self.text, self.selection[1], -1 )
                self.selection = [ new_cursor_pos, new_cursor_pos ]
                self.makeVisible( self.selection[1] )
                self.paint()

            elif vk==VK_RIGHT:
                self.closeList()
                self.appendUndo(None)
                new_cursor_pos = self.word_break_handler( self.text, self.selection[1], +1 )
                self.selection = [ new_cursor_pos, new_cursor_pos ]
                self.makeVisible( self.selection[1] )
                self.paint()

            elif vk==VK_BACK:
                if self.selection[0]==self.selection[1] and self.selection[0]>0:
                    selection_left = self.word_break_handler( self.text, self.selection[1], -1 )
                    selection_right = self.selection[1]
                    update_info = EditWidget.UpdateInfo(
                        self.text[ : selection_left ] + self.text[ selection_right : ],
                        [ selection_left, selection_left ]
                        )
                else:
                    selection_left = min( self.selection[0], self.selection[1] )
                    selection_right = max( self.selection[0], self.selection[1] )
                    update_info = EditWidget.UpdateInfo(
                        self.text[ : selection_left ] + self.text[ selection_right : ],
                        [ selection_left, selection_left ]
                        )

                if self.candidate_handler:
                    self.popupListPrepare( update_info, False )

                if self.update_handler:
                    if self.update_handler(update_info)==False:
                        return

                self.appendUndo( action_Delete( update_info.selection[0], self.text[ selection_left : selection_right ] ) )
                self.appendUndo(None)

                self.text = update_info.text
                self.selection = update_info.selection
                self.makeVisible( self.selection[1] )
                self.paint()

                if self.candidate_handler:
                    self.popupList()

            elif vk==VK_A:
                self.closeList()
                self.appendUndo(None)
                self.selection = [ 0, len(self.text) ]
                self.makeVisible( self.selection[1] )
                self.paint()

            elif vk==VK_X:
                self.appendUndo(None)
                if self.selection[0]==self.selection[1] : return

                selection_left = min( self.selection[0], self.selection[1] )
                selection_right = max( self.selection[0], self.selection[1] )

                update_info = EditWidget.UpdateInfo(
                    self.text[ : selection_left ] + self.text[ selection_right : ],
                    [ selection_left, selection_left ]
                    )

                self.closeList()

                if self.update_handler:
                    if self.update_handler(update_info)==False:
                        return

                self.appendUndo( action_Delete( selection_left, self.text[ selection_left : selection_right ] ) )
                self.appendUndo(None)

                ckit_misc.setClipboardText( self.text[ selection_left : selection_right ] )

                self.text = update_info.text
                self.selection = update_info.selection
                self.makeVisible( self.selection[1] )
                self.paint()

            elif vk==VK_C:
                self.appendUndo(None)
                if self.selection[0]==self.selection[1] : return

                selection_left = min( self.selection[0], self.selection[1] )
                selection_right = max( self.selection[0], self.selection[1] )

                update_info = EditWidget.UpdateInfo(
                    self.text,
                    [ self.selection[1], self.selection[1] ]
                    )

                self.closeList()

                if self.update_handler:
                    if self.update_handler(update_info)==False:
                        return

                ckit_misc.setClipboardText( self.text[ selection_left : selection_right ] )

                self.text = update_info.text
                self.selection = update_info.selection
                self.makeVisible( self.selection[1] )
                self.paint()

            elif vk==VK_V:
                self.appendUndo(None)
                selection_left = min( self.selection[0], self.selection[1] )
                selection_right = max( self.selection[0], self.selection[1] )

                clipboard_text = ckit_misc.getClipboardText()

                update_info = EditWidget.UpdateInfo(
                    self.text[ : selection_left ] + clipboard_text + self.text[ selection_right : ],
                    [ selection_left+len(clipboard_text), selection_left+len(clipboard_text) ]
                    )

                self.closeList()

                if self.update_handler:
                    if self.update_handler(update_info)==False:
                        return

                self.appendUndo( action_Insert( selection_left, clipboard_text ) )
                self.appendUndo(None)

                self.text = update_info.text
                self.selection = update_info.selection
                self.makeVisible( self.selection[1] )
                self.paint()

            elif vk==VK_Z:
                self.appendUndo(None)
                self.undo()

            elif vk==VK_Y:
                self.redo()
            
            elif vk==VK_H:
                self.backSpace()
                
            elif vk==VK_K:
                self.removeCandidate()

        elif mod==MODKEY_SHIFT|MODKEY_CTRL:

            if vk==VK_LEFT:
                self.closeList()
                self.appendUndo(None)
                self.selection[1] = self.word_break_handler( self.text, self.selection[1], -1 )
                self.makeVisible( self.selection[1] )
                self.paint()

            elif vk==VK_RIGHT:
                self.closeList()
                self.appendUndo(None)
                self.selection[1] = self.word_break_handler( self.text, self.selection[1], +1 )
                self.makeVisible( self.selection[1] )
                self.paint()

        elif mod==0:

            if vk in self.vk_complete or vk in (VK_UP,VK_DOWN,VK_PRIOR,VK_NEXT):
                if self.candidate_handler:
                    self.window.removeKeyMessage()
                    if self.list_window.numItems()==0:
                        self.insertText("")
                        return True
                    return self.list_window.onKeyDown( vk, mod )

            elif vk==VK_TAB:
                
                if self.candidate_handler:

                    self.window.removeKeyMessage()

                    update_info = EditWidget.UpdateInfo(
                        self.text,
                        self.selection
                        )

                    candidate_list, base_len = self.candidate_handler(update_info)
                    
                    if len(candidate_list)>0:

                        common_prefix = candidate_list[0].lower()
                        for item in candidate_list:
                            item_lower = item.lower()
                            while 1:
                                if item_lower.startswith(common_prefix): break
                                common_prefix = common_prefix[:-1]
                                if not common_prefix : break
                            if not common_prefix : break

                        # 大文字小文字を区別する
                        common_prefix_case = candidate_list[0][ : len(common_prefix) ]
                        
                        selection_left = min( self.selection[0], self.selection[1] )
                        selection_right = max( self.selection[0], self.selection[1] )
                        
                        undo_action = None
                        
                        if self.text[ base_len : base_len + len(common_prefix) ].lower() == common_prefix:
                        
                            # 入力文字列の中に確定部分が含まれているので、選択範囲を変更するだけ
                            new_selection_left = base_len + len(common_prefix)
                            update_info = EditWidget.UpdateInfo(
                                self.text,
                                [ new_selection_left, max(new_selection_left,selection_right) ]
                                )

                        else:
                        
                            # 入力文字列に確定部分が含まれていないので、挿入する
                            left = self.text[:base_len]
                            mid = common_prefix_case
                            right = self.text[selection_right:]
                            update_info = EditWidget.UpdateInfo(
                                left + mid + right,
                                [ len(left)+len(mid), len(left)+len(mid) ]
                                )
                            
                            undo_action = action_Replace( base_len, self.text[base_len:selection_right], common_prefix_case )    

                        if self.update_handler:
                            if self.update_handler(update_info)==False:
                                return
                                
                        if undo_action:
                            self.appendUndo(undo_action)

                        self.text = update_info.text
                        self.selection = update_info.selection

                        self.insertText("")

                        return True

            elif vk==VK_ESCAPE:
                if self.candidate_handler:
                    if self.list_window.numItems()>0:
                        self.closeList()
                        return True

            elif self.selection[0]==self.selection[1]:
                if vk==VK_LEFT:
                    self.closeList()
                    self.appendUndo(None)
                    new_cursor_pos = max( self.selection[1]-1, 0 )
                    self.selection = [ new_cursor_pos, new_cursor_pos ]
                elif vk==VK_RIGHT:
                    self.closeList()
                    self.appendUndo(None)
                    new_cursor_pos = min( self.selection[1]+1, len(self.text) )
                    self.selection = [ new_cursor_pos, new_cursor_pos ]
                elif vk==VK_HOME:
                    self.closeList()
                    self.appendUndo(None)
                    self.selection = [ 0, 0 ]
                elif vk==VK_END:
                    self.closeList()
                    self.appendUndo(None)
                    self.selection = [ len(self.text), len(self.text) ]
                elif vk==VK_DELETE:
                    update_info = EditWidget.UpdateInfo(
                        self.text[ : self.selection[0] ] + self.text[ self.selection[1]+1 : ],
                        [ self.selection[0], self.selection[0] ]
                        )

                    self.closeList()

                    if self.update_handler:
                        if self.update_handler(update_info)==False:
                            return

                    if isinstance( self.last_action, action_Delete ):
                        self.last_action.text += self.text[ self.selection[0] : self.selection[0]+1 ]
                    else:
                        self.appendUndo( action_Delete( self.selection[0], self.text[ self.selection[0] : self.selection[0]+1 ] ) )

                    self.text = update_info.text
                    self.selection = update_info.selection
                elif vk==VK_BACK:
                    self.backSpace()
                    return True
                self.makeVisible( self.selection[1] )
                self.paint()
            else:
                if vk==VK_LEFT:
                    self.closeList()
                    self.appendUndo(None)
                    new_cursor_pos = min( self.selection[0], self.selection[1] )
                    self.selection = [ new_cursor_pos, new_cursor_pos ]
                elif vk==VK_RIGHT:
                    self.closeList()
                    self.appendUndo(None)
                    new_cursor_pos = max( self.selection[0], self.selection[1] )
                    self.selection = [ new_cursor_pos, new_cursor_pos ]
                elif vk==VK_HOME:
                    self.closeList()
                    self.appendUndo(None)
                    self.selection = [ 0, 0 ]
                elif vk==VK_END:
                    self.closeList()
                    self.appendUndo(None)
                    self.selection = [ len(self.text), len(self.text) ]
                elif vk==VK_DELETE or vk==VK_BACK:
                    selection_left = min( self.selection[0], self.selection[1] )
                    selection_right = max( self.selection[0], self.selection[1] )
                    update_info = EditWidget.UpdateInfo(
                        self.text[ : selection_left ] + self.text[ selection_right : ],
                        [ selection_left, selection_left ]
                        )

                    self.closeList()

                    if self.update_handler:
                        if self.update_handler(update_info)==False:
                            return
                    self.appendUndo( action_Delete( selection_left, self.text[ selection_left : selection_right ] ) )
                    self.appendUndo(None)
                    self.text = update_info.text
                    self.selection = update_info.selection
                self.makeVisible( self.selection[1] )
                self.paint()

    def onChar( self, ch, mod ):

        #print( "onChar", ch )

        if self.autofix_list:
            for autofix_string in self.autofix_list:
                if chr(ch) in autofix_string:

                    selection_left = min( self.selection[0], self.selection[1] )
                    selection_right = max( self.selection[0], self.selection[1] )

                    min_pos = -1
                    for autofix_char in autofix_string:
                        pos = self.text.find( autofix_char, selection_left, selection_right )
                        if pos>=0:
                            if min_pos<0 or pos<min_pos : min_pos=pos

                    if min_pos>=0:
                        self.selection = [ min_pos, selection_right ]
                    else:
                        self.selection = [ selection_right, selection_right ]

        if ch<=0xff and ( ch<0x20 or ch==0x7f ):
            return
        else:
            self.insertText( chr(ch) )

    def onWindowActivate( self, active ):
        if self.list_window:
            self.list_window.paint()

    def onWindowMove( self ):
        self.closeList()

    def popupListPrepare( self, update_info=None, complete=True, expected=None ):

        if self.candidate_handler:

            if update_info==None:
                update_info = EditWidget.UpdateInfo(
                    self.text,
                    self.selection
                    )

            if expected==None:
                expected = EditWidget.UpdateInfo(
                    self.text,
                    self.selection
                    )

            selection_right = max( update_info.selection[0], update_info.selection[1] )

            candidate_list, base_len = self.candidate_handler(update_info)
            #print( self.scroll_pos, base_len )

            if complete:
                expected_candidate = expected.text[ base_len : max( expected.selection[0], expected.selection[1] ) ]
                try:
                    select = candidate_list.index(expected_candidate)
                except ValueError:
                    select = 0
            else:
                select = -1
            
            if complete and len(candidate_list)>0:

                left = update_info.text[:base_len]
                mid = candidate_list[select]
                right = update_info.text[selection_right:]

                update_info.text = left + mid + right
                update_info.selection = [ update_info.selection[0], len(left)+len(mid) ]
            
            self.list_window_prepared_info = [ candidate_list, base_len, select ]

    def popupList(self):

        if self.candidate_handler:

            candidate_list, base_len, select = self.list_window_prepared_info

            text = self.alternateText(self.text)
            x, y1 = self.window.charToScreen( self.x+self.window.getStringWidth(text[:base_len])-self.scroll_pos, self.y )
            x, y2 = self.window.charToScreen( self.x+self.window.getStringWidth(text[:base_len])-self.scroll_pos, self.y+1 )
            
            # ちらつき防止のため、新しい候補リストウインドウを作る
            new_list_window_possize_info = [ x, y1, y2, min( len(candidate_list), EditWidget.candidate_list_max_height ) ]
            if self.list_window_possize_info != new_list_window_possize_info:
                self.list_window_base_len = base_len
                old_list_window = self.list_window
                old_list_window.enable(False)
                self.list_window = self.createCandidateWindow()
                def delayedDestroy():
                    old_list_window.destroy()
                self.window.delayedCall( delayedDestroy, 50 )
                self.list_window_possize_info = new_list_window_possize_info

            self.list_window.setItems( x, y1, y2, candidate_list, select )
            
        self.list_window_prepared_info = None    

    def isListOpened(self):
        return self.list_window and self.list_window.numItems()!=0

    def closeList(self):
        if self.candidate_handler:
            x, y = self.window.charToScreen( 0, 0 )
            self.list_window_base_len = 0
            self.list_window_possize_info = None
            self.list_window.setItems( x, y, y, [], 0 )

    def onListSelChange( self, select, text ):
    
        selection_left = min( self.selection[0], self.selection[1] )
        selection_right = max( self.selection[0], self.selection[1] )

        left = self.text[:self.list_window_base_len]
        mid = text
        right = self.text[selection_right:]
        
        update_info = EditWidget.UpdateInfo(
            left + mid + right,
            [ selection_left, len(left)+len(mid) ]
            )

        if self.update_handler:
            if self.update_handler(update_info)==False:
                return
                
        old = self.text[self.list_window_base_len:selection_right]
        new = text
        if old != new:
            self.appendUndo( action_Replace( self.list_window_base_len, old, new ) )

        self.text = update_info.text
        self.selection = update_info.selection
        self.makeVisible( self.selection[1] )
        self.paint()

    def removeCandidate(self):
        if self.candidate_remove_handler:
            if not self.candidate_remove_handler( self.text ):
                return False
            if self.isListOpened():
                selection = self.list_window.getSelection()
                self.list_window.remove( selection )
            else:
                self.clear()
            return True
        return False

    def insertText( self, text ):
        selection_left = min( self.selection[0], self.selection[1] )
        selection_right = max( self.selection[0], self.selection[1] )

        new_cursor_pos = selection_left + len(text)
        update_info = EditWidget.UpdateInfo(
            self.text[ : selection_left ] + text + self.text[ selection_right : ],
            [ new_cursor_pos, new_cursor_pos ]
            )

        if self.candidate_handler:
            self.popupListPrepare( update_info, self.auto_complete )

        if self.update_handler:
            if self.update_handler(update_info)==False:
                return

        inserted_text = text + update_info.text[ update_info.selectionLeft() : update_info.selectionRight() ]

        if selection_left==selection_right:
            if isinstance( self.last_action, action_Insert ):
                self.last_action.text += inserted_text
            else:
                if inserted_text:
                    self.appendUndo( action_Insert( selection_left, inserted_text ) )
        else:
            old = self.text[selection_left:selection_right]
            new = inserted_text
            if old != new:
                self.appendUndo( action_Replace( selection_left, old, new ) )

        self.text = update_info.text
        self.selection = update_info.selection
        self.makeVisible( self.selection[1] )
        self.paint()

        if self.candidate_handler:
            self.popupList()

    def backSpace(self):
        remove_left = max( self.selection[1]-1, 0 )
        update_info = EditWidget.UpdateInfo(
            self.text[ : remove_left ] + self.text[ self.selection[1] : ],
            [ remove_left, remove_left ]
            )

        self.closeList()

        if self.update_handler:
            if self.update_handler(update_info)==False:
                return

        if isinstance( self.last_action, action_Back ):
            self.last_action.text = self.text[ remove_left : self.selection[1] ] + self.last_action.text
            self.last_action.pos -= 1
        else:
            self.appendUndo( action_Back( remove_left, self.text[ remove_left : self.selection[1] ] ) )

        self.text = update_info.text
        self.selection = update_info.selection
        self.makeVisible( self.selection[1] )
        self.paint()

    def selectAll(self):
        self.selection = [ 0, len(self.text) ]
        self.paint()

    def clear(self):
    
        update_info = EditWidget.UpdateInfo(
            "",
            [ 0, 0 ]
            )

        if self.update_handler:
            if self.update_handler(update_info)==False:
                return
                
        self.appendUndo( action_Delete( 0, self.text ) )
        self.appendUndo(None)

        self.text = update_info.text
        self.selection = update_info.selection
        self.makeVisible( self.selection[1] )
        self.paint()

    def makeVisible( self, pos ):

        def countStringWidthRight( text, pos, width ):
            while True:
                if pos<0 or pos>len(text)-1 :
                    width -= 1
                else:
                    width -= self.window.getStringWidth(text[pos])
                if width < 0 : break
                pos += 1
            return pos

        def countStringWidthLeft( text, pos, width ):
            while True:
                pos -= 1
                if pos<0 or pos>len(text)-1 :
                    width -= 1
                else:
                    width -= self.window.getStringWidth(text[pos])
                if width < 0 : break
            return pos + 1

        if pos < countStringWidthRight( self.text, self.scroll_pos+1, self.margin1-1 ) :
            # 左側にスクロールする
            self.scroll_pos = max( countStringWidthLeft( self.text, pos, self.margin2 ), -self.margin1 )
        elif pos > countStringWidthRight( self.text, self.scroll_pos, self.width-self.margin1 ) :
            # 右側にスクロールする
            pos1 = countStringWidthLeft( self.text, pos, self.width-self.margin2 )
            pos2 = countStringWidthLeft( self.text, len(self.text), self.width-self.margin1 )
            self.scroll_pos = min( pos1, pos2 )
        elif self.isScrolled() and len(self.text) < countStringWidthRight( self.text, self.scroll_pos, self.width-self.margin1 ) :
            # 右側の余白が大きくなりすぎないように左側にスクロールする
            pos1 = countStringWidthLeft( self.text, pos, self.width-self.margin2 )
            pos2 = countStringWidthLeft( self.text, len(self.text), self.width-self.margin1 )
            self.scroll_pos = min( pos1, pos2 )
            self.scroll_pos = max( self.scroll_pos, -self.margin1 )

        assert( self.scroll_pos >= -self.margin1 )

    def alternateText( self, text ):
        return text.replace( "\t", "　" )

    def paint(self):

        selection_left = min( self.selection[0], self.selection[1] )
        selection_right = max( self.selection[0], self.selection[1] )

        x = self.x
        width = self.width

        attribute_edit = ckitcore.Attribute( fg=ckit_theme.getColor("fg") )
        attribute_edit_selected = ckitcore.Attribute( fg=ckit_theme.getColor("select_fg"), bg=ckit_theme.getColor("select_bg"))

        self.window.putString( x, self.y, width, 1, attribute_edit, " " * width )

        text = self.alternateText(self.text)

        if self.scroll_pos < selection_left:

            if self.scroll_pos<0:
                left_string = text[ 0 : selection_left ]
                x += 1
                width -= 1
            else:
                left_string = text[ self.scroll_pos : selection_left ]

            self.window.putString( x, self.y, width, 1, attribute_edit, left_string )
            left_string_width = self.window.getStringWidth(left_string)
            x += left_string_width
            width -= left_string_width

        if selection_left < selection_right:

            if self.scroll_pos<selection_left:
                mid_string = text[ selection_left : selection_right ]
            else:
                mid_string = text[ self.scroll_pos : selection_right ]

            if self.enable_cursor:
                attr = attribute_edit_selected
            else:
                attr = attribute_edit

            self.window.putString( x, self.y, width, 1, attr, mid_string )
            mid_string_width = self.window.getStringWidth(mid_string)
            x += mid_string_width
            width -= mid_string_width

        if selection_right < self.scroll_pos + self.width:

            right_string = text[ selection_right : ]

            self.window.putString( x, self.y, width, 1, attribute_edit, right_string )
            right_string_width = self.window.getStringWidth(right_string)
            x += right_string_width
            width -= right_string_width

        if self.enable_cursor:
            if self.scroll_pos<0:
                left_string = text[ 0 : self.selection[1] ]
                offset = 1
            else:
                left_string = text[ self.scroll_pos : self.selection[1] ]
                offset = 0
            left_string_width = self.window.getStringWidth(left_string)
            self.window.setCursorPos( self.x + offset + left_string_width, self.y )

    def getText(self):
        return self.text

    def setText(self,text):
        self.text = text

    def getSelection(self):
        return self.selection

    def setSelection(self,selection):
        self.selection = selection

    def isScrolled(self):
        return self.scroll_pos != -self.margin1

class CandidateWindow( ckit_textwindow.TextWindow ):

    def __init__( self, x, y, min_width, min_height, max_width, max_height, parent_window, keydown_hook=None, selchange_handler=None ):
        
        bg = ckit_theme.getColor("bg")
        fg = ckit_theme.getColor("fg")

        frame_color = ( 
            (bg[0] + fg[0])//2,
            (bg[1] + fg[1])//2,
            (bg[2] + fg[2])//2,
        )  
        
        ckit_textwindow.TextWindow.__init__(
            self,
            x=x,
            y=y,
            width=5,
            height=5,
            origin= ORIGIN_X_LEFT | ORIGIN_Y_TOP,
            parent_window=parent_window,
            bg_color = ckit_theme.getColor("bg"),
            caret0_color = ckit_theme.getColor("caret0"),
            caret1_color = ckit_theme.getColor("caret1"),
            frame_color = frame_color,
            border_size = 0,
            transparency = 255, # チラつきを防ぐために WS_EX_LAYERED にする
            show = False,
            resizable = False,
            title_bar = False,
            minimizebox = False,
            maximizebox = False,
            activate_handler = self.onActivate,
            keydown_handler = self.onKeyDown,
            char_handler = self.onChar,
            ncpaint = True,
            )

        self.parent_window = parent_window

        self.min_width = min_width
        self.min_height = min_height
        self.max_width = max_width
        self.max_height = max_height

        self.keydown_hook = keydown_hook
        self.selchange_handler = selchange_handler

        self.setItems( x, y, y, [], 0 )

    def setItems( self, x, y1, y2, items, select=0 ):

        # ウインドウの枠を考慮して x を調整する
        char_x, char_y = self.charToScreen(0,0)
        window_rect = self.getWindowRect()
        x -= char_x - window_rect[0]

        max_item_width = 0
        for item in items:
            if isinstance(item,list) or isinstance(item,tuple):
                item = item[0]
            item_width = self.getStringWidth(item)
            if item_width>max_item_width:
                max_item_width=item_width

        window_width = max_item_width
        window_height = len(items)

        window_width = max(window_width,self.min_width)
        window_height = max(window_height,self.min_height)

        window_width = min(window_width,self.max_width)
        window_height = min(window_height,self.max_height)
        
        # 画面に収まらない場合は上方向に配置する
        y = y2
        monitor_info_list = pyauto.Window.getMonitorInfo()
        for monitor_info in monitor_info_list:
            if monitor_info[0][0] <= x < monitor_info[0][2] and monitor_info[0][1] <= y1 < monitor_info[0][3]:
                window_rect = self.getWindowRect()
                char_w, char_h = self.getCharSize()
                if y2 + (window_rect[3]-window_rect[1]) + (self.max_height-self.height())*char_h >= monitor_info[1][3]:
                    y = y1 - ((window_rect[3]-window_rect[1]) + (window_height-self.height())*char_h)
                break
        
        if not len(items):
            self.show( False, False )

        self.setPosSize(
            x=x,
            y=y,
            width=window_width,
            height=window_height,
            origin= ORIGIN_X_LEFT | ORIGIN_Y_TOP
            )

        self.items = items
        self.scroll_info = ckit_misc.ScrollInfo()
        self.select = select
        self.scroll_info.makeVisible( self.select, self.height() )

        self.paint()

        if len(items):
            self.show( True, False )

    def remove( self, index ):
        
        del self.items[index]
        
        if self.select > index:
            self.select -= 1
        
        if not len(self.items):
            self.select = -1
            self.show( False, False )
            return

        if self.select<0 : self.select=0
        if self.select>len(self.items)-1 : self.select=len(self.items)-1

        if self.selchange_handler:
            self.selchange_handler( self.select, self.items[self.select] )

        self.paint()

    def onActivate( self, active ):
        if active:
            self.parent_window.activate()

    def onKeyDown( self, vk, mod ):

        if self.keydown_hook:
            if self.keydown_hook( vk, mod ):
                return True

        if mod==MODKEY_SHIFT:
            if vk==VK_SPACE:
                if not len(self.items) : return True
                self.select -= 1
                if self.select<0 : self.select=len(self.items)-1
                self.scroll_info.makeVisible( self.select, self.height() )

                if self.selchange_handler:
                    self.selchange_handler( self.select, self.items[self.select] )

                self.paint()
                return True

        elif mod==0:
            if vk==VK_SPACE:
                if not len(self.items) : return True
                self.select += 1
                if self.select>len(self.items)-1 : self.select=0
                self.scroll_info.makeVisible( self.select, self.height() )

                if self.selchange_handler:
                    self.selchange_handler( self.select, self.items[self.select] )

                self.paint()
                return True

            elif vk==VK_UP:
                if not len(self.items) : return True
                if self.select>=0:
                    self.select -= 1
                    if self.select<0 : self.select=0
                else:
                    self.select=len(self.items)-1
                self.scroll_info.makeVisible( self.select, self.height() )

                if self.selchange_handler:
                    self.selchange_handler( self.select, self.items[self.select] )

                self.paint()
                return True

            elif vk==VK_DOWN:
                if not len(self.items) : return True
                self.select += 1
                if self.select>len(self.items)-1 : self.select=len(self.items)-1
                self.scroll_info.makeVisible( self.select, self.height() )

                if self.selchange_handler:
                    self.selchange_handler( self.select, self.items[self.select] )

                self.paint()
                return True

            elif vk==VK_PRIOR:
                if not len(self.items) : return True

                if self.select>=0:
                    if self.select>self.scroll_info.pos :
                        self.select = self.scroll_info.pos
                    else:
                        self.select -= self.height()
                        if self.select<0 : self.select=0
                else:
                    self.select=len(self.items)-1

                self.scroll_info.makeVisible( self.select, self.height() )

                if self.selchange_handler:
                    self.selchange_handler( self.select, self.items[self.select] )

                self.paint()
                return True

            elif vk==VK_NEXT:
                if not len(self.items) : return True
                if self.select<self.scroll_info.pos+self.height()-1:
                    self.select = self.scroll_info.pos+self.height()-1
                else:
                    self.select += self.height()
                if self.select>len(self.items)-1 : self.select=len(self.items)-1
                self.scroll_info.makeVisible( self.select, self.height() )

                if self.selchange_handler:
                    self.selchange_handler( self.select, self.items[self.select] )

                self.paint()
                return True

    def onChar( self, ch, mod ):
        pass

    def paint(self):

        x=0
        y=0
        width=self.width()
        height=self.height()

        attribute_normal = ckitcore.Attribute( fg=ckit_theme.getColor("fg") )
        attribute_candidate = ckitcore.Attribute( fg=ckit_theme.getColor("fg"), bg=ckit_theme.getColor("bg"))
        attribute_candidate_selected = ckitcore.Attribute( fg=ckit_theme.getColor("select_fg"), bg=ckit_theme.getColor("select_bg"))

        active = self.parent_window.isActive()
        
        for i in range(height):
            index = self.scroll_info.pos+i
            if index < len(self.items):

                item = self.items[index]
                if isinstance(item,list) or isinstance(item,tuple):
                    item = item[0]

                if active and self.select==index:
                    attr=attribute_candidate_selected
                else:
                    attr=attribute_candidate
                self.putString( x, y+i, width, 1, attr, " " * width )
                self.putString( x, y+i, width, 1, attr, item )
            else:
                self.putString( x, y+i, width, 1, attribute_normal, " " * width )

    def getSelection(self):
        return self.select

    def numItems(self):
        return len(self.items)


# ----------------------------------------------------------------------------------------------------------------------------
#
# ----------------------------------------------------------------------------------------------------------------------------

## 時刻編集ウィジェット
#
class TimeWidget(Widget):

    FOCUS_YEAR = 0
    FOCUS_MONTH = 1
    FOCUS_DAY = 2
    FOCUS_HOUR = 3
    FOCUS_MINUTE = 4
    FOCUS_SECOND = 5

    def __init__( self, window, x, y, timestamp, bg_margin=2 ):

        Widget.__init__( self, window, x, y, 22, 1 )

        pos1 = self.window.charToClient(x,y)
        pos2 = self.window.charToClient(x+22,y+1)
        self.plane_edit = ckit_theme.ThemePlane3x3( self.window, 'edit.png' )
        self.plane_edit.setPosSize( pos1[0]-bg_margin, pos1[1]-bg_margin, pos2[0]-pos1[0]+bg_margin*2, pos2[1]-pos1[1]+bg_margin*2 )

        self.year_edit   = EditWidget( window, x+1,  y, 4, 1, "%04d"%timestamp[0], None, 0, 1, no_bg=True, update_handler=self.onYearUpdate )
        self.month_edit  = EditWidget( window, x+6,  y, 2, 1, "%02d"%timestamp[1], None, 0, 1, no_bg=True, update_handler=self.onMonthUpdate )
        self.day_edit    = EditWidget( window, x+9,  y, 2, 1, "%02d"%timestamp[2], None, 0, 1, no_bg=True, update_handler=self.onDayUpdate )
        self.hour_edit   = EditWidget( window, x+13, y, 2, 1, "%02d"%timestamp[3], None, 0, 1, no_bg=True, update_handler=self.onHourUpdate )
        self.minute_edit = EditWidget( window, x+16, y, 2, 1, "%02d"%timestamp[4], None, 0, 1, no_bg=True, update_handler=self.onMinuteUpdate )
        self.second_edit = EditWidget( window, x+19, y, 2, 1, "%02d"%timestamp[5], None, 0, 1, no_bg=True, update_handler=self.onSecondUpdate )

        self.year_edit.enableIme(False)
        self.month_edit.enableIme(False)
        self.day_edit.enableIme(False)
        self.hour_edit.enableIme(False)
        self.minute_edit.enableIme(False)
        self.second_edit.enableIme(False)

        self.year_edit.selectAll()
        self.month_edit.selectAll()
        self.day_edit.selectAll()
        self.hour_edit.selectAll()
        self.minute_edit.selectAll()
        self.second_edit.selectAll()

        self.focus = TimeWidget.FOCUS_YEAR

        self.paint()

    def onYearUpdate( self, info ):
        if len(info.text.strip())==0 :
            i=0
        else:
            i = int(info.text)
        if i>9999 : return False
        info.text = "%04d"%i
        info.selection = [4,4]
        return True

    def onMonthUpdate( self, info ):
        if len(info.text.strip())==0 :
            i=0
        else:
            i = int(info.text)
        if i>12 : return False
        info.text = "%02d"%i
        info.selection = [2,2]
        return True

    def onDayUpdate( self, info ):
        if len(info.text.strip())==0 :
            i=0
        else:
            i = int(info.text)
        if i>31 : return False
        info.text = "%02d"%i
        info.selection = [2,2]
        return True

    def onHourUpdate( self, info ):
        if len(info.text.strip())==0 :
            i=0
        else:
            i = int(info.text)
        if i>23 : return False
        info.text = "%02d"%i
        info.selection = [2,2]
        return True

    def onMinuteUpdate( self, info ):
        if len(info.text.strip())==0 :
            i=0
        else:
            i = int(info.text)
        if i>59 : return False
        info.text = "%02d"%i
        info.selection = [2,2]
        return True

    def onSecondUpdate( self, info ):
        if len(info.text.strip())==0 :
            i=0
        else:
            i = int(info.text)
        if i>59 : return False
        info.text = "%02d"%i
        info.selection = [2,2]
        return True

    def onKeyDown( self, vk, mod ):

        #print( "onKeyDown", vk )

        if vk==VK_LEFT:
            if self.focus==TimeWidget.FOCUS_MONTH:
                self.focus=TimeWidget.FOCUS_YEAR
                self.year_edit.selectAll()
            elif self.focus==TimeWidget.FOCUS_DAY:
                self.focus=TimeWidget.FOCUS_MONTH
                self.month_edit.selectAll()
            elif self.focus==TimeWidget.FOCUS_HOUR:
                self.focus=TimeWidget.FOCUS_DAY
                self.hour_edit.selectAll()
            elif self.focus==TimeWidget.FOCUS_MINUTE:
                self.focus=TimeWidget.FOCUS_HOUR
                self.minute_edit.selectAll()
            elif self.focus==TimeWidget.FOCUS_SECOND:
                self.focus=TimeWidget.FOCUS_MINUTE
                self.second_edit.selectAll()
            self.paint()

        elif vk==VK_RIGHT:
            if self.focus==TimeWidget.FOCUS_YEAR:
                self.focus=TimeWidget.FOCUS_MONTH
                self.month_edit.selectAll()
            elif self.focus==TimeWidget.FOCUS_MONTH:
                self.focus=TimeWidget.FOCUS_DAY
                self.day_edit.selectAll()
            elif self.focus==TimeWidget.FOCUS_DAY:
                self.focus=TimeWidget.FOCUS_HOUR
                self.hour_edit.selectAll()
            elif self.focus==TimeWidget.FOCUS_HOUR:
                self.focus=TimeWidget.FOCUS_MINUTE
                self.minute_edit.selectAll()
            elif self.focus==TimeWidget.FOCUS_MINUTE:
                self.focus=TimeWidget.FOCUS_SECOND
                self.second_edit.selectAll()
            self.paint()

        else:
            if self.focus==TimeWidget.FOCUS_YEAR:
                self.year_edit.onKeyDown( vk, mod )
            elif self.focus==TimeWidget.FOCUS_MONTH:
                self.month_edit.onKeyDown( vk, mod )
            elif self.focus==TimeWidget.FOCUS_DAY:
                self.day_edit.onKeyDown( vk, mod )
            elif self.focus==TimeWidget.FOCUS_HOUR:
                self.hour_edit.onKeyDown( vk, mod )
            elif self.focus==TimeWidget.FOCUS_MINUTE:
                self.minute_edit.onKeyDown( vk, mod )
            elif self.focus==TimeWidget.FOCUS_SECOND:
                self.second_edit.onKeyDown( vk, mod )

    def onChar( self, ch, mod ):

        #print( "onChar", ch )

        if "0123456789\b".find(chr(ch)) >= 0:

            if self.focus==TimeWidget.FOCUS_YEAR:
                self.year_edit.onChar( ch, mod )
            elif self.focus==TimeWidget.FOCUS_MONTH:
                self.month_edit.onChar( ch, mod )
            elif self.focus==TimeWidget.FOCUS_DAY:
                self.day_edit.onChar( ch, mod )
            elif self.focus==TimeWidget.FOCUS_HOUR:
                self.hour_edit.onChar( ch, mod )
            elif self.focus==TimeWidget.FOCUS_MINUTE:
                self.minute_edit.onChar( ch, mod )
            elif self.focus==TimeWidget.FOCUS_SECOND:
                self.second_edit.onChar( ch, mod )

    def paint(self):

        attribute_edit = ckitcore.Attribute( fg=ckit_theme.getColor("fg") )

        self.window.putString( self.x, self.y, self.width, 1, attribute_edit, "     /  /      :  :   " )

        self.year_edit.enableCursor( self.enable_cursor and self.focus==TimeWidget.FOCUS_YEAR )
        self.year_edit.paint()

        self.month_edit.enableCursor( self.enable_cursor and self.focus==TimeWidget.FOCUS_MONTH )
        self.month_edit.paint()

        self.day_edit.enableCursor( self.enable_cursor and self.focus==TimeWidget.FOCUS_DAY )
        self.day_edit.paint()

        self.hour_edit.enableCursor( self.enable_cursor and self.focus==TimeWidget.FOCUS_HOUR )
        self.hour_edit.paint()

        self.minute_edit.enableCursor( self.enable_cursor and self.focus==TimeWidget.FOCUS_MINUTE )
        self.minute_edit.paint()

        self.second_edit.enableCursor( self.enable_cursor and self.focus==TimeWidget.FOCUS_SECOND )
        self.second_edit.paint()

    def getValue(self):
        return (
                int(self.year_edit.getText()),
                int(self.month_edit.getText()),
                int(self.day_edit.getText()),
                int(self.hour_edit.getText()),
                int(self.minute_edit.getText()),
                int(self.second_edit.getText())
            )


# ----------------------------------------------------------------------------------------------------------------------------
#
# ----------------------------------------------------------------------------------------------------------------------------

## プログレスバーウィジェット
#
class ProgressBarWidget(Widget):

    busy_anim_speed = 0.4

    def __init__( self, window, x, y, width, height ):

        Widget.__init__( self, window, x, y, width, height )

        self.img0 = ckit_theme.createThemeImage('progress0.png')
        self.img1 = ckit_theme.createThemeImage('progress1.png')

        self.frame_plane = ckitcore.ImagePlane( self.window, (0,0), (0,0), 1 )
        self.frame_plane.setImage(self.img0)
        self.bar_plane_list = [ ckitcore.ImagePlane( self.window, (0,0), (0,0), 0 ) ]
        self.bar_plane_list[0].setImage(self.img1)
        
        self.busy_mode = False
        self.busy_anim_phase = 0
        self.busy_anim_theta = 0
        self.busy_anim_chase = 0
        self.busy_anim_value = [ 0, 0 ]
        self.value = 0

        self.paint()

    def destroy(self):
        self.window.killTimer(self._onTimer)
        self.frame_plane.destroy()
        for bar_plane in self.bar_plane_list:
            bar_plane.destroy()

    def show(self,visible):
        Widget.show(self,visible)
        self.frame_plane.show(visible)
        for bar_plane in self.bar_plane_list:
            bar_plane.show(visible)

    def paint(self):

        pos1 = self.window.charToClient( self.x, self.y )
        pos2 = self.window.charToClient( self.x+self.width, self.y+self.height )
        margin_y = 2

        self.frame_plane.setPosition( ( pos1[0], pos1[1]+margin_y ) )
        self.frame_plane.setSize( ( pos2[0]-pos1[0], pos2[1]-pos1[1]-margin_y ) )

        if self.busy_mode:
            self.bar_plane_list[0].setPosition( ( pos1[0] + int((pos2[0]-pos1[0])*self.busy_anim_value[0]), pos1[1]+margin_y ) )
            self.bar_plane_list[0].setSize( ( int((pos2[0]-pos1[0])*(self.busy_anim_value[1]-self.busy_anim_value[0])), pos2[1]-pos1[1]-margin_y ) )
        elif isinstance(self.value,list):
            for i in range(len(self.value)):
                offset_y = ( pos2[1]-pos1[1]-margin_y ) * i // len(self.value)
                self.bar_plane_list[i].setPosition( ( pos1[0], pos1[1]+margin_y+offset_y ) )
                self.bar_plane_list[i].setSize( ( int((pos2[0]-pos1[0])*self.value[i]), (pos2[1]-pos1[1]-margin_y)//len(self.value) ) )
                
        else:
            self.bar_plane_list[0].setPosition( ( pos1[0], pos1[1]+margin_y ) )
            self.bar_plane_list[0].setSize( ( int((pos2[0]-pos1[0])*self.value), pos2[1]-pos1[1]-margin_y ) )

        attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg"))
        self.window.putString( self.x, self.y, self.width, 1, attr, " " * self.width )

    def setValue( self, value ):
    
        def rebuildBar(num):
        
            if len(self.bar_plane_list)==num:
                return

            for bar_plane in self.bar_plane_list:
                bar_plane.destroy()
            self.bar_plane_list = []    

            for i in range(num):
                bar_plane = ckitcore.ImagePlane( self.window, (0,0), (0,0), 0 )
                self.bar_plane_list.append(bar_plane)
                bar_plane.setImage(self.img1)

        if value!=None:

            self.busy_mode = False

            if isinstance(value,list) or isinstance(value,tuple):
            
                if isinstance(value,tuple):
                    value = list(value)

                rebuildBar(len(value))
                
                for i in range(len(value)):
                    value[i] = max( value[i], 0.0 )
                    value[i] = min( value[i], 1.0 )
                    
                if self.value != value:
                    self.value = value
                    self.paint()
            
            else:

                rebuildBar(1)

                value = max( value, 0.0 )
                value = min( value, 1.0 )
    
                if self.value != value:
                    self.value = value
                    self.paint()

        else:
            if not self.busy_mode:
                rebuildBar(1)
                self.busy_mode = True
                self.busy_anim_phase = 0
                self.busy_anim_theta = 0
                self.busy_anim_chase = 0
                self.busy_anim_value = [ 0, 0 ]
                self.window.setTimer( self._onTimer, 50 )

    def getValue(self):
        return self.value

    def _onTimer(self):
    
        if self.busy_anim_phase==0:
            self.busy_anim_theta += ProgressBarWidget.busy_anim_speed
            if self.busy_anim_theta > math.pi*0.5:
                self.busy_anim_phase = 1

        elif self.busy_anim_phase==1:
            self.busy_anim_theta += ProgressBarWidget.busy_anim_speed
            self.busy_anim_chase += ProgressBarWidget.busy_anim_speed
            if self.busy_anim_theta > math.pi:
                self.busy_anim_theta = math.pi
                self.busy_anim_phase = 2

        elif self.busy_anim_phase==2:
            self.busy_anim_chase += ProgressBarWidget.busy_anim_speed
            if self.busy_anim_chase > math.pi:
                self.busy_anim_chase = math.pi
                self.busy_anim_phase = 3

        elif self.busy_anim_phase==3:
            self.busy_anim_theta -= ProgressBarWidget.busy_anim_speed
            if self.busy_anim_theta < math.pi*0.5:
                self.busy_anim_phase = 4

        elif self.busy_anim_phase==4:
            self.busy_anim_theta -= ProgressBarWidget.busy_anim_speed
            self.busy_anim_chase -= ProgressBarWidget.busy_anim_speed
            if self.busy_anim_theta < 0:
                self.busy_anim_theta = 0
                self.busy_anim_phase = 5

        elif self.busy_anim_phase==5:
            self.busy_anim_chase -= ProgressBarWidget.busy_anim_speed
            if self.busy_anim_chase < 0:
                self.busy_anim_chase = 0
                self.busy_anim_phase = 0

        value1 = math.cos(self.busy_anim_theta) * 0.5 + 0.5
        value2 = math.cos(self.busy_anim_chase) * 0.5 + 0.5
        
        self.busy_anim_value = [ min(value1,value2), max(value1,value2) ]
            
        self.paint()

## @} widget

