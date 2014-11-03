from ckit import ckitcore
from ckit import ckit_textwindow
from ckit import ckit_widget
from ckit import ckit_theme
from ckit import ckit_wallpaper
from ckit.ckit_const import *

#--------------------------------------------------------------------

class Dialog( ckit_textwindow.TextWindow ):

    RESULT_CANCEL = 0
    RESULT_OK     = 1

    def __init__( self, parent_window, text, items ):

        self.parent_window = parent_window

        parent_window_rect = parent_window.getWindowRect()
        window_x, window_y = (parent_window_rect[0] + parent_window_rect[2])//2, (parent_window_rect[1] + parent_window_rect[3])//2

        ckit_textwindow.TextWindow.__init__(
            self,
            x=window_x,
            y=window_y,
            width=1,
            height=1,
            origin= ORIGIN_X_CENTER | ORIGIN_Y_CENTER,
            parent_window=parent_window,
            bg_color = ckit_theme.getColor("bg"),
            caret0_color = ckit_theme.getColor("caret0"),
            caret1_color = ckit_theme.getColor("caret1"),
            show = False,
            resizable = False,
            title = text,
            minimizebox = False,
            maximizebox = False,
            caret = True,
            close_handler = self.onClose,
            keydown_handler = self.onKeyDown,
            char_handler = self.onChar,
            )

        self.setCaretPosition( -1, -1 )

        # 部品初期化
        self.items = items
        self.rebuildItems()
        
        # ウインドウサイズ調整
        self.setPosSize(
            x=window_x,
            y=window_y,
            width=self.window_width,
            height=self.window_height,
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

        try:
            self.wallpaper = ckit_wallpaper.Wallpaper(self)
            self.wallpaper.copy( self.parent_window )
            self.wallpaper.adjust()
        except:
            self.wallpaper = None

        self.paint()

    def onClose(self):
        self.result = Dialog.RESULT_CANCEL
        self.quit()

    def onEnter(self):
        self.result = Dialog.RESULT_OK
        self.quit()

    def rebuildItems(self):
        x = 2
        y = 1
        self.window_width = 0 + 2*2
        for item in self.items:
            item.build( self, x, y )
            w,h = item.size()
            self.window_width = max( self.window_width, w + 2*2 )
            y += h
        self.window_height = y+1

        window_rect = self.getWindowRect()
        self.setPosSize(
            x=window_rect[0],
            y=window_rect[1],
            width=self.window_width,
            height=self.window_height,
            origin= ORIGIN_X_LEFT | ORIGIN_Y_TOP
            )

        try:
            self.wallpaper.copy( self.parent_window )
            self.wallpaper.adjust()
        except:
            self.wallpaper = None

    def onKeyDown( self, vk, mod ):

        item = self.items[self.focus]
        if item.onKeyDown( vk, mod ):
            self.rebuildItems()
            self.paint()
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
            item.paint( focused )

    def getValueById(self,id):
        for item in self.items:
            if item.id==id:
                return item.getValue()
        raise KeyError(id)

    def getResult(self):
        values = {}
        for item in self.items:
            if item.id!=None:
                values[item.id] = item.getValue()
        return ( self.result, values )

    #--------------------------------------------------------------------

    class Item:
        
        def __init__( self, visible=None ):
            self.dialog = None
            self.visible = visible
        
        def isVisible(self):
            return ( self.visible==None or self.visible(self.dialog) )

    class StaticText(Item):

        def __init__( self, indent, text, visible=None ):
            Dialog.Item.__init__(self,visible)
            self.id = None
            self.indent = indent
            self.text = text

        def build( self, dialog, x, y ):
            self.dialog = dialog
            self.x = x
            self.y = y
            self.width = self.dialog.getStringWidth(self.text)

        def size(self):
            if self.isVisible():
                return (self.width+self.indent,1)
            else:
                return (0,0)

        def focusable(self):
            return False

        def onKeyDown( self, vk, mod ):
            pass

        def onChar( self, ch, mod ):
            pass

        def paint( self, focused ):
            if not self.isVisible(): return
            attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg"))
            self.dialog.putString( self.x+self.indent, self.y, self.dialog.width(), 1, attr, self.text )

    #--------------------------------------------------------------------

    class Edit(Item):

        def __init__( self, id, indent, width, text, value, auto_complete=False, autofix_list=None, update_handler=None, candidate_handler=None, candidate_remove_handler=None, word_break_handler=None, visible=None ):
            Dialog.Item.__init__(self,visible)
            self.id = id
            self.indent = indent
            self.width = width
            self.text = text
            self.value = value
            self.selection = [ 0, len(self.value) ]
            self.auto_complete = auto_complete
            self.autofix_list = autofix_list
            self.update_handler = update_handler
            self.candidate_handler = candidate_handler
            self.candidate_remove_handler = candidate_remove_handler
            self.word_break_handler = word_break_handler
            self.widget = None

        def build( self, dialog, x, y ):

            self.dialog = dialog
            self.x = x
            self.y = y

            if self.isVisible():
                if not self.widget:
                    label_width = self.dialog.getStringWidth(self.text) + 2
                    self.widget = ckit_widget.EditWidget( self.dialog, x+self.indent+label_width, y, self.width-label_width, 1, self.value, [ 0, len(self.value) ], auto_complete = self.auto_complete, autofix_list = self.autofix_list, update_handler = self.update_handler, candidate_handler=self.candidate_handler, candidate_remove_handler=self.candidate_remove_handler, word_break_handler = self.word_break_handler )
            else:
                if self.widget:
                    self.value = self.widget.getText()
                    self.widget.destroy()
                    self.widget = None

        def size(self):
            if self.widget:
                return (self.width+self.indent,1)
            else:
                return (0,0)

        def focusable(self):
            return self.isVisible()

        def getValue(self):
            if self.widget:
                return self.widget.getText()
            else:
                return self.value

        def onKeyDown( self, vk, mod ):

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

        def paint( self, focused ):
            
            if not self.widget: return

            if focused:
                attr = ckitcore.Attribute( fg=ckit_theme.getColor("select_fg"), bg=ckit_theme.getColor("select_bg"))
            else:
                attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg"))

            self.dialog.putString( self.x+self.indent, self.y, self.dialog.width(), 1, attr, self.text )

            self.widget.enableCursor(focused)
            self.widget.paint()

    #--------------------------------------------------------------------

    class CheckBox(Item):

        def __init__( self, id, indent, text, value, visible=None ):
            Dialog.Item.__init__(self,visible)
            self.id = id
            self.indent = indent
            self.text = text
            self.value = value
            self.widget = None

        def build( self, dialog, x, y ):

            self.dialog = dialog
            self.x = x
            self.y = y

            if self.isVisible():
                if not self.widget:
                    self.width = self.dialog.getStringWidth(self.text) + 3
                    self.widget = ckit_widget.CheckBoxWidget( self.dialog, x+self.indent, y, self.width, 1, self.text, self.value )
            else:
                if self.widget:
                    self.value = self.widget.getValue()
                    self.widget.destroy()
                    self.widget = None

        def size(self):
            if self.widget:
                return (self.width+self.indent,1)
            else:
                return (0,0)

        def focusable(self):
            return self.isVisible()

        def getValue(self):
            if self.widget:
                return self.widget.getValue()
            else:
                return self.value

        def onKeyDown( self, vk, mod ):
            return self.widget.onKeyDown( vk, mod )

        def onChar( self, ch, mod ):
            pass

        def paint( self, focused ):
            if not self.widget: return
            self.widget.enableCursor(focused)
            self.widget.paint()

    #--------------------------------------------------------------------

    class Choice(Item):

        def __init__( self, id, indent, text, items, value, visible=None ):
            Dialog.Item.__init__(self,visible)
            self.id = id
            self.indent = indent
            self.text = text
            self.items = items
            self.value = value
            self.widget = None

        def build( self, dialog, x, y ):

            self.dialog = dialog
            self.x = x
            self.y = y

            if self.isVisible():
                if not self.widget:
                    self.width = self.dialog.getStringWidth(self.text)
                    for item in self.items:
                        self.width += self.dialog.getStringWidth(item) + 2
                    self.widget = ckit_widget.ChoiceWidget( self.dialog, x+self.indent, y, self.width, 1, self.text, self.items, self.value )
            else:
                if self.widget:
                    self.value = self.widget.getValue()
                    self.widget.destroy()
                    self.widget = None

        def size(self):
            if self.widget:
                return (self.width+self.indent,1)
            else:
                return (0,0)

        def focusable(self):
            return self.isVisible()

        def getValue(self):
            if self.widget:
                return self.widget.getValue()
            else:
                return self.value

        def onKeyDown( self, vk, mod ):
            return self.widget.onKeyDown( vk, mod )

        def onChar( self, ch, mod ):
            pass

        def paint( self, focused ):
            if not self.widget: return
            self.widget.enableCursor(focused)
            self.widget.paint()

