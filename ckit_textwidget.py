import os
import io
import re
import gc
import math
import shutil
import bisect
import cProfile

import pyauto

from ckit import ckitcore
from ckit import ckit_textwindow
from ckit import ckit_widget
from ckit import ckit_command
from ckit import ckit_theme
from ckit import ckit_key
from ckit import ckit_misc
from ckit import ckit_userconfig
from ckit import ckit_resource
from ckit.ckit_const import *


## @addtogroup textwidget テキスト編集ウィジェット機能
## @{

#--------------------------------------------------------------------

Line = ckitcore.Line

DEFAULT_LINEEND = "\r\n" # FIXME : カスタマイズ

#--------------------------------------------------------------------

import re
import struct

lexer_debug = False

class Token:

    stock_SimpleTextLine = struct.pack("ii",0,0)
        
    @staticmethod
    def pack( tokens ):
        if tokens == [(0,0)]:
            #print( "simple text line", id(Token.stock_SimpleTextLine) )
            return Token.stock_SimpleTextLine
        s = b""
        for token in tokens:
            s += struct.pack("ii",token[0],token[1])
        return s

    @staticmethod
    def unpack( s ):
        tokens = []
        for i in range( 0, len(s), 8 ):
            tokens.append( struct.unpack( "ii", s[i:i+8] ) )
        return tokens

Token_Text = 0
Token_Keyword = 1
Token_Name = 2
Token_Number = 3
Token_String = 4
Token_Preproc = 5
Token_Comment = 6
Token_Space = 7
Token_Error = -1

def rootContext():
    return 'root'

## シンタックス分析クラスのベースクラス
class Lexer:

    def __init__(self):
        pass

    def lex( self, ctx, line, detail ):
        pass

## テキストファイル用のシンタックス分析クラス
class TextLexer(Lexer):
    
    tokens = [ ( 0, Token_Text ) ]
    
    def __init__(self):
        Lexer.__init__(self)

    def lex( self, ctx, line, detail ):
        if detail:
            return TextLexer.tokens, ctx
        else:
            return ctx

## 正規表現のテーブルを使ったシンタックス分析クラスのベースクラス
class RegexLexer(Lexer):

    Detail = 1<<0  # detailモードの場合のみ使用するルール

    def __init__(self):
        Lexer.__init__(self)
        self.rule_map = {}
        self.rule_map_ctx = {}
        self.re_flags = 0
        self.compiled = False

    def _compile(self):
    
        for k in self.rule_map.keys():
            rule_list = self.rule_map[k]
            for i in range(len(rule_list)):
                rule = rule_list[i]
                rule = list(rule)
                if len(rule)<=2:
                    rule.append(None)
                if len(rule)<=3:
                    rule.append(0)
                if rule[0]!=None:
                    rule[0] = re.compile( rule[0], self.re_flags )
                rule_list[i] = rule
                
                assert( not( isinstance( rule[2], tuple ) or isinstance( rule[2], list ) ) )
                
                if rule[0]==None or (rule[3] & RegexLexer.Detail)==0:
                    if not k in self.rule_map_ctx:
                        self.rule_map_ctx[k] = []
                    self.rule_map_ctx[k].append(rule)

        self.compiled = True

    def lex( self, ctx, line, detail ):
    
        if not self.compiled:
            self._compile()
    
        if lexer_debug:
            print( "line :", line )

        tokens = []
        pos = 0
        
        def getRuleList():
            if detail:
                return self.rule_map[ctx]
            else:
                return self.rule_map_ctx[ctx]

        rule_list = getRuleList()

        while pos<=len(line):

            if lexer_debug:
                print( "  context :", ctx )
                
            last = pos==len(line)

            nearest_rule = None
            nearest_result = None
            default_rule = None

            for rule in rule_list:

                if rule[0]!=None:
                    re_result = rule[0].search( line, pos )
                    if re_result:
                        if nearest_result==None or re_result.start()<nearest_result.start():
                            nearest_result = re_result
                            nearest_rule = rule
                            if re_result.start()==pos:
                                break
                else:
                    default_rule = rule
                    break
                
            else:
                if pos<len(line):
                    tokens.append( ( pos, Error ) )
                break
                
            if lexer_debug:
                print( "  nearest_rule :", nearest_rule )
                print( "  default_rule :", default_rule )

            if default_rule:
                if len(tokens)==0 or tokens[-1][1]!=default_rule[1]:
                    if pos<len(line):
                        if lexer_debug:
                            print( "  %d, %d (default rule)" % (pos, default_rule[1]) )
                        tokens.append( ( pos, default_rule[1] ) )

            if nearest_rule:

                pos = nearest_result.start()
                rule = nearest_rule

                if isinstance( rule[1], tuple ) or isinstance( rule[1], list ):
                    action_list = rule[1]
                    for i in range(len(action_list)):
                        if len(tokens)==0 or tokens[-1][1]!=action_list[i]:
                            if pos<len(line):
                                if lexer_debug:
                                    print( "  %d, %d (group %d)" % (pos, action_list[i], i) )
                                tokens.append( ( pos, action_list[i] ) )
                        pos += len( nearest_result.group(i+1) )

                else:
                    if len(tokens)==0 or tokens[-1][1]!=rule[1]:
                        if pos<len(line):
                            if lexer_debug:
                                print( "  %d, %d" % (pos, rule[1]) )
                            tokens.append( ( pos, rule[1] ) )
                    pos += len( nearest_result.group(0) )

            if rule[2]:
                ctx = rule[2]
                rule_list = getRuleList()
                if lexer_debug:
                    print( "  context ->", ctx )
                    
            if default_rule and not nearest_rule:
                break
                    
            if last:
                break

        if detail:
            return tokens, ctx
        else:
            return ctx


#--------------------------------------------------------------------

class CompletionResult:
    def __init__( self, items, selection, left, right ):
        self.items = items
        self.selection = selection
        self.left = left
        self.right = right


class Completion:

    def __init__(self):
        pass

    def getCandidateList( self, window, edit, point ):
        return None


class WordCompletion(Completion):

    def __init__(self):
        Completion.__init__(self)

    def getCandidateList( self, window, edit, point ):

        word_left = point.wordLeft()
        word_right = point.wordRight()
        hint = edit.getText( word_left, point )
        tail = edit.getText( point, word_right )

        if not re.fullmatch( "[a-zA-Z0-9_]+", hint ):
            return None

        # 単語の途中にキャレットがあるときは補完しない
        if re.match( "[a-zA-Z0-9_]+", tail ):
            return None

        items = set()
        
        re_patern = re.compile( "\\b" + hint + "[a-zA-Z0-9_]+" )
        
        for edit in window.edit_list:
            
            # 速度のため、行数が大きすぎるファイルは無視
            if len(edit.doc.lines) > 10000:
                continue

            for line in edit.doc.lines:

                s = line.s

                # 速度のため、長すぎる行は無視
                if len(s) > 1000:
                    continue

                pos = 0
                while pos<len(s):
                    re_result = re_patern.search(s,pos)
                    if re_result:
                        items.add( re_result.group(0) )
                        pos += 1
                    else:
                        break
        
        items = list(items)
        items.sort()
        
        return CompletionResult( items, 0, word_left, point )


#--------------------------------------------------------------------

## TextWidget の動作モードのベースクラス
class Mode:

    name = "-"
    
    tab_by_space = False
    tab_width = 4
    
    show_tab = True
    show_space = False
    show_wspace = True
    show_lineend = True
    show_fileend = True

    cancel_selection_on_copy = False
    copy_line_if_not_selected = True
    cut_line_if_not_selected = True

    wordbreak = ckit_misc.WordBreak(
        ( 
            "\"!@#$%^&*()+|~-=\`[]{};:',./<>?", # 0
            ckit_misc.UnicodeRange('0','z'),       # 1
            ckit_misc.UnicodeRange('、','〟'),     # 2
            ckit_misc.UnicodeRange('ぁ','ゖ'),     # 3
            " \t",                              # 4
        ),
        {
            -1 : [ 0, 1, 2 ],
             0 : [ -1, 1, 2, 3 ],
             1 : [ -1, 0, 2, 3 ],
             2 : [ -1, 0, 1, 3 ],
             3 : [ -1, 0, 1, 2 ],
             4 : [ -1, 0, 1, 2, 3 ],
        }
    )

    bracket = (
        '{}',
        '()',
        '[]',
        '<>',
    )

    def __init__(self):
        pass

    def __str__(self):
        return self.name

    def executeCommand( self, name, info ):

        try:
            command = getattr( self, "command_" + name )
        except AttributeError:
            return False
        
        command(info)
        return True

    def enumCommand(self):
        for attr in dir(self):
            if attr.startswith("command_"):
                yield attr[ len("command_") : ]

    @staticmethod
    def staticconfigure(window):
        pass

    def configure( self, edit ):
        pass

class TextMode(Mode):

    name = "text"
    
    def __init__(self):
        Mode.__init__(self)
        self.lexer = TextLexer()
        self.completion = WordCompletion()
        
    @staticmethod
    def staticconfigure(window):
        Mode.staticconfigure(window)
        ckit_userconfig.callConfigFunc("staticconfigure_TextMode",window)

    def configure( self, edit ):
        Mode.configure( self, edit )
        ckit_userconfig.callConfigFunc("configure_TextMode",self,edit)

#--------------------------------------------------------------------

class UndoInfo:

    def __init__(self):

        self.old_text       = None
        self.old_text_left  = None
        self.old_text_right = None
        self.old_anchor     = None
        self.old_cursor     = None

        self.new_text       = None
        self.new_text_left  = None
        self.new_text_right = None
        self.new_anchor     = None
        self.new_cursor     = None

    def __str__(self):
        return "UndoInfo( %s,%s,  %s,%s )" % ( self.old_text_left, self.old_text_right, self.new_text_left, self.new_text_right )


class AtomicUndoInfo:

    def __init__(self):
        self.old_anchor = None
        self.old_cursor = None
        self.new_anchor = None
        self.new_cursor = None
        self.items      = []

    def __str__(self):
        return "AtomicUndoInfo( %s,%s,  %s,%s )" % ( self.old_anchor, self.old_cursor, self.new_anchor, self.new_cursor )


