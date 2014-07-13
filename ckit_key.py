import os
import ctypes

import ckit
from ckit.ckit_const import *


class KeyEvent:

    vk_str_table = {}
    str_vk_table = {}

    vk_str_table_common = {

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

    vk_str_table_std = {
        VK_OEM_1 : "Semicolon",
        VK_OEM_2 : "Slash",
        VK_OEM_3 : "BackQuote",
        VK_OEM_4 : "OpenBracket",
        VK_OEM_5 : "BackSlash",
        VK_OEM_6 : "CloseBracket",
        VK_OEM_7 : "Quote",
    }

    vk_str_table_jpn = {
        VK_OEM_1    : "Colon",
        VK_OEM_2    : "Slash",
        VK_OEM_3    : "Atmark",
        VK_OEM_4    : "OpenBracket",
        VK_OEM_5    : "Yen",
        VK_OEM_6    : "CloseBracket",
        VK_OEM_7    : "Caret",
        VK_OEM_102  : "BackSlash",
    }

    str_vk_table_common = {

        "A" : VK_A,
        "B" : VK_B,
        "C" : VK_C,
        "D" : VK_D,
        "E" : VK_E,
        "F" : VK_F,
        "G" : VK_G,
        "H" : VK_H,
        "I" : VK_I,
        "J" : VK_J,
        "K" : VK_K,
        "L" : VK_L,
        "M" : VK_M,
        "N" : VK_N,
        "O" : VK_O,
        "P" : VK_P,
        "Q" : VK_Q,
        "R" : VK_R,
        "S" : VK_S,
        "T" : VK_T,
        "U" : VK_U,
        "V" : VK_V,
        "W" : VK_W,
        "X" : VK_X,
        "Y" : VK_Y,
        "Z" : VK_Z,

        "0" : VK_0,
        "1" : VK_1,
        "2" : VK_2,
        "3" : VK_3,
        "4" : VK_4,
        "5" : VK_5,
        "6" : VK_6,
        "7" : VK_7,
        "8" : VK_8,
        "9" : VK_9,

        "MINUS"  : VK_OEM_MINUS,
        "PLUS"   : VK_OEM_PLUS,
        "COMMA"  : VK_OEM_COMMA,
        "PERIOD" : VK_OEM_PERIOD,

        "NUMLOCK"  : VK_NUMLOCK,
        "DIVIDE"   : VK_DIVIDE,
        "MULTIPLY" : VK_MULTIPLY,
        "SUBTRACT" : VK_SUBTRACT,
        "ADD"      : VK_ADD,
        "DECIMAL"  : VK_DECIMAL,

        "NUM0" : VK_NUMPAD0,
        "NUM1" : VK_NUMPAD1,
        "NUM2" : VK_NUMPAD2,
        "NUM3" : VK_NUMPAD3,
        "NUM4" : VK_NUMPAD4,
        "NUM5" : VK_NUMPAD5,
        "NUM6" : VK_NUMPAD6,
        "NUM7" : VK_NUMPAD7,
        "NUM8" : VK_NUMPAD8,
        "NUM9" : VK_NUMPAD9,

        "F1"  : VK_F1,
        "F2"  : VK_F2,
        "F3"  : VK_F3,
        "F4"  : VK_F4,
        "F5"  : VK_F5,
        "F6"  : VK_F6,
        "F7"  : VK_F7,
        "F8"  : VK_F8,
        "F9"  : VK_F9,
        "F10" : VK_F10,
        "F11" : VK_F11,
        "F12" : VK_F12,

        "LEFT"     : VK_LEFT  ,
        "RIGHT"    : VK_RIGHT ,
        "UP"       : VK_UP    ,
        "DOWN"     : VK_DOWN  ,
        "SPACE"    : VK_SPACE ,
        "TAB"      : VK_TAB   ,
        "BACK"     : VK_BACK  ,
        "RETURN"   : VK_RETURN,
        "ENTER"    : VK_RETURN,
        "ESCAPE"   : VK_ESCAPE,
        "ESC"      : VK_ESCAPE,
        "CAPSLOCK" : VK_CAPITAL,
        "CAPS"     : VK_CAPITAL,
        "CAPITAL"  : VK_CAPITAL,
        "APPS"     : VK_APPS,

        "INSERT"   : VK_INSERT,
        "DELETE"   : VK_DELETE,
        "HOME"     : VK_HOME,
        "END"      : VK_END,
        "PAGEDOWN" : VK_NEXT,
        "PAGEUP"   : VK_PRIOR,

        "ALT"  : VK_MENU ,
        "LALT" : VK_LMENU,
        "RALT" : VK_RMENU,
        "CTRL"  : VK_CONTROL ,
        "LCTRL" : VK_LCONTROL,
        "RCTRL" : VK_RCONTROL,
        "SHIFT"  : VK_SHIFT ,
        "LSHIFT" : VK_LSHIFT,
        "RSHIFT" : VK_RSHIFT,
        "LWIN" : VK_LWIN,
        "RWIN" : VK_RWIN,

        "PRINTSCREEN" : VK_SNAPSHOT,
        "SCROLLLOCK"  : VK_SCROLL,
        "PAUSE"       : VK_PAUSE,
    }

    str_vk_table_std = {

        "SEMICOLON"     : VK_OEM_1,
        "COLON"         : VK_OEM_1,
        "SLASH"         : VK_OEM_2,
        "BACKQUOTE"     : VK_OEM_3,
        "TILDE"         : VK_OEM_3,
        "OPENBRACKET"   : VK_OEM_4,
        "BACKSLASH"     : VK_OEM_5,
        "YEN"           : VK_OEM_5,
        "CLOSEBRACKET"  : VK_OEM_6,
        "QUOTE"         : VK_OEM_7,
        "DOUBLEQUOTE"   : VK_OEM_7,
        "UNDERSCORE"    : VK_OEM_MINUS,
        "ASTERISK"      : VK_8,
        "ATMARK"        : VK_2,
        "CARET"         : VK_6,
    }

    str_vk_table_jpn = {
        
        "SEMICOLON"     : VK_OEM_PLUS,
        "COLON"         : VK_OEM_1,
        "SLASH"         : VK_OEM_2,
        "BACKQUOTE"     : VK_OEM_3,
        "TILDE"         : VK_OEM_7,
        "OPENBRACKET"   : VK_OEM_4,
        "BACKSLASH"     : VK_OEM_102,
        "YEN"           : VK_OEM_5,
        "CLOSEBRACKET"  : VK_OEM_6,
        "QUOTE"         : VK_7,
        "DOUBLEQUOTE"   : VK_2,
        "UNDERSCORE"    : VK_OEM_102,
        "ASTERISK"      : VK_OEM_1,
        "ATMARK"        : VK_OEM_3,
        "CARET"         : VK_OEM_7,
    }

    str_mod_table = {

        "ALT"   :  MODKEY_ALT,
        "CTRL"  :  MODKEY_CTRL,
        "SHIFT" :  MODKEY_SHIFT,
        "WIN"   :  MODKEY_WIN,

        "A" :  MODKEY_ALT,
        "C" :  MODKEY_CTRL,
        "S" :  MODKEY_SHIFT,
        "W" :  MODKEY_WIN,
    }

    def __init__( self, vk, mod, extra=None ):
        self.vk = vk
        self.mod = mod
        self.extra = extra

    def __str__(self):
        s = ""
        if self.mod & MODKEY_ALT : s += "A-"
        if self.mod & MODKEY_CTRL : s += "C-"
        if self.mod & MODKEY_SHIFT : s += "S-"
        if self.mod & MODKEY_WIN : s += "W-"
        s += KeyEvent.vkToStr(self.vk)
        return s

    def __hash__(self):
        return self.vk

    def __eq__(self,rhs):
        if self.vk!=rhs.vk : return False
        if self.mod!=rhs.mod : return False
        if self.extra!=None and rhs.extra!=None and self.extra!=rhs.extra : return False
        return True

    @staticmethod
    def fromString(s):

        s = s.upper()

        vk = None
        mod=0

        token_list = s.split("-")

        for token in token_list[:-1]:
            mod |= KeyEvent.strToMod(token.strip())

        token = token_list[-1].strip()

        vk = KeyEvent.strToVk(token)

        return KeyEvent(vk,mod)

    @staticmethod
    def initTables():
        
        if os.name=="nt":
            keyboard_type = ctypes.windll.user32.GetKeyboardType(0)
        else:
            # FIXME : 実装
            keyboard_type = 4
        
        KeyEvent.str_vk_table = KeyEvent.str_vk_table_common
        KeyEvent.vk_str_table = KeyEvent.vk_str_table_common
        
        if keyboard_type==7:
            KeyEvent.str_vk_table.update(KeyEvent.str_vk_table_jpn)
            KeyEvent.vk_str_table.update(KeyEvent.vk_str_table_jpn)
        else:
            KeyEvent.str_vk_table.update(KeyEvent.str_vk_table_std)
            KeyEvent.vk_str_table.update(KeyEvent.vk_str_table_std)

    @staticmethod
    def strToVk(name):
        try:
            vk = KeyEvent.str_vk_table[name.upper()]
        except KeyError:
            try:
                vk = int(name.strip("()"))
            except:
                raise ValueError
        return vk

    @staticmethod
    def vkToStr(vk):
        try:
            name = KeyEvent.vk_str_table[vk]
        except KeyError:
            name = "(%d)" % vk
        return name

    @staticmethod
    def strToMod( name, force_LR=False ):
        try:
            mod = KeyEvent.str_mod_table[ name.upper() ]
        except KeyError:
            raise ValueError
        if force_LR and (mod & 0xff):
            mod <<= 8
        return mod


class Keymap:

    @staticmethod
    def init():
        KeyEvent.initTables()

    def __init__(self):
        self.table = {}

    def __setitem__( self, expression, value ):

        if isinstance(expression,KeyEvent):
            key_cond = expression
            if key_cond.extra==None:
                print( "OBSOLETE : Use string for key expression :", key_cond )
        elif isinstance(expression,int):
            key_cond = KeyEvent(expression,0)
            print( "OBSOLETE : Use string for key expression :", key_cond )
        elif isinstance(expression,str):
            try:
                key_cond = KeyEvent.fromString(expression)
            except ValueError:
                print( "ERROR : Key expression is not valid :", expression )
                return
        else:
            raise TypeError

        self.table[key_cond] = value

    def __getitem__( self, expression ):
        try:
            key_cond = KeyEvent.fromString(expression)
        except ValueError:
            print( "ERROR : Key expression is not valid :", expression )
            return

        return self.table[key_cond]

    def __delitem__( self, expression ):
        try:
            key_cond = KeyEvent.fromString(expression)
        except ValueError:
            print( "ERROR : Key expression is not valid :", expression )
            return

        del self.table[key_cond]

