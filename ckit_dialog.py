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
        x, y = (parent_window_rect[0] + parent_window_rect[2])//2, (parent_window_rect[1] + parent_window_rect[3])//2

        ckitcore.Window.__init__(
            self,
            x=x,
            y=y,
            width=2,
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

        self.setPosSize(
            x=x,
            y=y,
            width=window_width,
            height=window_height,
            origin= ORIGIN_X_CENTER | ORIGIN_Y_CENTER
            )
        self.show(True)

        """
        self.recursive_checkbox = ckit.CheckBoxWidget( self, 2, 3, self.width()-4, 1, "サブディレクトリ", recursive )
        self.regexp_checkbox = ckit.CheckBoxWidget( self, 2, 4, self.width()-4, 1, "正規表現", regexp )
        self.ignorecase_checkbox = ckit.CheckBoxWidget( self, 2, 5, self.width()-4, 1, "大文字/小文字を無視", ignorecase )
        """

        self.focus = 0
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

        elif vk==VK_UP or (vk==VK_TAB and mod==MODKEY_SHIFT):
            focus = self.focus
            while True:
                focus -= 1
                if focus < 0:
                    return
                if self.items[focus].focusable():
                    self.focus = focus
                    self.paint()

        elif vk==VK_DOWN or vk==VK_TAB:
            focus = self.focus
            while True:
                focus += 1
                if focus >= len(self.items):
                    return
                if self.items[focus].focusable():
                    self.focus = focus
                    self.paint()

    def onChar( self, ch, mod ):
        item = self.items[self.focus]
        item.onChar( ch, mod )

    def paint(self):

        for item in self.items:
            focused = (item == self.items[self.focus])
            item.paint( self, focused )

        """
        self.recursive_checkbox.enableCursor(self.focus==GrepWindow.FOCUS_RECURSIVE)
        self.recursive_checkbox.paint()

        self.regexp_checkbox.enableCursor(self.focus==GrepWindow.FOCUS_REGEXP)
        self.regexp_checkbox.paint()

        self.ignorecase_checkbox.enableCursor(self.focus==GrepWindow.FOCUS_IGNORECASE)
        self.ignorecase_checkbox.paint()
        """

    def getResult(self):
        if self.result:
            return [ self.pattern_edit.getText(), self.recursive_checkbox.getValue(), self.regexp_checkbox.getValue(), self.ignorecase_checkbox.getValue() ]
        else:
            return None

    #--------------------------------------------------------------------

    class Edit:

        def __init__( self, id, text, value, width ):
            self.id = id
            self.text = text
            self.value = value
            self.width = width

        def build( self, window, x, y ):
            self.x = x
            self.y = y
            label_width = window.getStringWidth(self.text) + 2
            self.widget = ckit_widget.EditWidget( window, x+label_width, y, self.width-label_width, 1, self.value, [ 0, len(self.value) ] )

        def size(self):
            return (self.width,1)

        def focusable(self):
            return True

        def onKeyDown( self, vk, mod ):
            print( "Edit.onKeyDown", vk, mod )

            """
            if self.widget.isListOpened():
                if vk==VK_RETURN or vk==VK_ESCAPE:
                    self.widget.closeList()
                    return True
                else:
                    return self.widget.onKeyDown( vk, mod )
            """

            return self.widget.onKeyDown( vk, mod )

        def onChar( self, ch, mod ):
            print( "Edit.onChar", ch, mod )
            self.widget.onChar( ch, mod )

        def paint( self, window, focused ):

            print( "Edit.paint", focused )

            if focused:
                attr = ckitcore.Attribute( fg=ckit_theme.getColor("select_fg"), bg=ckit_theme.getColor("select_bg"))
            else:
                attr = ckitcore.Attribute( fg=ckit_theme.getColor("fg"))

            window.putString( self.x, self.y, window.width()-2*2, 1, attr, self.text )

            self.widget.enableCursor(focused)
            self.widget.paint()

