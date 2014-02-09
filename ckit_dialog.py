from ckit import ckitcore
from ckit import ckit_widget
from ckit import ckit_theme
from ckit.ckit_const import *

#--------------------------------------------------------------------

class Dialog( ckitcore.Window ):

    RESULT_CANCEL = 0
    RESULT_OK     = 1

    def __init__( self, parent_window, text, items ):

        parent_window_rect = parent_window.getWindowRect()
        window_x, window_y = (parent_window_rect[0] + parent_window_rect[2])//2, (parent_window_rect[1] + parent_window_rect[3])//2

        ckitcore.Window.__init__(
            self,
            x=window_x,
            y=window_y,
            width=1,
            height=1,
            origin= ORIGIN_X_CENTER | ORIGIN_Y_CENTER,
            parent_window=parent_window,
            bg_color = ckit_theme.getColor("bg"),
            cursor0_color = ckit_theme.getColor("cursor0"),
            cursor1_color = ckit_theme.getColor("cursor1"),
            show = False,
            resizable = False,
            title = text,
            minimizebox = False,
            maximizebox = False,
            cursor = True,
            close_handler = self.onClose,
            keydown_handler = self.onKeyDown,
            char_handler = self.onChar,
            )

        self.setCursorPos( -1, -1 )

        # 部品初期化
        x = 2
        y = 1
        window_width = 0 + 2*2
        self.items = items
        for item in self.items:
            item.build( self, x, y )
            w,h = item.size()
            window_width = max( window_width, w + 2*2 )
            y += h
        window_height = y+1

        # ウインドウサイズ調整
        self.setPosSize(
            x=window_x,
            y=window_y,
            width=window_width,
            height=window_height,
            origin= ORIGIN_X_CENTER | ORIGIN_Y_CENTER
            )
        self.show(True)

        # 初期フォーカス位置
        self.focus = 0
        while True:
            if self.focus >= len(self.items):
                self.focus = None
                break
            if self.items[self.focus].focusable():
                break
            self.focus += 1

        self.result = Dialog.RESULT_CANCEL

        self.paint()

    def onClose(self):
        self.result = Dialog.RESULT_CANCEL
        self.quit()

    def onEnter(self):
        self.result = Dialog.RESULT_OK
        self.quit()

    def onKeyDown( self, vk, mod ):

        item = self.items[self.focus]
        if item.onKeyDown( vk, mod ):
            return

        if vk==VK_RETURN:
            self.onEnter()

        elif vk==VK_ESCAPE:
            self.onClose()

        elif vk==VK_UP:
            focus = self.focus
            while True:
                focus -= 1
                if focus < 0:
                    return
                if self.items[focus].focusable():
                    self.focus = focus
                    self.paint()
                    return

        elif vk==VK_DOWN:
            focus = self.focus
            while True:
                focus += 1
                if focus >= len(self.items):
                    return
                if self.items[focus].focusable():
                    self.focus = focus
                    self.paint()
                    return

    def onChar( self, ch, mod ):
        item = self.items[self.focus]
        item.onChar( ch, mod )

    def paint(self):
        for item in self.items:
            focused = (item == self.items[self.focus])
            item.paint( self, focused )

    def getResult(self):
        values = {}
        for item in self.items:
            if item.id!=None:
                values[item.id] = item.getValue()
        return ( self.result, values )

    #--------------------------------------------------------------------

    class StaticText:

        def __init__( self, indent, text ):
            self.id = None
            self.indent = indent
            self.text = text

        def build( self, window, x, y ):
            self.x = x
            self.y = y
            self.width = window.getStringWidth(self.text)

        def size(self):
            return (self.width+self.indent,1)

        def focusable(self):
            return False

        def onKeyDown( self, vk, mod ):
            pass

        def onChar( self, ch, mod ):
            pass

        def paint( self, window, focused ):
            attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg"))
            window.putString( self.x+self.indent, self.y, window.width(), 1, attr, self.text )

    #--------------------------------------------------------------------

    class Edit:

        def __init__( self, id, indent, width, text, value, candidate_handler=None, candidate_remove_handler=None ):
            self.id = id
            self.indent = indent
            self.width = width
            self.text = text
            self.value = value
            self.candidate_handler = candidate_handler
            self.candidate_remove_handler = candidate_remove_handler

        def build( self, window, x, y ):
            self.x = x
            self.y = y
            label_width = window.getStringWidth(self.text) + 2
            self.widget = ckit_widget.EditWidget( window, x+self.indent+label_width, y, self.width-self.indent-label_width, 1, self.value, [ 0, len(self.value) ], candidate_handler=self.candidate_handler, candidate_remove_handler=self.candidate_remove_handler )

        def size(self):
            return (self.width+self.indent,1)

        def focusable(self):
            return True

        def getValue(self):
            return self.widget.getText()

        def onKeyDown( self, vk, mod ):

            # Spaceで補完しない
            if vk==VK_SPACE:
                return False
            
            if self.widget.isListOpened():
                # Enter/Esc で補完候補を閉じる
                if vk==VK_RETURN or vk==VK_ESCAPE:
                    self.widget.closeList()
                    return True
            else:
                # Up/Down で補完候補を出さない
                if vk==VK_UP or vk==VK_DOWN:
                    return False
            
            return self.widget.onKeyDown( vk, mod )

        def onChar( self, ch, mod ):
            self.widget.onChar( ch, mod )

        def paint( self, window, focused ):

            if focused:
                attr = ckitcore.Attribute( fg=ckit_theme.getColor("select_fg"), bg=ckit_theme.getColor("select_bg"))
            else:
                attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg"))

            window.putString( self.x+self.indent, self.y, window.width(), 1, attr, self.text )

            self.widget.enableCursor(focused)
            self.widget.paint()

    #--------------------------------------------------------------------

    class CheckBox:

        def __init__( self, id, indent, text, value ):
            self.id = id
            self.indent = indent
            self.text = text
            self.value = value

        def build( self, window, x, y ):
            self.x = x
            self.y = y
            self.width = window.getStringWidth(self.text) + 3
            self.widget = ckit_widget.CheckBoxWidget( window, x+self.indent, y, self.width, 1, self.text, self.value )

        def size(self):
            return (self.width+self.indent,1)

        def focusable(self):
            return True

        def getValue(self):
            return self.widget.getValue()

        def onKeyDown( self, vk, mod ):
            return self.widget.onKeyDown( vk, mod )

        def onChar( self, ch, mod ):
            pass

        def paint( self, window, focused ):
            self.widget.enableCursor(focused)
            self.widget.paint()

    #--------------------------------------------------------------------

    class Choice:

        def __init__( self, id, indent, text, items, value ):
            self.id = id
            self.indent = indent
            self.text = text
            self.items = items
            self.value = value

        def build( self, window, x, y ):
            self.x = x
            self.y = y
            self.width = window.getStringWidth(self.text)
            for item in self.items:
                self.width += window.getStringWidth(item) + 2
            self.widget = ckit_widget.ChoiceWidget( window, x+self.indent, y, self.width, 1, self.text, self.items, self.value )

        def size(self):
            return (self.width+self.indent,1)

        def focusable(self):
            return True

        def getValue(self):
            return self.widget.getValue()

        def onKeyDown( self, vk, mod ):
            return self.widget.onKeyDown( vk, mod )

        def onChar( self, ch, mod ):
            pass

        def paint( self, window, focused ):
            self.widget.enableCursor(focused)
            self.widget.paint()