## TextWidget の文書中の位置を示すクラス
class Point:

    def __init__( self, edit, line=0, index=0 ):
        self.edit = edit
        self.line = line
        self.index = index

    def copy(self):
        return Point( self.edit, self.line, self.index )

    def __eq__(self,other):
        return isinstance(other,Point) and self.line==other.line and self.index==other.index

    def __ne__(self,other):
        return not isinstance(other,Point) or self.line!=other.line or self.index!=other.index

    def __gt__(self,other):
        if self.line>other.line:
            return True
        if self.line<other.line:
            return False
        return self.index>other.index

    def __ge__(self,other):
        if self.line>other.line:
            return True
        if self.line<other.line:
            return False
        return self.index>=other.index

    def __lt__(self,other):
        if self.line<other.line:
            return True
        if self.line>other.line:
            return False
        return self.index<other.index

    def __le__(self,other):
        if self.line<other.line:
            return True
        if self.line>other.line:
            return False
        return self.index<=other.index

    def __str__(self):
        return "Point(%d,%d)" % (self.line,self.index)

    def __repr__(self):
        return "Point(%d,%d)" % (self.line,self.index)

    def left( self, block_mode=False ):
        point = self.copy()
        if point.index>0:
            point.index -= 1
        else:
            if not block_mode and point.line>0:
                point.line -= 1
                point.index = len(self.edit.doc.lines[point.line].s)
        return point

    def right( self, block_mode=False ):
        point = self.copy()
        if block_mode or point.index<len(self.edit.doc.lines[point.line].s):
            point.index += 1
        else:
            if point.line<len(self.edit.doc.lines)-1:
                point.line += 1
                point.index = 0
        return point

    def wordLeft( self, use_type_bounds=True ):

        point = self.copy()
        if point.index>0:
            line = self.edit.doc.lines[point.line]
            point.index = self.edit.doc.mode.wordbreak( line.s, point.index, -1, use_type_bounds=use_type_bounds )
        else:
            if point.line>0:
                point.line -= 1
                line = self.edit.doc.lines[point.line]
                point.index = self.edit.doc.mode.wordbreak( line.s, len(line.s), -1, use_type_bounds=use_type_bounds )
        return point

    def wordRight( self, use_type_bounds=True ):

        point = self.copy()
        if point.index<len(self.edit.doc.lines[point.line].s):
            line = self.edit.doc.lines[point.line]
            point.index = self.edit.doc.mode.wordbreak( line.s, point.index, +1, use_type_bounds=use_type_bounds )
        else:
            if point.line<len(self.edit.doc.lines)-1:
                point.line += 1
                line = self.edit.doc.lines[point.line]
                point.index = self.edit.doc.mode.wordbreak( line.s, 0, +1, use_type_bounds=use_type_bounds )
        return point

    def tabLeft(self):
        point = self.copy()
        tab_width = self.edit.doc.mode.tab_width
        column = self.edit.getColumnFromIndex( point.line, point.index )
        column = max( ( column - 1 ) // tab_width * tab_width, 0 )
        point.index = self.edit.getIndexFromColumn( point.line, column )
        return point

    def up( self, step=1, block_mode=False ):
        point = self.copy()
        if point.line-step>=0:
            point.line -= step
            point.index = self.edit.getIndexFromColumn( point.line, self.edit.ideal_column, block_mode=block_mode )
        else:
            point.line = 0
            point.index = 0
        return point

    def down( self, step=1, block_mode=False ):
        point = self.copy()
        if point.line+step<=len(self.edit.doc.lines)-1:
            point.line += step
            point.index = self.edit.getIndexFromColumn( point.line, self.edit.ideal_column, block_mode=block_mode )
        else:
            point.line = len(self.edit.doc.lines)-1
            point.index = len(self.edit.doc.lines[point.line].s)
        return point

    def lineBegin(self):
        point = self.copy()
        point.index = 0
        return point

    def lineEnd(self):
        point = self.copy()
        point.index = len(self.edit.doc.lines[point.line].s)
        return point
        
    def lineFirstGraph(self):
        point = self.copy()
        s = self.edit.doc.lines[point.line].s
        s2 = s.lstrip(" \t")
        point.index = len(s)-len(s2)
        return point

    def correspondingBracket( self, bracket_pair_list ):
    
        point = self.copy()

        s = self.edit.doc.lines[point.line].s

        for bracket_pair in bracket_pair_list:
            if point.index<len(s) and bracket_pair[0]==s[point.index]:
                step = 1
                depth = 1
                inside = False
                point = point.right()
                break
            elif point.index<len(s) and bracket_pair[1]==s[point.index]:
                step = -1
                depth = 1
                inside = True
                break
            elif point.index>0 and bracket_pair[0]==s[point.index-1]:
                step = 1
                depth = 1
                inside = True
                break
            elif point.index>0 and bracket_pair[1]==s[point.index-1]:
                step = -1
                depth = 1
                inside = False
                point = point.left()
                break
        else:
            return point
        
        num_checked_lines = 0
                    
        while True:

            s = self.edit.doc.lines[point.line].s

            if step>0:
                pos1 = s.find( bracket_pair[0], point.index )
                pos2 = s.find( bracket_pair[1], point.index )
            else:
                pos1 = s.rfind( bracket_pair[0], 0, point.index )
                pos2 = s.rfind( bracket_pair[1], 0, point.index )
                
            if pos1>=0 and (pos2<0 or int(math.copysign(1,pos2-pos1))==step):
                depth += step
                point.index = pos1
            elif pos2>=0 and (pos1<0 or int(math.copysign(1,pos1-pos2))==step):
                depth -= step
                point.index = pos2
            else:
                if step>0:
                    point = point.lineEnd()
                else:
                    point = point.lineBegin()
                
                # 1000行チェックしても見つからなかったら諦める
                num_checked_lines += 1
                if num_checked_lines > 1000:
                    # FIXME : エラーメッセージをステータスバーに表示するべき
                    return self.copy()

            if depth<=0 : break

            if step>0:
                new_point = point.right()
            else:
                new_point = point.left()

            # 末端まで到達
            if new_point==point:
                return self.copy()

            point = new_point

        if (step>0 and not inside) or (step<0 and inside):
            point = point.right()

        return point


## TextWidget の選択範囲を示すクラス
class Selection:

    def __init__( self, doc ):
        self.pos = [ doc.pointDocumentBegin(), doc.pointDocumentBegin() ]
        self.direction = 0
        self.block_mode = False

    def left(self):
        return self.pos[0].copy()

    def right(self):
        return self.pos[1].copy()

    def cursor(self):
        if self.direction==1:
            return self.pos[1].copy()
        else:
            return self.pos[0].copy()

    def anchor(self):
        if self.direction==1:
            return self.pos[0].copy()
        else:
            return self.pos[1].copy()

    def setSelection( self, anchor, cursor ):

        anchor = anchor.copy()
        cursor = cursor.copy()

        if anchor==cursor:
            self.direction = 0
            self.pos = [ anchor, cursor ]
        elif anchor<cursor:
            self.direction = 1
            self.pos = [ anchor, cursor ]
        else:
            self.direction = -1
            self.pos = [ cursor, anchor ]

    def setBlockMode( self, block_mode ):
        self.block_mode = block_mode


## TextWidget の文書を示すクラス
class Document:

    def __init__( self, filename, mode, encoding=None ):

        if filename:
            filename = ckit_misc.normPath(filename)

        self.fd = None
        self.flock = None

        self.lines = [ Line("") ]
        self.encoding = ckit_misc.TextEncoding('utf-8')
        self.lineend = DEFAULT_LINEEND
        self.filename = filename
        self.timestamp = None
        self.readonly = False
        self.bg_color_name = None
        self.diff_mode = False

        self.undo_list = []
        self.redo_list = []
        self.atomic_undo = None
        self.modcount = 0
        
        # この Document を参照している TextWidget 等のリスト        
        self.reference_list = []
        
        self.mode = None
        self.lexer = None
        self.lex_ctx_dirty_top = 0
        self.lex_token_dirty_top = 0
        self.minor_mode_list = []

        if filename and os.path.exists(filename):

            self.fd = open( filename, "rb+" )
            self.flock = ckit_misc.FileReaderLock( self.fd.fileno() )

            self.readFile( self.fd, encoding )
            
            if ckit_misc.getFileAttribute(filename) & ckit_misc.FILE_ATTRIBUTE_READONLY:
                self.setReadOnly(True)
        
            # 大きいファイルは TextMode にする ( 100000 行 or 10 MB )
            self.fd.seek( 0, os.SEEK_END )
            file_size = self.fd.tell()
            if len(self.lines) > 100000 or file_size > 10 * 1024 * 1024:
                # FIXME : メッセージを表示するべき
                mode = TextMode()

        self.setMode(mode)

        self.clearFileModified()

    def _destroy(self):
        
        #print("Document._destroy", self.filename )

        if self.flock:
            self.flock.unlock()
            self.flock = None

        if self.fd:
            self.fd.close()
            self.fd = None
            
        self.lines = None

    def addReference( self, textwidget ):
        self.reference_list.append(textwidget)

    def removeReference( self, textwidget ):
        self.reference_list.remove(textwidget)
        if not self.reference_list:
            self._destroy()

    def getName(self):
        if self.filename:
            return os.path.basename(self.filename)
        else:
            return "untitled"

    def getFullpath(self):
        return self.filename

    def isModified(self):
        return self.modcount==None or self.modcount!=0

    def clearModified(self):
        for line in self.lines:
            line.modified = False
        self.modcount = 0

    def isFileModified(self):
        if not self.filename : return False
        try:
            timestamp = os.stat(self.filename).st_mtime
        except WindowsError:
            return False
        return self.timestamp != timestamp
        
    def clearFileModified(self):
        if self.filename:
            try:
                self.timestamp = os.stat(self.filename).st_mtime
            except WindowsError:
                self.timestamp = None

    def setMode( self, mode ):
        self.mode = mode
        self.lexer = mode.lexer
        
        for line in self.lines:
            line.ctx = None
            line.tokens = None
        self.lex_ctx_dirty_top = 0
        self.lex_token_dirty_top = 0

    def appendMinorMode( self, mode ):
        self.minor_mode_list.append(mode)

    def removeMinorMode( self, name ):
        for mode in self.minor_mode_list:
            if mode.name == name:
                self.minor_mode_list.remove(mode)
                return
        else:
            raise ValueError(name)

    def clearMinorMode(self):
        self.minor_mode_list.clear()

    def hasMinorMode( self, name ):
        for mode in self.minor_mode_list:
            if mode.name == name:
                return True
        else:
            return False

    def setReadOnly( self, readonly ):
        self.readonly = readonly
        if self.readonly:
            self.setBGColor("readonly_bg")

    def setBGColor( self, bg_color_name ):
        self.bg_color_name = bg_color_name
    
    def _decodeLine( self, b ):

        s = b.decode( encoding=self.encoding.encoding, errors='replace' )

        # 波ダッシュ → 全角チルダ
        if self.encoding.encoding=="cp932":
            s = s.replace( "\u301c", "\uff5e" )
        
        return s

    def _reload( self, position, length ):
        
        # test
        #print( "_reload", position, length )
        
        self.fd.seek( position, os.SEEK_SET )
        b = self.fd.read( length )
        s = self._decodeLine(b)
        return s

    def checkMemoryUsageAndOffload(self):

        memory_usage = ckit_misc.getProcessMemoryUsage()

        # FIXME : 閾値はもっと賢く決める
        if memory_usage > 1024 * 1024 * 1024:
            Line.offload( self.lines )
            gc.collect()

    def readFile( self, fd, encoding=None ):
    
        self.lines = []

        self.undo_list = []
        self.redo_list = []
        self.modcount = 0
        
        # エンコーディングを判定
        fd.seek( 0, os.SEEK_SET )
        data = fd.read( 1024 * 1024 )
        detected_encoding = ckit_misc.detectTextEncoding( data, ascii_as="utf-8" )
        
        # BOMの直後に戻る
        if detected_encoding.bom:
            bom_len = len(detected_encoding.bom)
        else:
            bom_len = 0
        fd.seek( bom_len, os.SEEK_SET )

        if not encoding:
            encoding = detected_encoding

        if not encoding.encoding:
            raise UnicodeError
        
        self.encoding = encoding
        
        b_cr = "\r".encode( encoding=self.encoding.encoding )
        b_lf = "\n".encode( encoding=self.encoding.encoding )
        b_crlf = b_cr + b_lf
        pattern_line = re.compile( b".*%s|.*%s|.*%s|.*" % (b_crlf, b_cr, b_lf) )
        
        position = fd.tell()
        
        # バイナリモードで 1MB ずつ読み込んで、行に分割する        
        chunk_size = 1024 * 1024
        chunk = b""

        while True:
        
            new_chunk = fd.read( chunk_size )
            chunk = chunk + new_chunk

            for match in pattern_line.finditer(chunk):
            
                # ファイルに続きがある場合は、Chunkの最後部を処理せず次に回す
                if match.end() == len(chunk):
                    if new_chunk:
                        chunk = chunk[ match.start() : ]
                        break
                
                line_s = match.group(0)
                if not line_s:
                    continue

                line_s2 = self._decodeLine(line_s)
                
                line = Line( line_s2, reload_handler=self._reload, reload_pos=position, reload_len=len(line_s) )
            
                # メモリ消費量を小さくするために初期状態はオフロード
                Line.offload(line)

                self.lines.append(line)
            
                position += len(line_s)
            
            else:
                break

        # 空か、改行で終わっている場合は、最後に行を追加する
        if len(self.lines)==0 or self.lines[-1].end:
            self.lines.append( Line("") )

        # 最初の行の改行コードを文書の改行コードとして採用する
        if len(self.lines)>0 and self.lines[0].end:
            self.lineend = self.lines[0].end
        else:
            self.lineend = DEFAULT_LINEEND

        self.lex_ctx_dirty_top = 0
        self.lex_token_dirty_top = 0

    def writeFile( self, filename ):

        fd = None
        tmp_fd = None
        tmp_filename = None
        
        # 同一ファイルへの上書きかをチェック
        overwrite = False
        if self.fd and filename and os.path.exists(filename):
            st1 = os.stat( self.fd.fileno() )
            st2 = os.stat( filename )
            overwrite = (st1.st_ino == st2.st_ino)

        # FIXME : overwrite = False のとき、テンポラリファイルを使う必要がない
        
        try:
            
            # Offloadしている行を壊さないようにBytesIOに保存
            tmp_fd = io.BytesIO()

            if self.encoding.bom:
                tmp_fd.write( self.encoding.bom )

            for i, line in enumerate(self.lines):

                s = line.s + line.end
        
                # 全角チルダ → 波ダッシュ
                if self.encoding.encoding=="cp932":
                    s = s.replace( "\uff5e", "\u301c" )
        
                b = s.encode( self.encoding.encoding, 'replace' )
                Line.updateReloadInfo( line, tmp_fd.tell(), len(b) )
                tmp_fd.write(b)
                
                # 10MB より大きいファイルになる場合は、BytesIO ではなくテンポラリファイルを使う
                if isinstance(tmp_fd,io.BytesIO) and tmp_fd.tell() > 10 * 1024 * 1024:
                    bytes_fd = tmp_fd
                    tmp_filename = ckit_misc.makeTempFile( "writeFile", ".tmp" )
                    tmp_fd = open( tmp_filename, "rb+" )
                    tmp_fd.write( bytes_fd.getbuffer() )
                    bytes_fd.close()
                
                if i % 10000 == 0:
                    self.checkMemoryUsageAndOffload()
        
            if overwrite:
                fd = self.fd
            else:
                fd = open( filename, "wb" )

            # テンポラリファイルからコピーする
            tmp_fd.seek( 0, os.SEEK_SET )
            
            # FIXME : なぜか、Unlockしないとwriteかflushでエラーになる。            
            if overwrite:
                self.flock.unlock()
            
            fd.seek( 0, os.SEEK_SET )
            fd.truncate(0)
            shutil.copyfileobj( tmp_fd, fd )

            fd.flush()
            os.fsync(fd.fileno())

        finally:
            if not overwrite and fd:
                fd.close()
            if tmp_fd:
                tmp_fd.close()
            if overwrite:
                self.flock = ckit_misc.FileReaderLock( self.fd.fileno() )

        # テンポラリファイルを削除
        if tmp_filename:
            os.unlink(tmp_filename)


    def updateSyntaxContext( self, stop=None, max_lex=None ):

        #print( "updateSyntaxContext", stop, max_lex )

        start = self.lex_ctx_dirty_top

        if stop==None:
            stop = len(self.lines)
        else:
            stop = min( stop, len(self.lines) )

        if max_lex==None:
            max_lex = len(self.lines)

        if self.lex_ctx_dirty_top==None or self.lex_ctx_dirty_top>=stop:
            return

        line = start
        num_lex = 0
        prev_line_dirty = (self.lines[line].ctx==None)

        while True:
    
            if self.lines[line].ctx==None or prev_line_dirty:

                if line==0:
                    ctx = rootContext()
                else:
                    prev_line = self.lines[line-1]
                    ctx = self.lexer.lex( prev_line.ctx, prev_line.s, False )

                if self.lines[line].ctx != ctx:
                    self.lines[line].ctx = ctx
                    self.lines[line].tokens = None
                    prev_line_dirty = True
                else:
                    prev_line_dirty = False

                num_lex += 1

            line += 1

            if line>=stop or num_lex>=max_lex:
                break

        if line<len(self.lines):
            if prev_line_dirty:
                self.lines[line].ctx = None
            self.lex_ctx_dirty_top = line
        else:
            self.lex_ctx_dirty_top = None


    def updateSyntaxTokens( self, start=None, stop=None, max_lex=None ):
    
        #print( "updateSyntaxTokens", start, stop, max_lex )

        if start==None:
            start = 0

        if stop==None:
            stop = len(self.lines)
        else:
            stop = min( stop, len(self.lines) )

        if max_lex==None:
            max_lex = len(self.lines)

        if self.lex_token_dirty_top==None or self.lex_token_dirty_top>=stop:
            return

        num_lex = 0

        for line in range( start, stop ):
            if self.lines[line].tokens==None:
                ctx = self.lines[line].ctx
                assert( ctx != None )
                tokens, tmp = self.lexer.lex( ctx, self.lines[line].s, True )
                self.lines[line].tokens = Token.pack( tokens )

                num_lex += 1
                if num_lex>=max_lex:
                    break
        
        if start<=self.lex_token_dirty_top and line==len(self.lines)-1:
            self.lex_token_dirty_top = None

    def fillSyntaxContextAndTokens( self, start, stop, ctx, tokens ):

        if start==None:
            start = 0

        if stop==None:
            stop = len(self.lines)
        else:
            stop = min( stop, len(self.lines) )

        for line in range( start, stop ):
            self.lines[line].ctx = ctx
            self.lines[line].tokens = Token.pack(tokens)

    def updateSyntaxTimer(self):
        if not isinstance( self.lexer, TextLexer ):
            self.updateSyntaxContext( max_lex=1000 )
            self.updateSyntaxTokens( start=self.lex_token_dirty_top, max_lex=100 )

    def isSyntaxDirty(self):
        return (self.lex_ctx_dirty_top!=None or self.lex_token_dirty_top!=None)

#--------------------------------------------------------------------

## TextWidget の検索条件を保存し実行するクラス
class Search:

    def __init__( self, text, word, case, regex ):

        self.text = text
        self.word = word
        self.case = case
        self.regex = regex
        self.text_upper = self.text.upper()

        # 検索するテキストの最初と最後が単語区切りになりえるかチェックして、
        # そうでなかったら単語単位の検索を無効にする
        if len(self.text)>=2 and re.match( "[a-zA-Z0-9_].*[a-zA-Z0-9_]", self.text )==None:
            self.word = False
        elif re.match( "[a-zA-Z0-9_]", self.text )==None:
            self.word = False

        if self.regex or self.word:

            if self.regex:
                if self.word:
                    re_pattern = r"\b" + self.text + r"\b"
                else:
                    re_pattern = self.text
            else:
                re_pattern = ""
                for c in self.text:
                    if c in "\\[]^":
                        c = "\\" + c;
                    re_pattern += "[" + c + "]"

                if self.word:
                    re_pattern = r"\b" + re_pattern + r"\b"

            re_option = re.UNICODE
            if not self.case:
                re_option |= re.IGNORECASE

            self.re_object = re.compile( re_pattern, re_option )
            self.re_result = None

        else:
            self.re_object = None
            self.re_result = None

    def __str__(self):
        return self.text

    def search( self, line, pos=None, end=None ):

        if self.re_object:

            if pos==None:
                re_result = self.re_object.search(line)
            elif end==None:
                re_result = self.re_object.search(line,pos)
            else:
                re_result = self.re_object.search(line,pos,end)
        
            if self.regex:
                self.re_result = re_result
            else:
                self.re_result = None
        
            if re_result:
                return ( re_result.start(), re_result.end() )
            else:
                return None
        
        else:

            if self.case:
                text = self.text
            else:
                text = self.text_upper
                line = line.upper()
            
            if pos==None:
                pos = line.find(text)
            elif end==None:
                pos = line.find(text,pos)
            else:
                pos = line.find(text,pos,end)
        
            if pos>=0:
                return ( pos, pos+len(text) )
            else:
                return None


#--------------------------------------------------------------------

class MouseInfo:
    def __init__( self, mode, **args ):
        self.mode = mode
        self.__dict__.update(args)

#--------------------------------------------------------------------

## テキスト編集ウィジェット
#
class TextWidget(ckit_widget.Widget):

    BLOCK_SELECTION_COLUMN_OFFSET = 0.5
    
    rect_mode_clipboard_number = None # 矩形選択モードで格納したクリップボードのシーケンス番号

    def __init__( self, window, x, y, width, height, message_handler=None ):

        ckit_widget.Widget.__init__( self, window, x, y, width, height )

        self.enableIme(True)

        self.command = window.command
        self.doc = None
        self.selection = Selection(self)
        self.ideal_column = 0
        self.visible_first_line = 0
        self.visible_first_column = 0
        self.scroll_margin_v = 3
        self.scroll_margin_h = 0
        self.scroll_bottom_adjust = False
        self.show_lineno = True
        self.search_object = None
        self.search_re_result = None
        self.mouse_click_info = None
        self.message_handler = message_handler

        self.selection_changed_handler_list = []

        self.candidate_window = None

        self.plane_lineno = None
        self.plane_scrollbar0 = None
        self.plane_scrollbar1 = None
        self.createThemePlane()

        doc = Document( filename=None, mode=TextMode() )
        self.setDocument(doc)

    def destroy(self):

        if self.candidate_window:
            self.candidate_window.destroy()

        self.destroyThemePlane()
        
        self.doc.removeReference(self)

    def show(self,visible):
        ckit_widget.Widget.show(self,visible)
        if visible:
            self.createThemePlane()
        else:
            self.destroyThemePlane()    

    ## 設定を読み込む
    #
    #  キーマップなどをリセットした上で、モードの設定を反映しなおします。
    #
    def configure(self):

        self.keymap = ckit_key.Keymap()

        # モードごとの設定
        self.doc.mode.configure(self)

        for mode in self.doc.minor_mode_list:
            mode.configure(self)

    def setDocument( self, doc ):
        
        if self.doc:
            self.doc.removeReference(self)
        self.doc = doc
        if self.doc:
            self.doc.addReference(self)

        self.setCursor( self.pointDocumentBegin() )
        
    def setEncoding( self, encoding ):
        self.doc.encoding = encoding
        self.doc.modcount = None
        self._notifyTextModified( self.selection.cursor(), self.selection.cursor(), self.selection.cursor() )

    def setLineEnd( self, lineend ):
        self.doc.lineend = lineend
        for line in self.doc.lines:
            if line.end:
                line.end=lineend
        self.doc.modcount = None
        self._notifyTextModified( self.selection.cursor(), self.selection.cursor(), self.selection.cursor() )

    def setPosSize( self, x, y, width, height ):
        ckit_widget.Widget.setPosSize( self, x, y, width, height )

    def createThemePlane(self):
        if not self.plane_lineno:
            self.plane_lineno = ckit_theme.ThemePlane3x3( self.window, 'lineno.png' )
        if not self.plane_scrollbar0:
            self.plane_scrollbar0 = ckit_theme.ThemePlane3x3( self.window, 'scrollbar0.png', -1 )
        if not self.plane_scrollbar1:    
            self.plane_scrollbar1 = ckit_theme.ThemePlane3x3( self.window, 'scrollbar1.png', -2 )

    def destroyThemePlane(self):
        if self.plane_lineno:
            self.plane_lineno.destroy()
            self.plane_lineno = None
        if self.plane_scrollbar0:
            self.plane_scrollbar0.destroy()
            self.plane_scrollbar0 = None
        if self.plane_scrollbar1:
            self.plane_scrollbar1.destroy()
            self.plane_scrollbar1 = None

    ## 指定した行に関してカラムから文字インデックスを取得する
    def getIndexFromColumn( self, line, column, sub_x=0.0, block_mode=False ):
        if len(self.doc.lines) <= line : return 0
        s = self.doc.lines[ line ].s
        column_list = self.window.getStringColumns( s, self.doc.mode.tab_width )
        pos = bisect.bisect_right( column_list, column ) - 1
        if pos+1 > len(column_list)-1:
            if block_mode:
                return len(s) + column - column_list[-1] + int( sub_x + 0.25 )
            else:
                return len(s)
        elif column + sub_x < ( column_list[pos] + column_list[pos+1] ) * 0.5 + 0.25:
            return pos
        else:
            return pos+1

    def getColumnFromIndex( self, line, index ):
        if len(self.doc.lines) <= line : return 0
        line = self.doc.lines[ line ].s
        s = line[:index]
        width = self.window.getStringWidth( s, self.doc.mode.tab_width )
        if index>len(line) : width += index-len(line)
        return width
        
    def getCursorPos(self):
        cursor = self.selection.cursor()
        line = self.doc.lines[cursor.line].s
        left_string = line[:cursor.index]
        x = self.x + self.lineNoWidth() + self.window.getStringWidth( left_string, self.doc.mode.tab_width ) - self.visible_first_column
        if cursor.index>len(line) : x += cursor.index-len(line)
        y = self.y + cursor.line - self.visible_first_line
        return ( x, y )

    def onKeyDown( self, vk, mod ):

        #print( "onKeyDown", vk, mod )

        if self.candidate_window:
            if self.candidate_window.onKeyDown(vk,mod):
                return True

        try:
            func = self.keymap.table[ ckit_key.KeyEvent(vk,mod) ]
        except KeyError:
            return

        func( ckit_command.CommandInfo() )

        return True


    def onChar( self, ch, mod ):

        #print( "onChar", ch )

        if mod & MODKEY_CTRL:
            return

        if self.candidate_window:
            if self.candidate_window.onChar(ch,mod):
                return True

        if ch<=0xff and ( ch<0x20 or ch==0x7f ):
            return

        self.modifyText( text=chr(ch) )

        # 補完リストが出ている間の文字入力は絞込み動作
        if self.candidate_window:
            self.popupList( auto_fix=False )


    def onLeftButtonDown( self, char_x, char_y, sub_x, sub_y, mod ):
        #print( "onLeftButtonDown", char_x, char_y, mod )
        self.mouse_click_info=None
        point = self.pointMouse( char_x, char_y, sub_x )
        if char_x<self.x+self.lineNoWidth():
            if mod==MODKEY_SHIFT:
                anchor = self.selection.anchor()
                if self.selection.direction<0 and anchor.index==0:
                    anchor_line = max(anchor.line-1,0)
                else:
                    anchor_line = anchor.line
                if point.line<anchor.line:
                    anchor = self.point(anchor_line,0).lineEnd().right()
                    cursor = point.lineBegin()
                else:
                    anchor = self.point(anchor_line,0)
                    cursor = point.lineEnd().right()
                self.setSelection( anchor, cursor, block_mode=False )
                self.mouse_click_info = MouseInfo( "lineno", line=anchor_line )
            else:
                left = point.lineBegin()
                right = point.lineEnd().right()
                self.setSelection( left, right, block_mode=False )
                self.mouse_click_info = MouseInfo( "lineno", line=point.line )
        else:
            if mod==MODKEY_SHIFT:
                anchor = self.selection.anchor()
                self.setSelection(anchor,point)
            elif mod==MODKEY_ALT:
                point = self.pointMouse( char_x, char_y, sub_x, block_mode=True )
                self.setSelection( point, point, block_mode=True )
            else:
                self.setCursor(point)
            self.mouse_click_info = MouseInfo("left")

    def onLeftButtonUp( self, char_x, char_y, sub_x, sub_y, mod ):
        #print( "onLeftButtonUp", char_x, char_y, mod )
        self.mouse_click_info=None

    def onLeftButtonDoubleClick( self, char_x, char_y, sub_x, sub_y, mod ):
        #print( "onLeftButtonDoubleClick", char_x, char_y, mod )
        self.mouse_click_info=None
        point = self.pointMouse( char_x, char_y, sub_x )
        if char_x<self.x+self.lineNoWidth():
            left = point.lineBegin()
            right = point.lineEnd().right()
            self.setSelection( left, right, block_mode=False )
            self.mouse_click_info = MouseInfo( "lineno", line=point.line )
        else:
            line = self.doc.lines[point.line]
            left = self.point( point.line, self.doc.mode.wordbreak( line.s, min(point.index+1,len(line.s)), -1, use_type_bounds=False ) )
            right = self.point( point.line, self.doc.mode.wordbreak( line.s, point.index, +1, use_type_bounds=False ) )
            self.setSelection( left, right, block_mode=False, make_visible=True, make_anchor_visible=True )
            self.mouse_click_info = MouseInfo( "left_double", left=left, right=right )

    def onRightButtonDown( self, char_x, char_y, sub_x, sub_y, mod ):
        #print( "onRightButtonDown", char_x, char_y, mod )
        pass

    def onRightButtonUp( self, char_x, char_y, sub_x, sub_y, mod ):
        #print( "onRightButtonUp", char_x, char_y, mod )
        pass

    def onMouseMove( self, char_x, char_y, sub_x, sub_y, mod ):
        #print( "onMouseMove", char_x, char_y, mod )

        if self.mouse_click_info==None : return

        if self.mouse_click_info.mode=="left":
            anchor = self.selection.anchor()
            cursor = self.pointMouse( char_x, char_y, sub_x, block_mode=self.selection.block_mode )
            self.setSelection( anchor, cursor )

        elif self.mouse_click_info.mode=="left_double":
            point = self.pointMouse( char_x, char_y, sub_x )
            line = self.doc.lines[point.line]
            if point<self.mouse_click_info.left:
                anchor = self.mouse_click_info.right
                cursor = self.point( point.line, self.doc.mode.wordbreak( line.s, min(point.index+1,len(line.s)), -1, use_type_bounds=False ) )
            elif self.mouse_click_info.right<=point:
                anchor = self.mouse_click_info.left
                cursor = self.point( point.line, self.doc.mode.wordbreak( line.s, point.index, +1, use_type_bounds=False ) )
            else:
                anchor = self.mouse_click_info.left
                cursor = self.mouse_click_info.right
            self.setSelection( anchor, cursor )

        elif self.mouse_click_info.mode=="lineno":
            point = self.pointMouse( char_x, char_y, sub_x )
            if point.line<self.mouse_click_info.line:
                anchor = self.point( self.mouse_click_info.line, 0 ).lineEnd().right()
                cursor = self.point( point.line, 0 )
            else:
                anchor = self.point( self.mouse_click_info.line, 0 )
                cursor = self.point(point.line).lineEnd().right()
            self.setSelection( anchor, cursor )


    def onMouseWheel( self, char_x, char_y, sub_x, sub_y, wheel, mod ):
        #print( "onMouseWheel", char_x, char_y, wheel, mod )
        while wheel>0:
            self.scrollV(-3)
            wheel -= 1
        while wheel<0:
            self.scrollV(3)
            wheel += 1

    def _notifyTextModified( self, left, old_right, new_right ):
        for reference in self.doc.reference_list:
            if hasattr( reference, "onDocumentTextModified" ):
                reference.onDocumentTextModified( self, left, old_right, new_right )

    def onDocumentTextModified( self, sender, left, old_right, new_right ):

        if self!=sender:
            
            def adjustPoint( point, left, old_right, new_right ):
                if point < left : return point
                elif point < old_right : return left
                else : return self.point( point.line - old_right.line + new_right.line, point.index - old_right.index + new_right.index )
        
            anchor = adjustPoint( self.selection.anchor(), left, old_right, new_right )
            cursor = adjustPoint( self.selection.cursor(), left, old_right, new_right )

            self.setSelection( anchor, cursor, paint=False )

            self.paint()

    def onDocumentBookmark( self, sender, line, bookmark ):
        if self!=sender:
            self.paint()

    def point( self, line, index=0 ):
        return Point( self, line, index )

    def pointDocumentBegin(self):
        point = self.point( 0, 0 )
        return point

    def pointDocumentEnd(self):
        point = self.point( len(self.doc.lines)-1, 0 )
        point.index = len(self.doc.lines[point.line].s)
        return point

    def pointMouse( self, char_x, char_y, sub_x=0.0, block_mode=False ):
        line = char_y - self.y + self.visible_first_line
        if line<0:
            line = 0
            index = 0
        elif line > len(self.doc.lines)-1:
            line = len(self.doc.lines)-1
            index = len(self.doc.lines[line].s)
        else:
            x = char_x - self.x
            if x < self.lineNoWidth():
                index = 0
            else:
                column = x - self.lineNoWidth() + self.visible_first_column
                index = self.getIndexFromColumn( line, column, sub_x, block_mode=block_mode )
        return self.point( line, index )

    def getText( self, left, right, block_mode=False ):

        if left.line==right.line:
            line = self.doc.lines[ left.line ]
            text = line.s[ left.index : right.index ]
        else:
            lines = []
            if not block_mode:
                line = self.doc.lines[ left.line ]
                lines.append( line.s[ left.index : ] )
                lines.append( line.end )
                for i in range( left.line+1, right.line ):
                    line = self.doc.lines[i]
                    lines.append( line.s )
                    lines.append( line.end )
                line = self.doc.lines[ right.line ]
                lines.append( line.s[ : right.index ] )
            else:
                column1 = self.getColumnFromIndex( left.line, left.index )
                column2 = self.getColumnFromIndex( right.line, right.index )
                rect_column_left = min(column1,column2)
                rect_column_right = max(column1,column2)

                for i in range( left.line, right.line+1 ):

                    sel_left = self.getIndexFromColumn( i, rect_column_left, TextWidget.BLOCK_SELECTION_COLUMN_OFFSET )
                    sel_right = self.getIndexFromColumn( i, rect_column_right, TextWidget.BLOCK_SELECTION_COLUMN_OFFSET )

                    line = self.doc.lines[i]
                    lines.append( line.s[ sel_left : sel_right ] )
                    lines.append( line.end )
                
            text = "".join(lines)
        return text

    def modifyText( self, anchor=None, cursor=None, text="", text_block_mode=False, move_cursor=True, notify_modified=True, paint=True, modstep=1, append_undo=True, undo_select=False, ignore_readonly=False ):

        #print( "modifyText", anchor, cursor )

        if self.doc.readonly and not ignore_readonly:
            self.setMessage( ckit_resource.strings["readonly"], 1000, error=True )
            return

        if anchor==None : anchor = self.selection.anchor()
        if cursor==None : cursor = self.selection.cursor()

        if anchor<=cursor:
            left=anchor.copy()
            right=cursor.copy()
        else:
            left=cursor.copy()
            right=anchor.copy()
        
        # 通常選択状態で、Blockモードのテキストの挿入禁止
        if not self.selection.block_mode and self.selection.direction!=0 and text_block_mode:
            ckit_misc.messageBeep()
            return
        
        # Block選択状態で、通常のテキストの挿入禁止
        if self.selection.block_mode and not text_block_mode and text:
            ckit_misc.messageBeep()
            return
        
        block_mode = self.selection.block_mode or text_block_mode

        if not block_mode:

            if append_undo:
                undo_info = UndoInfo()
                undo_info.old_text = self.getText(left,right)
                undo_info.old_text_left = left.copy()
                undo_info.old_text_right = right.copy()
                if undo_select:
                    undo_info.old_anchor = anchor.copy()
                    undo_info.old_cursor = cursor.copy()
                else:
                    undo_info.old_anchor = cursor.copy()
                    undo_info.old_cursor = cursor.copy()

            # 選択範囲の削除
            if left!=right:

                line1 = self.doc.lines[ left.line ]
                line2 = self.doc.lines[ right.line ]

                s1 = line1.s[ : left.index ]
                s2 = line2.s[ right.index : ]

                if s1.strip() and line1.bg:
                    bg = line1.bg
                else:
                    bg = line2.bg

                if left.index>0 or left.line==right.line:
                    bookmark = line1.bookmark
                elif right.index==0:
                    bookmark = line2.bookmark
                else:
                    bookmark = 0

                line1.s = s1 + s2
                line1.end = line2.end
                line1.bg = bg
                line1.bookmark = bookmark
                if left.index or right.index:
                    line1.modified = True
                else:
                    line1.modified = line2.modified

                if left.line != right.line:
                    del self.doc.lines[ left.line+1 : right.line+1 ]

            anchor = left
            cursor = left.copy()

            # テキストの挿入
            if text:

                insert_lines = text.splitlines(True)
                for insert_line in insert_lines:

                    if insert_line.endswith("\r\n"):
                        insert_return = True
                        insert_line = insert_line[:-2]
                    elif insert_line.endswith("\n"):
                        insert_return = True
                        insert_line = insert_line[:-1]
                    elif insert_line.endswith("\r"):
                        insert_return = True
                        insert_line = insert_line[:-1]
                    else:
                        insert_return = False

                    line = self.doc.lines[ cursor.line ]
                    line.s = line.s[ : cursor.index ] + insert_line + line.s[ cursor.index : ]
                    cursor.index += len(insert_line)

                    if insert_return:
                        s1 = line.s[ : cursor.index ]
                        s2 = line.s[ cursor.index : ]

                        lineend1 = self.doc.lineend
                        lineend2 = line.end
                        if s1.strip():
                            bg1 = line.bg
                        else:
                            bg1 = 0
                        bg2 = line.bg
                    
                        line.s = s1
                        line.end = lineend1
                        line.bg = bg1
                        line.tokens = None

                        line2 = Line( s2 + lineend2 )
                        line2.bg = bg2
                        if line.modified or cursor.index : line2.modified = True
                        self.doc.lines.insert( cursor.line+1, line2 )

                        cursor.line += 1
                        cursor.index = 0

                    line.modified = True

                if anchor.line!=cursor.line and anchor.index==0:
                    self.doc.lines[cursor.line].bookmark = self.doc.lines[anchor.line].bookmark
                    self.doc.lines[anchor.line].bookmark = 0

            # Undo情報の登録
            if append_undo:
                undo_info.new_text = text
                undo_info.new_text_left = anchor.copy()
                undo_info.new_text_right = cursor.copy()
                if undo_select:
                    undo_info.new_anchor = anchor.copy()
                    undo_info.new_cursor = cursor.copy()
                else:
                    undo_info.new_anchor = cursor.copy()
                    undo_info.new_cursor = cursor.copy()
                self.appendUndo(undo_info)
        
        else: # block_mode
        
            insert_lines = text.splitlines(False)
            num_modify_line = max( len(insert_lines), right.line-left.line+1 )

            column1 = self.getColumnFromIndex( left.line, left.index )
            column2 = self.getColumnFromIndex( right.line, right.index )
            rect_column_left = min(column1,column2)
            rect_column_right = max(column1,column2)
            
            cursor = left.copy()
            cursor.index = self.getIndexFromColumn( cursor.line, rect_column_left, TextWidget.BLOCK_SELECTION_COLUMN_OFFSET )
            anchor = right.copy()
            anchor.index = self.getIndexFromColumn( anchor.line, rect_column_right, TextWidget.BLOCK_SELECTION_COLUMN_OFFSET )

            if append_undo:
                undo_left = left.lineBegin()
                undo_right = undo_left.copy()
                undo_right.line += num_modify_line
                undo_right = min( undo_right, self.pointDocumentEnd() )
                
                undo_info = UndoInfo()
                undo_info.old_text = self.getText(undo_left,undo_right)
                undo_info.old_text_left = undo_left
                undo_info.old_text_right = undo_right
                undo_info.old_anchor = cursor.copy()
                undo_info.old_cursor = cursor.copy()

            for i in range(num_modify_line):
                
                lineno = left.line + i
                
                if i<len(insert_lines):
                    insert_line = insert_lines[i]
                else:
                    insert_line = ""

                if lineno <= right.line:

                    sel_left = self.getIndexFromColumn( lineno, rect_column_left, TextWidget.BLOCK_SELECTION_COLUMN_OFFSET )
                    sel_right = self.getIndexFromColumn( lineno, rect_column_right, TextWidget.BLOCK_SELECTION_COLUMN_OFFSET )

                    line = self.doc.lines[lineno]
                    line.s = line.s[:sel_left] + insert_line + line.s[sel_right:]
                    line.tokens = None
                    line.modified = True

                elif lineno < len(self.doc.lines):
                    
                    index = self.getIndexFromColumn( lineno, rect_column_left )

                    line = self.doc.lines[lineno]
                    line.s = line.s[:index] + insert_line + line.s[index:]
                    line.tokens = None
                    line.modified = True
                    
                else:
                    line = Line( insert_line + self.doc.lineend )
                    self.doc.lines.append( line )

            # Undo情報の登録
            if append_undo:
                undo_left = left.lineBegin()
                undo_right = undo_left.copy()
                undo_right.line += num_modify_line
                undo_right = min( undo_right, self.pointDocumentEnd() )

                undo_info.new_text = self.getText(undo_left,undo_right)
                undo_info.new_text_left = undo_left
                undo_info.new_text_right = undo_right
                undo_info.new_anchor = cursor.copy()
                undo_info.new_cursor = cursor.copy()
                
                self.appendUndo(undo_info)


        # 変更カウント
        if self.doc.modcount!=None:
            self.doc.modcount += modstep
            if self.doc.modcount==0:
                for line in self.doc.lines:
                    line.modified=False

        if move_cursor:
            self.setCursor( cursor, paint=False )

        # シンタックスハイライトの再計算が必要
        if left.line+1<len(self.doc.lines):
            self.doc.lines[left.line+1].ctx = None
            if self.doc.lex_ctx_dirty_top==None:
                self.doc.lex_ctx_dirty_top = left.line+1
            else:
                self.doc.lex_ctx_dirty_top = min( self.doc.lex_ctx_dirty_top, left.line+1 )

        self.doc.lines[left.line].tokens = None
        if self.doc.lex_token_dirty_top==None:
            self.doc.lex_token_dirty_top = left.line
        else:
            self.doc.lex_token_dirty_top = min( self.doc.lex_token_dirty_top, left.line )

        if notify_modified:
            self._notifyTextModified(left, right, cursor)

        if paint:
            self.paint()

    def replaceLines( self, func ):

        left = self.selection.left()
        right = self.selection.right()

        left = left.lineBegin()
        if right.index>0:
            right = right.lineEnd().right()

        self.atomicUndoBegin( True, left, right )
        try:
            point1 = left.copy()
            while point1<right:

                point2 = point1.lineEnd()
                
                old_text = self.getText(point1,point2)
                new_text = func(old_text)
                if new_text != old_text:
                    self.modifyText( point1, point2, new_text, move_cursor=False, notify_modified=False, paint=False )
            
                point1.line += 1
                
                if point1.line % 10000 == 0:
                    self.doc.checkMemoryUsageAndOffload()
                
        finally:
            self.atomicUndoEnd( left, right )

        self._notifyTextModified(left, right, right)

        self.setSelection( left, right, block_mode=False )
        self.paint()

    def filterLines( self, func ):

        class LineInfo:
            pass

        left = self.selection.left()
        right = self.selection.right()
        old_right = right.copy()

        left = left.lineBegin()
        if right.index>0:
            right = right.lineEnd().right()

        self.atomicUndoBegin( True, left, right )
        try:
            point1 = left.copy()
            while point1<right:

                line = self.doc.lines[point1.line]
                info = LineInfo()
                info.bookmark = line.bookmark
                info.modified = line.modified

                point2 = point1.lineEnd()
                
                text = self.getText(point1,point2)
                if func( text, info ):
                    point1 = point1.down()
                else:
                    self.modifyText( point1, point2.right(), "", move_cursor=False, notify_modified=False, paint=False )
                    right = right.up()

                if point1.line % 10000 == 0:
                    self.doc.checkMemoryUsageAndOffload()
                
        finally:
            self.atomicUndoEnd( left, right )

        self._notifyTextModified(left, old_right, right)

        self.setSelection( left, right, block_mode=False )
        self.paint()

    def appendUndo( self, info ):

        # Redoリストをクリアするので、未変更状態に戻せない
        if self.doc.modcount!=None and self.doc.modcount<0:
            self.doc.modcount = None

        self.doc.redo_list = []

        if self.doc.atomic_undo==None:
            self.doc.undo_list.append(info)
        else:
            self.doc.atomic_undo.items.append(info)
    
    def atomicUndoBegin( self, undo_select=False, anchor=None, cursor=None ):

        if anchor==None : anchor = self.selection.anchor()
        if cursor==None : cursor = self.selection.cursor()

        self.doc.atomic_undo = AtomicUndoInfo()
        self.doc.atomic_undo.old_anchor = anchor.copy()
        self.doc.atomic_undo.old_cursor = cursor.copy()

    def atomicUndoEnd( self, anchor=None, cursor=None ):

        if anchor==None : anchor = self.selection.anchor()
        if cursor==None : cursor = self.selection.cursor()

        self.doc.atomic_undo.new_anchor = anchor.copy()
        self.doc.atomic_undo.new_cursor = cursor.copy()
        self.doc.undo_list.append(self.doc.atomic_undo)
        self.doc.atomic_undo = None

    def undo(self):

        if len(self.doc.undo_list)==0 : return False

        info = self.doc.undo_list.pop()
        self.doc.redo_list.append(info)
        
        if isinstance(info,UndoInfo):
            self.modifyText( info.new_text_left, info.new_text_right, info.old_text, move_cursor=False, paint=False, modstep=-1, append_undo=False )
        elif isinstance(info,AtomicUndoInfo):
            for i, item in enumerate(reversed(info.items)):
                self.modifyText( item.new_text_left, item.new_text_right, item.old_text, move_cursor=False, notify_modified=False, paint=False, modstep=-1, append_undo=False )
            self._notifyTextModified( info.new_anchor, info.new_cursor, info.old_cursor )

        self.setSelection( info.old_anchor, info.old_cursor, paint=False )

        self.paint()

        return True

    def redo(self):

        if len(self.doc.redo_list)==0 : return False

        info = self.doc.redo_list.pop()
        self.doc.undo_list.append(info)

        if isinstance(info,UndoInfo):
            self.modifyText( info.old_text_left, info.old_text_right, info.new_text, move_cursor=False, paint=False, append_undo=False )
        elif isinstance(info,AtomicUndoInfo):
            for i, item in enumerate(info.items):
                self.modifyText( item.old_text_left, item.old_text_right, item.new_text, move_cursor=False, notify_modified=False, paint=False, append_undo=False )
            self._notifyTextModified( info.old_anchor, info.old_cursor, info.new_cursor )

        self.setSelection( info.new_anchor, info.new_cursor, paint=False )
            
        self.paint()

        return True

    def setCursor( self, cursor, update_ideal_column=True, make_visible=True, make_anchor_visible=False, paint=True ):
        cursor = min( cursor, cursor.lineEnd() )
        self.setSelection( cursor, cursor, block_mode=False, update_ideal_column=update_ideal_column, make_visible=make_visible, make_anchor_visible=make_anchor_visible, paint=paint )

    def setSelection( self, anchor, cursor, block_mode=None, update_ideal_column=True, make_visible=True, make_anchor_visible=False, paint=True ):
    
        left_old, right_old = self.selection.left(), self.selection.right()

        if block_mode!=None:
            self.selection.setBlockMode(block_mode)

        self.selection.setSelection( anchor, cursor )

        left_new, right_new = self.selection.left(), self.selection.right()

        if update_ideal_column:
            self.ideal_column = self.getColumnFromIndex( cursor.line, cursor.index )

        line_range = ( min(left_old,left_new).line, max(right_old,right_new).line+1 )

        if make_visible:
            if make_anchor_visible:
                if self.makeVisible( [ anchor, cursor ], paint=False ):
                    line_range = None
            else:
                if self.makeVisible( cursor, paint=False ):
                    line_range = None

        for selection_changed_handler in self.selection_changed_handler_list:
            selection_changed_handler( self, anchor, cursor )

        if paint:
            self.paint( line_range = line_range )

    def bookmark( self, line, bookmark_list, paint=True ):

        for i in range(len(bookmark_list)):
            if self.doc.lines[line].bookmark==bookmark_list[i]:
                i += 1
                if i>=len(bookmark_list):
                    i=0
                break
        else:
            i=0

        self.doc.lines[line].bookmark = bookmark_list[i]
        
        for reference in self.doc.reference_list:
            if hasattr( reference, "onDocumentBookmark" ):
                reference.onDocumentBookmark( self, line, bookmark_list[i] )

        if paint:
            self.paint()
        
    def setBookmarkList( self, bookmark_list ):

        for line in self.doc.lines:
            line.bookmark = 0
        
        for bookmark in bookmark_list:
            if bookmark[0] < len(self.doc.lines):
                self.doc.lines[ bookmark[0] ].bookmark = bookmark[1]

    def getBookmarkList(self):

        bookmark_list = []

        for i, line in enumerate(self.doc.lines):
            if line.bookmark:
                bookmark_list.append( ( i, line.bookmark, line.s ) )

        return bookmark_list

    def isDiffColorMode(self):
        return self.doc.diff_mode

    def setDiffColorMode(self):
        self.doc.diff_mode = True

    def clearDiffColor(self):
        if self.doc.diff_mode:
            for line in self.doc.lines:
                line.bg = 0
            self.doc.diff_mode = False

    def seek( self, direction, func, move_cursor=True ):

        cursor = self.selection.cursor()
        
        if direction>0:
            line_range = range( cursor.line+1, len(self.doc.lines) )
        else:
            line_range = range( cursor.line-1, -1, -1 )
        
        for line in line_range:

            if func(self.doc.lines[line]):
                cursor = self.point( line, 0 )
                break

            if line % 10000 == 0:
                self.doc.checkMemoryUsageAndOffload()

        else:
            return None

        if move_cursor:
            self.setCursor( cursor, make_visible=False, paint=False )
            self.makeVisible( cursor, jump_mode=True )

        return cursor

    def search( self, search_object=None, point=None, direction=1, oneline=False, move_cursor=True, select=True, hitmark=True, paint=True, message=True ):
    
        if search_object==None:
            search_object = self.search_object
        else:
            if hitmark:
                self.search_object = search_object
        
        if not search_object:
            return None

        if point:
            point = point.copy()
        else:
            if direction>0:
                point = self.selection.right()
            else:
                point = self.selection.left()

        def foundMessage():
            if message:
               self.setMessage( ckit_resource.strings["search_found"] % search_object, 1000 )

        def notFoundMessage():
            if message:
                self.setMessage( ckit_resource.strings["search_not_found"] % search_object, 3000, error=True )

        while True:

            if direction>0:
                if point.line>=len(self.doc.lines):
                    notFoundMessage()
                    return None
            else:
                if point.line<0:
                    notFoundMessage()
                    return None

            line = self.doc.lines[point.line].s

            if direction>0:
                result = search_object.search( line, point.index )
                self.search_re_result = search_object.re_result
            else:

                def rsearch( end ):
                    pos = 0
                    last_found = None
                    while pos<len(line):
                        search_result = search_object.search( line, pos, end )
                        if not search_result : break
                        if last_found==None or last_found[0] < search_result[0] :
                            self.search_re_result = search_object.re_result
                            last_found = search_result
                        pos = search_result[0] + 1
                    return last_found

                result = rsearch( point.index )

            if result:
                cursor = self.point( point.line, result[0] )
                anchor = self.point( point.line, result[1] )
                break
                
            if oneline:
                notFoundMessage()
                return None

            if direction>0:
                point.line += 1
                point.index = 0
            else:
                point.line -= 1
                point.index = len(self.doc.lines[point.line].s)
            
            if point.line % 10000 == 0:
                self.doc.checkMemoryUsageAndOffload()

        foundMessage()

        if move_cursor:
            if select:
                self.setSelection( anchor, cursor, block_mode=False, make_visible=False, paint=False )
            else:
                self.setCursor( cursor, make_visible=False, paint=False )
                
            self.makeVisible( [ anchor, cursor ], jump_mode=True, paint=paint )

        return cursor.copy()

    def jumpLineNo( self, lineno ):
        lineno = max( lineno, 0 )
        lineno = min( lineno, len(self.doc.lines)-1 )
        point = self.point(lineno,0)
        self.visible_first_line = max( point.line - self.height//2, 0 )
        self.visible_first_column = 0
        self.setCursor( point, make_visible=False, paint=False )
        self.makeVisible( point, jump_mode=True )

    def scrollV( self, step ):

        step = max( -self.visible_first_line, step )
        step = min( len(self.doc.lines)-1 - self.visible_first_line, step )
        
        if step==0 : return

        self.visible_first_line = self.visible_first_line+step
        if step<0:
            cursor = self.selection.cursor().up(-step)
        else:
            cursor = self.selection.cursor().down(step)

        if self.selection.direction==0:
            self.setCursor( cursor, update_ideal_column=False, make_visible=False, paint=False )

        self.paint()

    def makeVisible( self, points, scroll_margin_v=None, scroll_margin_h=None, jump_mode=False, paint=True ):

        scrolled = False

        if isinstance(points,Point) : points = [points]
        if scroll_margin_v==None : scroll_margin_v = self.scroll_margin_v
        if scroll_margin_h==None : scroll_margin_h = self.scroll_margin_h
        scroll_margin_v = min( scroll_margin_v, self.height//2 )
        scroll_margin_h = min( scroll_margin_h, self.width//2 )
        
        for point in points:

            if jump_mode:
                if point.line < self.visible_first_line or point.line > self.visible_first_line + (self.height-1):
                    self.visible_first_line = point.line - self.height // 3
                    scrolled = True

            if point.line > self.visible_first_line + (self.height-1) - scroll_margin_v:
                self.visible_first_line = max( point.line - (self.height-1) + scroll_margin_v, 0 )
                scrolled = True

            if point.line < self.visible_first_line + scroll_margin_v:
                self.visible_first_line = max( point.line - scroll_margin_v, 0 )
                scrolled = True

            if self.scroll_bottom_adjust:
                if self.visible_first_line > len(self.doc.lines)-self.height+scroll_margin_v:
                    self.visible_first_line = len(self.doc.lines)-self.height+scroll_margin_v
                    scrolled = True

            if self.visible_first_line < 0:
                self.visible_first_line = 0
                scrolled = True

            text_rect = self.rectText()
            text_width = text_rect[2]-text_rect[0]

            column = self.getColumnFromIndex(point.line,point.index)

            if column > self.visible_first_column + (text_width-1) - scroll_margin_h:
                self.visible_first_column = max( column - (text_width-1) + scroll_margin_h, 0 )
                scrolled = True
        
            if column < self.visible_first_column + scroll_margin_h:
                self.visible_first_column = max( column - scroll_margin_h, 0 )
                scrolled = True

        if paint:
            self.paint()
        
        return scrolled

    def lineNoWidth(self):
        if self.show_lineno:
            str_max_lineno = str(len(self.doc.lines))
            keta = len(str_max_lineno)
            return min( keta+2, self.width )
        else:
            return min( 2, self.width )

    def rectLineNo(self):
        lineno_width = self.lineNoWidth()
        return [ self.x, self.y, self.x+lineno_width, self.y+self.height ]

    def rectText(self):
        lineno_width = self.lineNoWidth()
        return [ self.x+lineno_width, self.y, self.x+self.width-2, self.y+self.height ]

    def rectScrollbar(self):
        scrollbar_width = min( 2, self.width )
        return [ self.x+self.width-scrollbar_width, self.y, self.x+self.width, self.y+self.height ]

    @staticmethod
    def updateColor():
        TextWidget.color_fg = ckit_theme.getColor("fg")
        TextWidget.color_select_fg = ckit_theme.getColor("select_fg")
        TextWidget.color_select_bg = ckit_theme.getColor("select_bg")
        TextWidget.color_bar_fg = ckit_theme.getColor("bar_fg")
        TextWidget.color_bar_error_fg = ckit_theme.getColor("bar_error_fg")
        TextWidget.color_bookmark1 = ckit_theme.getColor("bookmark1")
        TextWidget.color_bookmark2 = ckit_theme.getColor("bookmark2")
        TextWidget.color_bookmark3 = ckit_theme.getColor("bookmark3")
        TextWidget.color_diff_bg1 = ckit_theme.getColor("diff_bg1")
        TextWidget.color_diff_bg2 = ckit_theme.getColor("diff_bg2")
        TextWidget.color_diff_bg3 = ckit_theme.getColor("diff_bg3")
        TextWidget.color_line_cursor = ckit_theme.getColor("line_cursor")
        TextWidget.color_search_mark = ckit_theme.getColor("search_mark")
        TextWidget.color_syntax_text = ckit_theme.getColor("text", "SYNTAX_COLOR")
        TextWidget.color_syntax_keyword = ckit_theme.getColor("keyword", "SYNTAX_COLOR")
        TextWidget.color_syntax_name = ckit_theme.getColor("name", "SYNTAX_COLOR")
        TextWidget.color_syntax_number = ckit_theme.getColor("number", "SYNTAX_COLOR")
        TextWidget.color_syntax_string = ckit_theme.getColor("string", "SYNTAX_COLOR")
        TextWidget.color_syntax_preproc = ckit_theme.getColor("preproc", "SYNTAX_COLOR")
        TextWidget.color_syntax_comment = ckit_theme.getColor("comment", "SYNTAX_COLOR")
        TextWidget.color_syntax_space = ckit_theme.getColor("space", "SYNTAX_COLOR")
        TextWidget.color_syntax_error = ckit_theme.getColor("error", "SYNTAX_COLOR")
        gray_select_bg = (TextWidget.color_select_bg[0]+TextWidget.color_select_bg[1]+TextWidget.color_select_bg[2])//3
        TextWidget.color_select_bg_inactive = ( gray_select_bg, gray_select_bg, gray_select_bg )

    def paint( self, line_range=None, paint_cursor=True ):

        if not self.visible: return

        x = self.x
        y = self.y
        width=self.width
        height=self.height

        selection_left = self.selection.left()
        selection_right = self.selection.right()
        cursor = self.selection.cursor()
        block_mode = self.selection.block_mode
        if block_mode:
            column1 = self.getColumnFromIndex( selection_left.line, selection_left.index )
            column2 = self.getColumnFromIndex( selection_right.line, selection_right.index )
            selection_rect_column_left = min(column1,column2)
            selection_rect_column_right = max(column1,column2)

        active = self.enable_cursor and self.window.isForeground()

        client_rect = self.window.getClientRect()
        offset_x, offset_y = self.window.charToClient( 0, 0 )
        char_w, char_h = self.window.getCharSize()

        tab_width = self.doc.mode.tab_width
        show_tab = self.doc.mode.show_tab
        show_space = self.doc.mode.show_space
        show_wspace = self.doc.mode.show_wspace
        show_lineend = self.doc.mode.show_lineend
        show_fileend = self.doc.mode.show_fileend
        
        if show_tab:
            altchar_tab = '►'
        else:
            altchar_tab = ' '
        if show_space:
            altchar_space = '˽'
        else:
            altchar_space = ' '
        if show_wspace:
            altchar_wspace = '□'
        else:
            altchar_wspace = '　'
        if show_lineend:
            altchar_lineend = '↵'
        else:
            altchar_lineend = ' '
        if show_fileend:
            altchar_fileend = '[EOF]'
        else:    
            altchar_fileend = ''

        lineno_width = self.lineNoWidth()
        keta = lineno_width-2

        # ウインドウの端にあわせる
        offset_x2 = 0
        if self.x==0 : offset_x2 = offset_x
        offset_x3 = 0
        if self.x+self.width==self.window.width() : offset_x3 = offset_x
        offset_y2 = 0
        if self.y==0 : offset_y2 = offset_y
        offset_y3 = 0
        if self.y+self.height==self.window.height() : offset_y3 = offset_y

        # 行番号
        self.plane_lineno.setPosSize( self.x*char_w+offset_x-offset_x2, self.y*char_h+offset_y-offset_y2, int((keta+1.6)*char_w)+offset_x2, self.height*char_h+offset_y2 )

        # スクロールバー
        scrollbar_rect = self.rectScrollbar()
        scrollbar_topleft = self.window.charToClient( scrollbar_rect[0], scrollbar_rect[1] )
        scrollbar_bottomright = self.window.charToClient( scrollbar_rect[2], scrollbar_rect[3] )
        scrollbar0_width  = scrollbar_bottomright[0]-scrollbar_topleft[0]
        scrollbar0_height = scrollbar_bottomright[1]-scrollbar_topleft[1]

        scrollbar1_height = scrollbar0_height * self.height // max( len(self.doc.lines)+self.height-1, 1 )
        scrollbar1_height = max( scrollbar1_height, char_h )
        scrollbar1_height = min( scrollbar1_height, scrollbar0_height )
        scrollbar1_y      = scrollbar_topleft[1] + (scrollbar0_height-scrollbar1_height) * self.visible_first_line // len(self.doc.lines)

        self.plane_scrollbar0.setPosSize( scrollbar_topleft[0], scrollbar_topleft[1]-offset_y2, scrollbar0_width+offset_x3, scrollbar0_height+offset_y2+offset_y3 )
        self.plane_scrollbar1.setPosSize( scrollbar_topleft[0], scrollbar1_y , scrollbar0_width, scrollbar1_height )

        # テキストのシンタックスハイライトの更新
        visible_bottom = self.visible_first_line + self.height
        if isinstance( self.doc.lexer, TextLexer ):
            self.doc.fillSyntaxContextAndTokens( start=self.visible_first_line, stop=visible_bottom, ctx=rootContext(), tokens=TextLexer.tokens )
        else:
            self.doc.updateSyntaxContext( stop=visible_bottom )
            self.doc.updateSyntaxTokens( start=self.visible_first_line, stop=visible_bottom )

        # テキストの描画
        x2 = x+lineno_width
        width2 = width-lineno_width
        
        attribute_whitespace = ckitcore.Attribute( fg=TextWidget.color_fg )

        # FIXME : この処理を毎回やる必要なし？
        bookmark_line = ( LINE_RECTANGLE, TextWidget.color_bar_fg )
        attribute_lineno_table = {}
        attribute_lineno_table[ ( False, 0 ) ] = ckitcore.Attribute( fg=TextWidget.color_bar_fg,        bg=None,                            line1=None )
        attribute_lineno_table[ ( False, 1 ) ] = ckitcore.Attribute( fg=TextWidget.color_bar_fg,        bg=TextWidget.color_bookmark1,  line1=bookmark_line )
        attribute_lineno_table[ ( False, 2 ) ] = ckitcore.Attribute( fg=TextWidget.color_bar_fg,        bg=TextWidget.color_bookmark2,  line1=bookmark_line )
        attribute_lineno_table[ ( False, 3 ) ] = ckitcore.Attribute( fg=TextWidget.color_bar_fg,        bg=TextWidget.color_bookmark3,  line1=bookmark_line )
        attribute_lineno_table[ ( True,  0 ) ] = ckitcore.Attribute( fg=TextWidget.color_bar_error_fg,  bg=None,                            line1=None )
        attribute_lineno_table[ ( True,  1 ) ] = ckitcore.Attribute( fg=TextWidget.color_bar_error_fg,  bg=TextWidget.color_bookmark1,  line1=bookmark_line )
        attribute_lineno_table[ ( True,  2 ) ] = ckitcore.Attribute( fg=TextWidget.color_bar_error_fg,  bg=TextWidget.color_bookmark2,  line1=bookmark_line )
        attribute_lineno_table[ ( True,  3 ) ] = ckitcore.Attribute( fg=TextWidget.color_bar_error_fg,  bg=TextWidget.color_bookmark3,  line1=bookmark_line )

        default_bg_color = None
        if self.doc.bg_color_name:
            default_bg_color = ckit_theme.getColor(self.doc.bg_color_name)
        
        # FIXME : この処理を毎回やる必要なし？
        attribute_table = {}
        bg_color_list = ( default_bg_color, TextWidget.color_diff_bg1, TextWidget.color_diff_bg2, TextWidget.color_diff_bg3 )
        line0_list = ( None, ( LINE_DOT | LINE_BOTTOM, TextWidget.color_line_cursor ) )
        line1_list = ( None, ( LINE_BOTTOM, TextWidget.color_search_mark ) )
        for bg in range(4):
            bg_color = bg_color_list[bg]
            for line_cursor in (False,True):
                line0 = line0_list[line_cursor]
                for search_mark in (False,True):
                    line1 = line1_list[search_mark]
                    attribute_table[ ( Token_Text,    bg, line_cursor, search_mark ) ] = ckitcore.Attribute( fg=TextWidget.color_syntax_text,     bg=bg_color, line0=line0, line1=line1 )
                    attribute_table[ ( Token_Keyword, bg, line_cursor, search_mark ) ] = ckitcore.Attribute( fg=TextWidget.color_syntax_keyword,  bg=bg_color, line0=line0, line1=line1 )
                    attribute_table[ ( Token_Name,    bg, line_cursor, search_mark ) ] = ckitcore.Attribute( fg=TextWidget.color_syntax_name,     bg=bg_color, line0=line0, line1=line1 )
                    attribute_table[ ( Token_Number,  bg, line_cursor, search_mark ) ] = ckitcore.Attribute( fg=TextWidget.color_syntax_number,   bg=bg_color, line0=line0, line1=line1 )
                    attribute_table[ ( Token_String,  bg, line_cursor, search_mark ) ] = ckitcore.Attribute( fg=TextWidget.color_syntax_string,   bg=bg_color, line0=line0, line1=line1 )
                    attribute_table[ ( Token_Preproc, bg, line_cursor, search_mark ) ] = ckitcore.Attribute( fg=TextWidget.color_syntax_preproc,  bg=bg_color, line0=line0, line1=line1 )
                    attribute_table[ ( Token_Comment, bg, line_cursor, search_mark ) ] = ckitcore.Attribute( fg=TextWidget.color_syntax_comment,  bg=bg_color, line0=line0, line1=line1 )
                    attribute_table[ ( Token_Space,   bg, line_cursor, search_mark ) ] = ckitcore.Attribute( fg=TextWidget.color_syntax_space,    bg=bg_color, line0=line0, line1=line1 )
                    attribute_table[ ( Token_Error,   bg, line_cursor, search_mark ) ] = ckitcore.Attribute( fg=TextWidget.color_syntax_error,    bg=bg_color, line0=line0, line1=line1 )

        if active:
            attribute_select = ckitcore.Attribute( fg=TextWidget.color_select_fg, bg=TextWidget.color_select_bg )
            attribute_select_search_mark = ckitcore.Attribute( fg=TextWidget.color_select_fg, bg=TextWidget.color_select_bg, line1=line1_list[1] )
        else:
            attribute_select = ckitcore.Attribute( fg=TextWidget.color_select_fg, bg=TextWidget.color_select_bg_inactive )
            attribute_select_search_mark = ckitcore.Attribute( fg=TextWidget.color_select_fg, bg=TextWidget.color_select_bg_inactive, line1=line1_list[1] )

        white_space = " " * width

        for i in range(height):
            line = i+self.visible_first_line

            if line_range and not ( line_range[0] <= line < line_range[1] ):
                continue
            
            if line < len(self.doc.lines):

                bg = self.doc.lines[line].bg
                
                line_cursor = 0
                if paint_cursor and active and line==cursor.line : line_cursor = 1
                
                search_hit = []
                if self.search_object:
                    search_pos = 0
                    while True:
                        search_result = self.search_object.search( self.doc.lines[line].s, search_pos )
                        if search_result and search_result[1]!=search_pos:
                            search_hit.append(search_result)
                            search_pos = search_result[1]
                        else:
                            break
                
                # 行番号の描画
                if self.show_lineno:
                    str_lineno = str(line+1)
                    self.window.putString( x, y+i, width, 1, attribute_lineno_table[ ( self.doc.lines[line].modified, self.doc.lines[line].bookmark ) ], " "*(keta-len(str_lineno)+1)+str_lineno )
                    self.window.putString( x+lineno_width-1, y+i, width-lineno_width+1, 1, attribute_whitespace, " " )
                else:
                    self.window.putString( x, y+i, width, 1, attribute_whitespace, "  " )

                paint_offset = [0]

                def paintToken( s, begin, end, attr_normal, attr_gray ):

                    if 0:
                        s2 = ckit_misc.expandTab( self.window, s[ begin : end ], tab_width, offset=paint_offset[0] )
                        self.window.putString( x2, y+i, width2, 1, attr_normal, s2, offset=paint_offset[0]-self.visible_first_column )
                        paint_offset[0] += self.window.getStringWidth(s2)
                        
                    if 1:
                        pos = begin

                        while True:

                            tab_pos = s.find('\t',pos,end)
                            if show_space:
                                space_pos = s.find(' ',pos,end)
                            else:
                                space_pos = -1
                            if show_wspace:
                                wspace_pos = s.find('　',pos,end)
                            else:    
                                wspace_pos = -1

                            if tab_pos>=0 and (space_pos<0 or tab_pos<space_pos) and (wspace_pos<0 or tab_pos<wspace_pos):
                                new_pos = tab_pos
                            elif space_pos>=0 and (tab_pos<0 or space_pos<tab_pos) and (wspace_pos<0 or space_pos<wspace_pos):
                                new_pos = space_pos
                            elif wspace_pos>=0 and (tab_pos<0 or wspace_pos<tab_pos) and (space_pos<0 or wspace_pos<space_pos):
                                new_pos = wspace_pos
                            else:
                                s2 = s[pos:end]
                                self.window.putString( x2, y+i, width2, 1, attr_normal, s2, offset=paint_offset[0]-self.visible_first_column )
                                paint_offset[0] += self.window.getStringWidth(s2)
                                break

                            if pos<new_pos:
                                s2 = s[pos:new_pos]
                                self.window.putString( x2, y+i, width2, 1, attr_normal, s2, offset=paint_offset[0]-self.visible_first_column )
                                paint_offset[0] += self.window.getStringWidth(s2)

                            if new_pos==tab_pos:
                                if show_tab:
                                    attr = attr_gray
                                else:
                                    attr = attr_normal
                                s2 = altchar_tab
                                s2_width = 1
                                while (paint_offset[0]+s2_width)%tab_width:
                                    s2 += ' '
                                    s2_width += 1

                            elif new_pos==space_pos:
                                if show_space:
                                    attr = attr_gray
                                else:
                                    attr = attr_normal
                                s2 = altchar_space
                                s2_width = 1

                            elif new_pos==wspace_pos:
                                if show_wspace:
                                    attr = attr_gray
                                else:
                                    attr = attr_normal
                                s2 = altchar_wspace
                                s2_width = 2

                            self.window.putString( x2, y+i, width2, 1, attr, s2, offset=paint_offset[0]-self.visible_first_column )
                            paint_offset[0] += s2_width

                            pos = new_pos+1

                def paintNormal( s, begin, end, tokens ):

                    for t in range(len(tokens)):

                        pos1 = tokens[t][0]
                        if t+1<len(tokens):
                            pos2 = tokens[t+1][0]
                        else:
                            pos2 = end
                        
                        if pos2<=begin : continue
                        if pos1>=end : break
                        
                        pos1 = max( pos1, begin )
                        pos2 = min( pos2, end )

                        for h in range(len(search_hit)):
                        
                            if search_hit[h][1]<=pos1 : continue
                            if search_hit[h][0]>=pos2 : break
                            
                            hit1 = max( search_hit[h][0], pos1 )
                            hit2 = min( search_hit[h][1], pos2 )

                            if pos1 < hit1:
                                paintToken( s, pos1, hit1, attribute_table[ ( tokens[t][1], bg, line_cursor, False ) ], attribute_table[ ( Token_Space, bg, line_cursor, False ) ] )

                            if hit1 < hit2:
                                paintToken( s, hit1, hit2, attribute_table[ ( tokens[t][1], bg, line_cursor, True ) ], attribute_table[ ( Token_Space, bg, line_cursor, True ) ] )
                                
                            pos1 = hit2
                            
                        paintToken( s, pos1, pos2, attribute_table[ ( tokens[t][1], bg, line_cursor, False ) ], attribute_table[ ( Token_Space, bg, line_cursor, False ) ] )

                def paintSelected( s, begin, end ):

                    pos1 = begin
                    pos2 = end

                    for h in range(len(search_hit)):
                    
                        if search_hit[h][1]<=pos1 : continue
                        if search_hit[h][0]>=pos2 : break
                        
                        hit1 = max( search_hit[h][0], pos1 )
                        hit2 = min( search_hit[h][1], pos2 )

                        if pos1 < hit1:
                            paintToken( s, pos1, hit1, attribute_select, attribute_select )

                        if hit1 < hit2:
                            paintToken( s, hit1, hit2, attribute_select_search_mark, attribute_select_search_mark )
                            
                        pos1 = hit2

                    paintToken( s, pos1, pos2, attribute_select, attribute_select )

                def paintEnd( selected ):
                    
                    if line == len(self.doc.lines)-1:
                        s = altchar_fileend
                        if show_fileend:
                            attr = attribute_table[ ( Token_Space, bg, line_cursor, False ) ]
                        else:
                            s = ' '
                            attr = attribute_table[ ( Token_Text, bg, line_cursor, False ) ]
                    else:
                        s = altchar_lineend
                        if show_lineend:
                            attr = attribute_table[ ( Token_Space, bg, line_cursor, False ) ]
                        else:
                            attr = attribute_table[ ( Token_Text, bg, line_cursor, False ) ]
                
                    if selected:
                        attr = attribute_select
                
                    self.window.putString( x2, y+i, width2, 1, attr, s, offset=paint_offset[0]-self.visible_first_column )
                    paint_offset[0] += self.window.getStringWidth(s)

                def paintSpace( space_width, selected ):
                    offset2 = max( paint_offset[0]-self.visible_first_column, 0 )
                    if selected:
                        attr = attribute_select
                    else:
                        attr = attribute_table[ ( Token_Text, bg, line_cursor, False ) ]
                    self.window.putString( x2, y+i, min(offset2+space_width,width2), 1, attr, white_space, offset=offset2 )
                    paint_offset[0] += space_width

                tokens = self.doc.lines[line].tokens

                if tokens==None:
                    print( "no syntax info", line )
                    continue

                tokens = Token.unpack(tokens)

                # 完全に選択範囲外の行
                if self.selection.direction==0 or line < selection_left.line or line > selection_right.line:
                    paintNormal( self.doc.lines[line].s, 0, len(self.doc.lines[line].s), tokens )
                    paintEnd( selected = False )
                    paintSpace( space_width=width2, selected = False )

                # 矩形選択モードの範囲行
                elif block_mode:
                    
                    sel_left = self.getIndexFromColumn( line, selection_rect_column_left, TextWidget.BLOCK_SELECTION_COLUMN_OFFSET )
                    sel_right = self.getIndexFromColumn( line, selection_rect_column_right, TextWidget.BLOCK_SELECTION_COLUMN_OFFSET )

                    paintNormal( self.doc.lines[line].s, 0, sel_left, tokens )
                    paintSelected( self.doc.lines[line].s, sel_left, sel_right )
                    paintNormal( self.doc.lines[line].s, sel_right, len(self.doc.lines[line].s), tokens )
                    paintEnd( selected = (selection_rect_column_left<=paint_offset[0]<selection_rect_column_right) )
                    if paint_offset[0]<selection_rect_column_left:
                        paintSpace( selection_rect_column_left-paint_offset[0], selected=False )
                    if paint_offset[0]<selection_rect_column_right:
                        paintSpace( selection_rect_column_right-paint_offset[0], selected=True )
                    paintSpace( space_width=width2, selected = False )

                else:

                    # 完全に選択範囲内の行
                    if selection_left.line < line < selection_right.line:
                        paintSelected( self.doc.lines[line].s, 0, len(self.doc.lines[line].s) )
                        paintSelected( altchar_lineend, 0, 1 )
                        paintSpace( space_width=width2, selected = False )

                    # 部分的に選択されている行
                    else:

                        sel_left = 0
                        sel_right = len(self.doc.lines[line].s)

                        if line == selection_left.line:
                            sel_left = selection_left.index
                        if line == selection_right.line:
                            sel_right = selection_right.index

                        paintNormal( self.doc.lines[line].s, 0, sel_left, tokens )
                        paintSelected( self.doc.lines[line].s, sel_left, sel_right )
                        paintNormal( self.doc.lines[line].s, sel_right, len(self.doc.lines[line].s), tokens )
                        paintEnd( selected = (line<selection_right.line) )
                        paintSpace( space_width=width2, selected = False )

            else:
                self.window.putString( x, y+i, width, 1, attribute_whitespace, " "*lineno_width )
                self.window.putString( x2, y+i, width2, 1, attribute_whitespace, white_space )

        if self.enable_cursor and self.window.isActive():
            x, y = self.getCursorPos()
            if paint_cursor and self.x<=x<self.x+self.width and self.y<=y<self.y+self.height:
                self.window.setCursorPos( x, y )
            else:
                self.window.setCursorPos( -1, -1 )

        # チラつきの原因になるのでコメントアウト
        #self.window.flushPaint()
        
        self.doc.checkMemoryUsageAndOffload()

    def setMessage( self, message, timeout=None, error=None ):
        if self.message_handler:
            self.message_handler( message, timeout, error )

    def onWindowMove(self):
        self.closeList()

    def onWindowSize(self):
        self.closeList()

    def popupList( self, auto_fix=True ):

        self.closeList(delay=True)

        cursor = self.selection.cursor()
        completion_result = self.doc.mode.completion.getCandidateList( self.window, self, cursor )
        if completion_result==None or not completion_result.items : return

        if auto_fix and len(completion_result.items)==1:
            self.modifyText( completion_result.left, completion_result.right, completion_result.items[0] )
            return

        def onKeyDown( vk, mod ):

            if mod==0:

                if vk==VK_RETURN:
                    selection = self.candidate_window.getSelection()
                    text = completion_result.items[selection]
                    self.closeList()
                    self.modifyText( completion_result.left, completion_result.right, text )
                    return True

                elif vk==VK_TAB:
                    common_prefix = completion_result.items[0]
                    for item in completion_result.items:
                        while common_prefix:
                            if item.startswith(common_prefix) : break
                            common_prefix = common_prefix[:-1]
                        if not common_prefix : break
                    if common_prefix:
                        if self.getText( completion_result.left, completion_result.right ) != common_prefix:
                            self.modifyText( completion_result.left, completion_result.right, common_prefix )
                            self.popupList( auto_fix=auto_fix )
                    return True

        def onSelChange( select, text ):
            #print( select, text )
            pass

        self.candidate_window = CandidateWindow( 0, 0, 10, 1, 80, 16, self.window, keydown_hook=onKeyDown, selchange_handler=onSelChange )

        left_string = self.doc.lines[completion_result.left.line].s[:completion_result.left.index]
        x = self.x + self.lineNoWidth() + self.window.getStringWidth( left_string, self.doc.mode.tab_width ) - self.visible_first_column
        y = self.y + completion_result.left.line - self.visible_first_line

        screen_x, screen_y1 = self.window.charToScreen( x, y )
        screen_x, screen_y2 = self.window.charToScreen( x, y+1 )

        self.candidate_window.setItems( screen_x, screen_y1, screen_y2, completion_result.items, completion_result.selection )

    def closeList( self, delay=False ):
        if self.candidate_window:
            if delay:
                # チラつき防止の遅延削除
                class DelayedCall:
                    def __call__(self):
                        self.candidate_window.destroy()
                delay = DelayedCall()
                delay.candidate_window = self.candidate_window
                self.window.delayedCall( delay, 50 )
            else:
                self.candidate_window.destroy()
            self.candidate_window = None

    def isListOpened(self):
        return self.candidate_window!=None

    def executeCommand( self, name, info ):
    
        if self.candidate_window:
            if self.candidate_window.executeCommand( name, info ):
                return True
            else:
                self.closeList()
        
        for mode in reversed(self.doc.minor_mode_list):
            if mode.executeCommand( name, info ):
                return True

        if self.doc.mode.executeCommand( name, info ):
            return True

        try:
            command = getattr( self, "command_" + name )
        except AttributeError:
            return False 
               
        command(info)
        return True

    def enumCommand(self):

        if self.candidate_window:
            for item in self.candidate_window.enumCommand():
                yield item

        for mode in reversed(self.doc.minor_mode_list):
            for item in mode.enumCommand():
                yield item

        for item in self.doc.mode.enumCommand():
            yield item

        for attr in dir(self):
            if attr.startswith("command_"):
                yield attr[ len("command_") : ]

    #--------------------------------------------------------
    # ここから下のメソッドはキーに割り当てることができる
    #--------------------------------------------------------

    def command_CursorLeft( self, info ):

        if self.selection.direction==0:
            cursor = self.selection.cursor().left()
        else:
            cursor = self.selection.left()

        self.setCursor(cursor)

    def command_CursorRight( self, info ):

        if self.selection.direction==0:
            cursor = self.selection.cursor().right()
        else:
            cursor = self.selection.right()

        self.setCursor(cursor)

    def command_CursorWordLeft( self, info ):

        cursor = self.selection.cursor().wordLeft()

        self.setCursor(cursor)

    def command_CursorWordRight( self, info ):

        cursor = self.selection.cursor().wordRight()

        self.setCursor(cursor)

    def command_CursorTabLeft( self, info ):

        cursor = self.selection.cursor().tabLeft()

        self.setCursor(cursor)

    def command_CursorLineBegin( self, info ):

        cursor = self.selection.cursor().lineBegin()

        self.setCursor(cursor)

    def command_CursorLineEnd( self, info ):

        cursor = self.selection.cursor().lineEnd()

        self.setCursor(cursor)

    def command_CursorLineFirstGraph( self, info ):

        cursor = self.selection.cursor()
        cursor2 = cursor.lineFirstGraph()
        
        if cursor==cursor2:
            info.result = False
            return

        self.setCursor(cursor2)

    def command_CursorUp( self, info ):

        cursor = self.selection.cursor().up()

        self.setCursor( cursor, update_ideal_column=False )

    def command_CursorDown( self, info ):

        cursor = self.selection.cursor().down()

        self.setCursor( cursor, update_ideal_column=False )

    def command_CursorPageUp( self, info ):

        step = self.height

        self.visible_first_line = max( self.visible_first_line-step, 0 )
        cursor = self.selection.cursor().up(step)

        self.setCursor( cursor, update_ideal_column=False, paint=False )
        
        self.paint()

    def command_CursorPageDown( self, info ):

        step = self.height

        self.visible_first_line = min( self.visible_first_line+step, len(self.doc.lines)-1 )
        cursor = self.selection.cursor().down(step)

        self.setCursor( cursor, update_ideal_column=False, paint=False )

        self.paint()

    def command_CursorCorrespondingBracket( self, info ):
    
        cursor = self.selection.cursor().correspondingBracket(self.doc.mode.bracket)
        self.setCursor(cursor)

    def command_CursorModifiedLinePrev( self, info ):
        def func(line):
            return line.modified
        if not self.seek( -1, func ):
            self.setMessage( ckit_resource.strings["modified_line_not_found"], 1000, error=True )

    def command_CursorModifiedLineNext( self, info ):
        def func(line):
            return line.modified
        if not self.seek( +1, func ):
            self.setMessage( ckit_resource.strings["modified_line_not_found"], 1000, error=True )

    def command_CursorBookmarkPrev( self, info ):
        def func(line):
            return line.bookmark
        if not self.seek( -1, func ):
            self.setMessage( ckit_resource.strings["bookmark_not_found"], 1000, error=True )

    def command_CursorBookmarkNext( self, info ):
        def func(line):
            return line.bookmark
        if not self.seek( +1, func ):
            self.setMessage( ckit_resource.strings["bookmark_not_found"], 1000, error=True )

    def command_CursorModifiedOrBookmarkPrev( self, info ):
        def func(line):
            return line.modified or line.bookmark
        if not self.seek( -1, func ):
            self.setMessage( ckit_resource.strings["modified_line_or_bookmark_not_found"], 1000, error=True )

    def command_CursorModifiedOrBookmarkNext( self, info ):
        def func(line):
            return line.modified or line.bookmark
        if not self.seek( +1, func ):
            self.setMessage( ckit_resource.strings["modified_line_or_bookmark_not_found"], 1000, error=True )

    def command_CursorDocumentBegin( self, info ):

        cursor = self.pointDocumentBegin()

        self.setCursor(cursor)

    def command_CursorDocumentEnd( self, info ):

        cursor = self.pointDocumentEnd()

        self.setCursor(cursor)

    def command_ScrollUp( self, info ):
        self.scrollV(-3)

    def command_ScrollDown( self, info ):
        self.scrollV(3)

    def command_ScrollCursorCenter( self, info ):

        cursor = self.selection.cursor()

        self.visible_first_line = cursor.line - self.height//2
        self.visible_first_line = max( self.visible_first_line, 0 )

        self.makeVisible(cursor)

    def command_SelectLeft( self, info ):

        cursor = self.selection.cursor().left(block_mode=self.selection.block_mode)
        anchor = self.selection.anchor()

        self.setSelection(anchor,cursor)

    def command_SelectRight( self, info ):

        cursor = self.selection.cursor().right(block_mode=self.selection.block_mode)
        anchor = self.selection.anchor()

        self.setSelection(anchor,cursor)

    def command_SelectWordLeft( self, info ):

        cursor = self.selection.cursor().wordLeft()
        anchor = self.selection.anchor()

        self.setSelection(anchor,cursor)

    def command_SelectWordRight( self, info ):

        cursor = self.selection.cursor().wordRight()
        anchor = self.selection.anchor()

        self.setSelection(anchor,cursor)

    def command_SelectLineBegin( self, info ):

        cursor = self.selection.cursor().lineBegin()
        anchor = self.selection.anchor()

        self.setSelection(anchor,cursor)

    def command_SelectLineEnd( self, info ):

        cursor = self.selection.cursor().lineEnd()
        anchor = self.selection.anchor()

        self.setSelection(anchor,cursor)

    def command_SelectUp( self, info ):

        cursor = self.selection.cursor().up(block_mode=self.selection.block_mode)
        anchor = self.selection.anchor()

        self.setSelection( anchor, cursor, update_ideal_column=False )

    def command_SelectDown( self, info ):

        cursor = self.selection.cursor().down(block_mode=self.selection.block_mode)
        anchor = self.selection.anchor()

        self.setSelection( anchor, cursor, update_ideal_column=False )

    def command_SelectPageUp( self, info ):

        step = self.height

        cursor = self.selection.cursor().up( step, block_mode=self.selection.block_mode )
        anchor = self.selection.anchor()

        self.setSelection( anchor, cursor, update_ideal_column=False )

    def command_SelectPageDown( self, info ):

        step = self.height

        cursor = self.selection.cursor().down( step, block_mode=self.selection.block_mode )
        anchor = self.selection.anchor()

        self.setSelection( anchor, cursor, update_ideal_column=False )

    def command_SelectCorrespondingBracket( self, info ):
    
        cursor = self.selection.cursor().correspondingBracket(self.doc.mode.bracket)
        anchor = self.selection.anchor()

        self.setSelection(anchor,cursor)

    def command_SelectModifiedLinePrev( self, info ):

        def func(line):
            return line.modified

        cursor = self.seek( -1, func, move_cursor=False )
        anchor = self.selection.anchor()

        if cursor:
            self.setSelection(anchor,cursor)
        else:
            self.setMessage( ckit_resource.strings["modified_line_not_found"], 1000, error=True )

    def command_SelectModifiedLineNext( self, info ):

        def func(line):
            return line.modified

        cursor = self.seek( +1, func, move_cursor=False )
        anchor = self.selection.anchor()

        if cursor:
            self.setSelection(anchor,cursor)
        else:
            self.setMessage( ckit_resource.strings["modified_line_not_found"], 1000, error=True )

    def command_SelectBookmarkPrev( self, info ):

        def func(line):
            return line.bookmark

        cursor = self.seek( -1, func, move_cursor=False )
        anchor = self.selection.anchor()

        if cursor:
            self.setSelection(anchor,cursor)
        else:
            self.setMessage( ckit_resource.strings["bookmark_not_found"], 1000, error=True )

    def command_SelectBookmarkNext( self, info ):

        def func(line):
            return line.bookmark

        cursor = self.seek( +1, func, move_cursor=False )
        anchor = self.selection.anchor()

        if cursor:
            self.setSelection(anchor,cursor)
        else:
            self.setMessage( ckit_resource.strings["bookmark_not_found"], 1000, error=True )

    def command_SelectModifiedOrBookmarkPrev( self, info ):

        def func(line):
            return line.modified or line.bookmark

        cursor = self.seek( -1, func, move_cursor=False )
        anchor = self.selection.anchor()

        if cursor:
            self.setSelection(anchor,cursor)
        else:
            self.setMessage( ckit_resource.strings["modified_line_or_bookmark_not_found"], 1000, error=True )

    def command_SelectModifiedOrBookmarkNext( self, info ):

        def func(line):
            return line.modified or line.bookmark

        cursor = self.seek( +1, func, move_cursor=False )
        anchor = self.selection.anchor()

        if cursor:
            self.setSelection(anchor,cursor)
        else:
            self.setMessage( ckit_resource.strings["modified_line_or_bookmark_not_found"], 1000, error=True )

    def command_SelectDocumentBegin( self, info ):

        cursor = self.pointDocumentBegin()
        anchor = self.selection.anchor()

        self.setSelection(anchor,cursor)

    def command_SelectDocumentEnd( self, info ):

        cursor = self.pointDocumentEnd()
        anchor = self.selection.anchor()

        self.setSelection(anchor,cursor)

    def command_SelectDocument( self, info ):

        anchor = self.pointDocumentBegin()
        cursor = self.pointDocumentEnd()

        self.setSelection( anchor, cursor, block_mode=False )
        self.paint()

    def command_SelectCancel( self, info ):

        cursor = self.selection.cursor()

        self.setCursor(cursor)

    def command_SelectScrollUp( self, info ):

        old_cursor = self.selection.cursor()
        cursor = self.selection.cursor()
        for i in range(3):
            cursor = cursor.up(block_mode=self.selection.block_mode)
        anchor = self.selection.anchor()

        self.setSelection( anchor, cursor, update_ideal_column=False )

        self.scrollV( cursor.line - old_cursor.line )

    def command_SelectScrollDown( self, info ):

        old_cursor = self.selection.cursor()
        cursor = self.selection.cursor()
        for i in range(3):
            cursor = cursor.down(block_mode=self.selection.block_mode)

        anchor = self.selection.anchor()

        self.setSelection( anchor, cursor, update_ideal_column=False )

        self.scrollV( cursor.line - old_cursor.line )

    def command_SelectBlock( self, info ):

        cursor = self.selection.cursor()
        anchor = self.selection.anchor()

        self.setSelection( anchor, cursor, block_mode=True )

    def command_InsertReturn( self, info ):
        self.modifyText( text = "\n" )

    def command_InsertReturnAutoIndent( self, info ):

        left = self.selection.left()

        s = self.doc.lines[left.line].s[:left.index]
        s2 = s.lstrip(" \t")
        indent = s[ : len(s)-len(s2) ]

        self.modifyText( text = "\n" + indent )

    def command_InsertTab( self, info ):

        left = self.selection.left()

        if self.doc.mode.tab_by_space:
            tab_width = self.doc.mode.tab_width
            column1 = self.getColumnFromIndex( left.line, left.index )
            column2 = ( column1 + tab_width ) // tab_width * tab_width
            self.modifyText( text = " " * (column2-column1) )
        else:
            self.modifyText( text = "\t" )

    def command_Delete( self, info ):
        self.command_DeleteCharRight(info)

    def command_DeleteCharRight( self, info ):
        if self.selection.direction==0:
            left = self.selection.cursor()
            right = left.right()
            if left!=right:
                self.modifyText( right, left, "" )
        else:
            self.modifyText( text = "" )

    def command_DeleteCharLeft( self, info ):
        if self.selection.direction==0:
            right = self.selection.cursor()
            left = right.left()
            if left!=right:
                self.modifyText( left, right, "" )
        else:
            self.modifyText( text = "" )

    def command_DeleteWord( self, info ):
        if self.selection.direction==0:
            cursor = self.selection.cursor()
            right = cursor.wordRight(False)
            left = right.wordLeft(False)
            if left!=right:
                self.modifyText( right, left, "" )
        else:
            self.modifyText( text = "" )

    def command_DeleteWordRight( self, info ):
        if self.selection.direction==0:
            left = self.selection.cursor()
            right = left.wordRight(False)
            if left!=right:
                self.modifyText( right, left, "" )
        else:
            self.modifyText( text = "" )

    def command_DeleteWordLeft( self, info ):
        if self.selection.direction==0:
            right = self.selection.cursor()
            left = right.wordLeft(False)
            if left!=right:
                self.modifyText( left, right, "" )
        else:
            self.modifyText( text = "" )

    def command_DeleteLine( self, info ):
        if self.selection.direction==0:
            cursor = self.selection.cursor()
            left = cursor.lineBegin()
            right = cursor.lineEnd()
            if left==right:
                right = right.right()
            if left!=right:
                self.modifyText( right, left, "" )
        else:
            self.modifyText( text = "" )

    def command_DeleteLineRight( self, info ):
        if self.selection.direction==0:
            left = self.selection.cursor()
            right = left.lineEnd()
            if left==right:
                right = right.right()
            if left!=right:
                self.modifyText( right, left, "" )
        else:
            self.modifyText( text = "" )

    def command_DeleteLineLeft( self, info ):
        if self.selection.direction==0:
            right = self.selection.cursor()
            left = right.lineBegin()
            if left==right:
                left = left.left()
            if left!=right:
                self.modifyText( left, right, "" )
        else:
            self.modifyText( text = "" )

    def command_Copy( self, info ):
        if self.selection.direction==0:
            if self.doc.mode.copy_line_if_not_selected:
                left = self.selection.cursor().lineBegin()
                right = left.lineEnd().right()
                text = self.getText( left, right )
                ckit_misc.setClipboardText(text)
        else:
            text = self.getText( self.selection.left(), self.selection.right(), self.selection.block_mode )
            ckit_misc.setClipboardText(text)
            if self.selection.block_mode:
                TextWidget.rect_mode_clipboard_number = ckit_misc.getClipboardSequenceNumber()

            if self.doc.mode.cancel_selection_on_copy:
                cursor = self.selection.cursor()
                self.setCursor(cursor)

    def command_Cut( self, info ):
        if self.selection.direction==0:
            if self.doc.mode.copy_line_if_not_selected:
                left = self.selection.cursor().lineBegin()
                right = left.lineEnd().right()
                text = self.getText( left, right )
                ckit_misc.setClipboardText(text)
                self.modifyText( right, left, text="" )
        else:
            text = self.getText( self.selection.left(), self.selection.right(), self.selection.block_mode )
            ckit_misc.setClipboardText(text)
            if self.selection.block_mode:
                TextWidget.rect_mode_clipboard_number = ckit_misc.getClipboardSequenceNumber()

            self.modifyText( text = "" )

    def command_Paste( self, info ):
        text = ckit_misc.getClipboardText()
        if text:
            text_block_mode = self.selection.block_mode or (TextWidget.rect_mode_clipboard_number == ckit_misc.getClipboardSequenceNumber())
            self.modifyText( text = text, text_block_mode=text_block_mode )

    def command_Undo( self, info ):
        if not self.undo():
            ckit_misc.messageBeep()

    def command_Redo( self, info ):
        if not self.redo():
            ckit_misc.messageBeep()

    def command_IndentSelection( self, info ):

        left = self.selection.left()
        right = self.selection.right()
        if left.line==right.line:
            info.result = False
            return

        def func(s):
            if s.lstrip(" \t"):
                if self.doc.mode.tab_by_space:
                    return " " * self.doc.mode.tab_width + s
                else:
                    return "\t" + s
            return s

        self.replaceLines(func)

    def command_UnindentSelection( self, info ):

        left = self.selection.left()
        right = self.selection.right()
        if left.line==right.line:
            info.result = False
            return

        def func(s):
            for i in range(self.doc.mode.tab_width+1):
                if i>=len(s) : break
                if s[i]=='\t':
                    i+=1
                    break
                if s[i]!=' ':
                    break
            return s[i:]

        self.replaceLines(func)

    def command_CompleteAbbrev( self, info ):
        self.popupList()

    def command_CloseList( self, info ):
        if self.isListOpened():
            self.closeList()
        else:
            info.result=False

    def command_Bookmark1( self, info ):
        cursor = self.selection.cursor()
        self.bookmark( cursor.line, [ 1, 0 ] )
            
    def command_Bookmark2( self, info ):
        cursor = self.selection.cursor()
        self.bookmark( cursor.line, [ 2, 0 ] )
            
    def command_Bookmark3( self, info ):
        cursor = self.selection.cursor()
        self.bookmark( cursor.line, [ 3, 0 ] )

    def command_Bookmark123( self, info ):
        cursor = self.selection.cursor()
        self.bookmark( cursor.line, [ 1, 2, 3, 0 ] )

    def command_DebugOffload( self, info ):
        Line.offload( self.doc.lines )

    def command_DebugGc( self, info ):
        gc.collect()

#--------------------------------------------------------------------

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

    def onActivate( self, active ):
        if active:
            self.parent_window.activate()

    def onKeyDown( self, vk, mod ):
        if self.keydown_hook:
            if self.keydown_hook( vk, mod ):
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

    def executeCommand( self, name, info ):

        try:
            command = getattr( self, "command_" + name )
        except AttributeError:
            return False
        
        command(info)
        return True

    def enumCommand(self):
        for attr in dir(self):
            if attr.startswith("command_"):
                yield attr[ len("command_") : ]

    #--------------------------------------------------------
    # ここから下のメソッドはキーに割り当てることができる
    #--------------------------------------------------------

    def command_CursorUp( self, info ):
        if not len(self.items) : return
        if self.select>=0:
            self.select -= 1
            if self.select<0 : self.select=0
        else:
            self.select=len(self.items)-1
        self.scroll_info.makeVisible( self.select, self.height() )

        if self.selchange_handler:
            self.selchange_handler( self.select, self.items[self.select] )

        self.paint()

    def command_CursorDown( self, info ):
        if not len(self.items) : return
        self.select += 1
        if self.select>len(self.items)-1 : self.select=len(self.items)-1
        self.scroll_info.makeVisible( self.select, self.height() )

        if self.selchange_handler:
            self.selchange_handler( self.select, self.items[self.select] )

        self.paint()

    def command_CursorPageUp( self, info ):
        if not len(self.items) : return
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

    def command_CursorPageDown( self, info ):
        if not len(self.items) : return
        if self.select<self.scroll_info.pos+self.height()-1:
            self.select = self.scroll_info.pos+self.height()-1
        else:
            self.select += self.height()
        if self.select>len(self.items)-1 : self.select=len(self.items)-1
        self.scroll_info.makeVisible( self.select, self.height() )

        if self.selchange_handler:
            self.selchange_handler( self.select, self.items[self.select] )

        self.paint()

## @} textwidget

