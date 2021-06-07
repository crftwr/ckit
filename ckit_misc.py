import sys
import os
import shutil
import ctypes
import ctypes.wintypes
import msvcrt
import inspect

import pyauto

## @addtogroup misc
## @{

#--------------------------------------------------------------------

_use_slash = False
_drive_case_upper = None

#--------------------------------------------------------------------

class ScrollInfo:

    def __init__( self ):
        self.pos = 0

    def makeVisible( self, index, visible_height, margin=0 ):
        if margin > visible_height//2 : margin = visible_height//2
        if index < self.pos + margin :
            self.pos = index - margin
        elif index > self.pos + visible_height - 1 - margin :
            self.pos = index - visible_height + 1 + margin
        if self.pos<0 : self.pos=0

#--------------------------------------------------------------------

# 引数が少ない関数を許容しながら呼び出す
def callFlexArg( func, *args ):

    if type(func).__name__=='instance':
        argspec = inspect.getargspec(func.__call__)
        num_args = len(argspec[0])-1
    elif inspect.ismethod(func):
        argspec = inspect.getargspec(func)
        num_args = len(argspec[0])-1
    else:
        argspec = inspect.getargspec(func)
        num_args = len(argspec[0])

    args = args[ : num_args ]
    func( *args )

#--------------------------------------------------------------------

## テキストのエンコーディングを表すクラス
class TextEncoding:

    def __init__( self, encoding='utf-8', bom=None ):
        self.encoding = encoding
        self.bom = bom

    def __str__(self):
        if self.encoding=='utf-8' and self.bom==None:
            return 'utf-8n'
        if self.encoding=='cp932':
            return 'shift-jis'
        return self.encoding

## テキストのエンコードを検出する
#
#  @param data 検出する文字列
#  @param maxlen 調査する最大byte数
#  @param maxline 調査する最大行数
#  @param ascii_as asciiだと検出された場合の代替のエンコーディング
#  @return 検出結果を格納した TextEncoding オブジェクト
#
#  渡されたデータのエンコーディングを推測します。
#  以下のエンコーディングの可能性を調査します。
#
#  - ascii
#  - cp932
#  - euc-jp
#  - iso-2022-jp
#  - utf-8
#  - utf-16-le
#  - utf-16-be
#
#  またバイナリデータであると推測された場合は、TextEncoding の encoding には None が入ります。
#
def detectTextEncoding( data, maxlen=1024*1024, maxline=1000, ascii_as=None ):

    result = TextEncoding()

    if data[:3] == b"\xEF\xBB\xBF":
        result.encoding = 'utf-8'
        result.bom = b"\xEF\xBB\xBF"
        return result
    elif data[:2] == b"\xFF\xFE":
        result.encoding = 'utf-16-le'
        result.bom = b"\xFF\xFE"
        return result
    elif data[:2] == b"\xFE\xFF":
        result.encoding = 'utf-16-be'
        result.bom = b"\xFE\xFF"
        return result

    if data.find(b'\0')>=0 :
        result.encoding = None
        return result

    if len(data) > maxlen:
        data = data[:maxlen]

    lines = data.splitlines()

    encoding_list = [
        [ 'ascii', 0 ],
        [ 'cp932', 0 ],
        [ 'euc-jp', 0 ],
        [ 'iso-2022-jp', 0 ],
        [ 'utf-8', 0 ],
    ]

    numline_read = 0

    for line in lines:

        for encoding in encoding_list:
            try:
                uniode_line = line.decode(encoding=encoding[0])
            except UnicodeDecodeError:
                encoding[1] -= 1
            else:
                if len(uniode_line)<len(line):
                    encoding[1] += 1

        numline_read += 1
        if numline_read >= maxline : break

    best_encoding = [ None, -1-numline_read//100 ]

    for encoding in encoding_list:
        if encoding[1]>best_encoding[1]:
            best_encoding = encoding
            
    result.encoding = best_encoding[0]

    if result.encoding=='ascii' and ascii_as:
        result.encoding = ascii_as

    return result

## 文字列中の最初のBOMを除去して返す
def removeBom(s):
    return s.lstrip( '\ufeff\ufffe' )

#--------------------------------------------------------------------

_argv = None

## Unicode 形式で sys.argv 相当の情報を取得する
def getArgv():

    global _argv
    
    if _argv==None:
    
        _argv = []

        from ctypes import POINTER, byref, cdll, c_int, windll
        from ctypes.wintypes import LPCWSTR, LPWSTR

        GetCommandLineW = cdll.kernel32.GetCommandLineW
        GetCommandLineW.argtypes = []
        GetCommandLineW.restype = LPCWSTR

        CommandLineToArgvW = windll.shell32.CommandLineToArgvW
        CommandLineToArgvW.argtypes = [LPCWSTR, POINTER(c_int)]
        CommandLineToArgvW.restype = POINTER(LPWSTR)

        cmd = GetCommandLineW()
        argc = c_int(0)
        argv = CommandLineToArgvW(cmd, byref(argc))
        if argc.value > 0:
            # Remove Python executable and commands if present
            start = argc.value - len(sys.argv)
            for i in range(start, argc.value):
                _argv.append(argv[i])
    
    return _argv

## デスクトップディレクトリを取得する
def getDesktopPath():

    MAX_PATH = 260
    CSIDL_DESKTOPDIRECTORY = 0x10

    buf = ctypes.create_unicode_buffer(MAX_PATH)
    ctypes.windll.shell32.SHGetSpecialFolderPathW( None, buf, CSIDL_DESKTOPDIRECTORY, 0 )

    return buf.value

## アプリケーションの実行ファイルのパスを取得する
def getAppExePath():
    return os.path.split(getArgv()[0])[0]

## アプリケーションのデータディレクトリのパスを取得する
def getAppDataPath():

    MAX_PATH = 260
    CSIDL_APPDATA = 26

    buf = ctypes.create_unicode_buffer(MAX_PATH)
    ctypes.windll.shell32.SHGetSpecialFolderPathW( None, buf, CSIDL_APPDATA, 0 )

    return buf.value

## ドキュメントディレクトリのパスを取得する
def getDocumentsPath():

    MAX_PATH = 260
    CSIDL_PERSONAL = 5

    buf = ctypes.create_unicode_buffer(MAX_PATH)
    ctypes.windll.shell32.SHGetSpecialFolderPathW( None, buf, CSIDL_PERSONAL, 0 )

    return buf.value

## ユーザプロファイルディレクトリのパスを取得する
def getProfilePath():

    MAX_PATH = 260
    CSIDL_PROFILE = 40

    buf = ctypes.create_unicode_buffer(MAX_PATH)
    ctypes.windll.shell32.SHGetSpecialFolderPathW( None, buf, CSIDL_PROFILE, 0 )

    return buf.value

## テンポラリファイル用ディレクトリのパスを取得する
def getTempPath():

    MAX_PATH = 260

    buf = ctypes.create_unicode_buffer(MAX_PATH)
    ctypes.windll.kernel32.GetTempPathW( MAX_PATH, buf )

    return buf.value

_data_path = None

## アプリケーションのデータ格納用のパスを取得する
# @sa setDataPath
def dataPath():
    return _data_path

## アプリケーションのデータ格納用のパスを設定する
# @sa dataPath
def setDataPath( data_path ):
    global _data_path
    _data_path = data_path

## 利用可能なドライブ文字を連結した文字列を取得する
def getDrives():
    drives = ""
    drive_bits = ctypes.windll.kernel32.GetLogicalDrives()
    for i in range(26):
        if drive_bits & (1<<i):
            drives += chr(ord('A')+i)
    return drives

## ドライブの種別を取得する
def getDriveType(drive):

    DRIVE_TYPE_UNDTERMINED = 0
    DRIVE_ROOT_NOT_EXIST = 1
    DRIVE_REMOVABLE = 2
    DRIVE_FIXED = 3
    DRIVE_REMOTE = 4
    DRIVE_CDROM = 5
    DRIVE_RAMDISK = 6

    drive_type = ctypes.windll.kernel32.GetDriveTypeW(drive)

    if drive_type==DRIVE_TYPE_UNDTERMINED:
        return ""
    elif drive_type==DRIVE_ROOT_NOT_EXIST:
        return ""
    elif drive_type==DRIVE_REMOVABLE:
        return "Removable"
    elif drive_type==DRIVE_FIXED:
        return "HDD"
    elif drive_type==DRIVE_REMOTE:
        return "Network"
    elif drive_type==DRIVE_CDROM:
        return "CD/DVD"
    elif drive_type==DRIVE_RAMDISK:
        return "Ramdisk"
    return ""


## ドライブの表示用の文字列を取得する
#
#  "C:\" の形式で渡す必要があります
#
def getDriveDisplayName(drive):

    MAX_PATH = 260

    class SHFILEINFO(ctypes.Structure):
        _fields_ = [("hIcon", ctypes.wintypes.HICON),
                    ("iIcon", ctypes.c_int),
                    ("dwAttributes", ctypes.wintypes.DWORD),
                    ("szDisplayName", ctypes.c_wchar * MAX_PATH),
                    ("szTypeName", ctypes.c_wchar * 80)]

    SHGFI_ICON              = 0x000000100
    SHGFI_DISPLAYNAME       = 0x000000200
    SHGFI_TYPENAME          = 0x000000400
    SHGFI_ATTRIBUTES        = 0x000000800
    SHGFI_ICONLOCATION      = 0x000001000
    SHGFI_EXETYPE           = 0x000002000
    SHGFI_SYSICONINDEX      = 0x000004000
    SHGFI_LINKOVERLAY       = 0x000008000
    SHGFI_SELECTED          = 0x000010000
    SHGFI_ATTR_SPECIFIED    = 0x000020000
    SHGFI_LARGEICON         = 0x000000000
    SHGFI_SMALLICON         = 0x000000001
    SHGFI_OPENICON          = 0x000000002
    SHGFI_SHELLICONSIZE     = 0x000000004
    SHGFI_PIDL              = 0x000000008
    SHGFI_USEFILEATTRIBUTES = 0x000000010

    shfileinfo = SHFILEINFO()

    flags = SHGFI_DISPLAYNAME
    ctypes.windll.shell32.SHGetFileInfoW(
        drive,
        0,
        ctypes.byref(shfileinfo),
        ctypes.sizeof(shfileinfo),
        flags
        )

    return shfileinfo.szDisplayName


FILE_ATTRIBUTE_READONLY         = 0x00000001
FILE_ATTRIBUTE_HIDDEN           = 0x00000002
FILE_ATTRIBUTE_SYSTEM           = 0x00000004
FILE_ATTRIBUTE_DIRECTORY        = 0x00000010
FILE_ATTRIBUTE_ARCHIVE          = 0x00000020
FILE_ATTRIBUTE_DEVICE           = 0x00000040
FILE_ATTRIBUTE_NORMAL           = 0x00000080
FILE_ATTRIBUTE_REPARSE_POINT    = 0x00000400

## ファイルの属性を取得する
def getFileAttribute(filename):
    return ctypes.windll.kernel32.GetFileAttributesW(filename)


## ファイルの属性を設定する
def setFileAttribute(filename,attribute):
    return ctypes.windll.kernel32.SetFileAttributesW(filename,attribute)


## ゴミ箱を使ってファイルを削除する
def deleteFilesUsingRecycleBin( hwnd, filename_list ):

    class SHFILEOPSTRUCT(ctypes.Structure):
        _fields_ = [("hwnd", ctypes.wintypes.HWND),
                    ("wFunc", ctypes.wintypes.UINT),
                    ("pFrom", ctypes.c_wchar_p),
                    ("pTo", ctypes.c_wchar_p),
                    ("fFlags", ctypes.wintypes.WORD),
                    ("fAnyOperationsAborted", ctypes.wintypes.BOOL),
                    ("hNameMappings", ctypes.c_void_p),
                    ("lpszProgressTitle", ctypes.c_wchar_p)]

    joint_filenames = ""
    for filename in filename_list:
        filename = filename.replace("/","\\")
        joint_filenames += filename
        joint_filenames += '\0'
    joint_filenames += '\0'
    c_joint_filenames = ctypes.create_unicode_buffer(joint_filenames)

    fileopdata = SHFILEOPSTRUCT()
    
    FO_MOVE	  = 1
    FO_COPY   = 2
    FO_DELETE = 3
    FO_RENAME = 4

    FOF_MULTIDESTFILES	        = 1
    FOF_CONFIRMMOUSE	        = 2
    FOF_SILENT	                = 4
    FOF_RENAMEONCOLLISION	    = 8
    FOF_NOCONFIRMATION	        = 16
    FOF_WANTMAPPINGHANDLE	    = 32
    FOF_ALLOWUNDO	            = 64
    FOF_FILESONLY	            = 128
    FOF_SIMPLEPROGRESS	        = 256
    FOF_NOCONFIRMMKDIR	        = 512
    FOF_NOERRORUI	            = 1024
    FOF_NOCOPYSECURITYATTRIBS   = 2048
    
    fileopdata.hwnd = hwnd
    fileopdata.wFunc = FO_DELETE
    fileopdata.pFrom = ctypes.cast( ctypes.byref(c_joint_filenames), ctypes.c_wchar_p )
    fileopdata.pTo = 0
    fileopdata.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION
    #fileopdata.fFlags = FOF_ALLOWUNDO
    fileopdata.fAnyOperationsAborted = 0
    fileopdata.hNameMappings = 0
    fileopdata.lpszProgressTitle = 0;

    return ctypes.windll.shell32.SHFileOperationW(
        ctypes.byref(fileopdata)
        )


## ディスクの空き容量と全体の容量を取得する
def getDiskSize(drive):
    free_size_for_user = ctypes.c_longlong()
    total_size = ctypes.c_longlong()
    free_size = ctypes.c_longlong()
    ctypes.windll.kernel32.GetDiskFreeSpaceExW( drive, ctypes.byref(free_size_for_user), ctypes.byref(total_size), ctypes.byref(free_size) )

    return (free_size.value,total_size.value)

#--------------------------------------------------------------------

CF_UNICODETEXT = 13
GHND = 66

## クリップボードのテキストを取得する
#
#  @return クリップボードから取得した文字列
#
def getClipboardText():

    ctypes.windll.user32.GetClipboardData.restype = ctypes.c_void_p
    ctypes.windll.kernel32.GlobalLock.restype = ctypes.c_wchar_p

    text = ""
    if ctypes.windll.user32.OpenClipboard(ctypes.c_int(0)):
        hClipMem = ctypes.windll.user32.GetClipboardData(ctypes.c_int(CF_UNICODETEXT))
        text = ctypes.windll.kernel32.GlobalLock(ctypes.c_void_p(hClipMem))
        ctypes.windll.kernel32.GlobalUnlock(ctypes.c_void_p(hClipMem))
        ctypes.windll.user32.CloseClipboard()
    if text==None:
        text = ""
    return text

## クリップボードにテキストを設定する
#
#  @param text  クリップボードに設定する文字列
#
def setClipboardText(text):

    ctypes.windll.kernel32.GlobalAlloc.restype = ctypes.c_void_p
    ctypes.windll.kernel32.GlobalLock.restype = ctypes.c_void_p

    bufferSize = (len(text)+1)*2
    hGlobalMem = ctypes.windll.kernel32.GlobalAlloc(ctypes.c_uint(GHND), ctypes.c_size_t(bufferSize))
    lpGlobalMem = ctypes.windll.kernel32.GlobalLock(ctypes.c_void_p(hGlobalMem))
    ctypes.cdll.msvcrt.memcpy( ctypes.c_void_p(lpGlobalMem), ctypes.c_wchar_p(text), ctypes.c_int(bufferSize))
    ctypes.windll.kernel32.GlobalUnlock(ctypes.c_void_p(hGlobalMem))
    if ctypes.windll.user32.OpenClipboard(0):
        ctypes.windll.user32.EmptyClipboard()
        ctypes.windll.user32.SetClipboardData( ctypes.c_int(CF_UNICODETEXT), ctypes.c_void_p(hGlobalMem) )
        ctypes.windll.user32.CloseClipboard()

## クリップボードのシーケンスナンバーを取得する
#
def getClipboardSequenceNumber():
    return ctypes.windll.user32.GetClipboardSequenceNumber()

#--------------------------------------------------------------------

## ファイルが他のプロセスから変更されないように共有ロックするためのクラス
#
class FileReaderLock:
    
    class OVERLAPPED(ctypes.Structure):
        _fields_ = [('Internal', ctypes.wintypes.LPVOID),
                    ('InternalHigh', ctypes.wintypes.LPVOID),
                    ('Offset', ctypes.wintypes.DWORD),
                    ('OffsetHigh', ctypes.wintypes.DWORD),
                    #('Pointer', ctypes.wintypes.LPVOID),
                    ('hEvent', ctypes.wintypes.HANDLE),
                    ]
                
    ## コンストラクタ
    #
    #  @param fileno  ロックしたいファイルのfileno
    #
    def __init__( self, fileno ):
    
        self.handle = msvcrt.get_osfhandle(fileno)

        LOCKFILE_FAIL_IMMEDIATELY = 0x00000001
        LOCKFILE_EXCLUSIVE_LOCK = 0x00000002

        overlapped = FileReaderLock.OVERLAPPED()

        result = ctypes.windll.kernel32.LockFileEx(
            self.handle,
            LOCKFILE_FAIL_IMMEDIATELY,
            0,
            0xffffffff,
            0xffffffff,
            ctypes.byref(overlapped),
            )

        if result==0:
            e = ctypes.WinError()
            print(e)
            raise e
            #raise ctypes.WinError()
            
        self.locked = True

    ## ロック解除
    #
    def unlock(self):
        
        if not self.locked: return

        overlapped = FileReaderLock.OVERLAPPED()

        result = ctypes.windll.kernel32.UnlockFileEx(
            self.handle,
            0,
            0xffffffff,
            0xffffffff,
            ctypes.byref(overlapped),
            )

        if result == 0:
            raise ctypes.WinError()
            
        self.locked = False

#--------------------------------------------------------------------

WM_CLOSE = 16

## 指定したプロセスIDのプロセスを終了する
def terminateProcess(pid):

    def callback( wnd, arg ):
        wnd_pid = ctypes.c_int(0)
        ctypes.windll.user32.GetWindowThreadProcessId( wnd.getHWND(), ctypes.byref(wnd_pid) )
        if wnd_pid.value==pid:
            wnd.sendMessage( WM_CLOSE, 0, 0 )
            return False
        return True
    pyauto.Window.enum( callback, None )

#--------------------------------------------------------------------

ELLIPSIS_NONE  = 0
ELLIPSIS_RIGHT = 1
ELLIPSIS_MID   = 2

ALIGN_LEFT   = 0
ALIGN_CENTER = 1
ALIGN_RIGHT  = 2

## 文字列を指定した長さに調節する
def adjustStringWidth( window, s, width, align=ALIGN_LEFT, ellipsis=ELLIPSIS_NONE ):

    if ellipsis==ELLIPSIS_RIGHT:
        original_width = window.getStringWidth(s)
        if original_width>width:
            str_width = 0
            pos = 0
            while True:
                char_width = window.getStringWidth(s[pos])
                if str_width + char_width >= width-1 :
                    if str_width + char_width == width-1:
                        return "%s\u22ef" % (s[:pos+1])
                    else:
                        return "%s\u22ef " % (s[:pos])
                str_width += char_width
                pos += 1

    elif ellipsis==ELLIPSIS_MID:

        original_width = window.getStringWidth(s)
        if original_width>width:
            left_width = (width-1)//2
            str_width = 0
            pos = 0
            while True:
                char_width = window.getStringWidth(s[pos])
                if str_width + char_width >= left_width :
                    if str_width + char_width == left_width:
                        str_width += char_width
                    break
                str_width += char_width
                pos += 1
            left_string = s[:pos]

            right_width = width-str_width-1
            str_width = 0
            pos = len(s)
            while True:
                pos -= 1
                char_width = window.getStringWidth(s[pos])
                if str_width + char_width >= right_width :
                    if str_width + char_width == right_width:
                        str_width += char_width
                    break
                str_width += char_width
            right_string = s[pos:]
            return "%s\u22ef%s" % (left_string,right_string)

    elif ellipsis==ELLIPSIS_NONE:
        original_width = window.getStringWidth(s)
        if original_width>width:
            left_width = width
            pos = 0
            while True:
                left_width -= window.getStringWidth(s[pos])
                if left_width < 0 : break
                pos += 1
            return s[:pos]

    else:
        assert(0)

    if align==ALIGN_LEFT:
        return s + ' '*( width - original_width )

    elif align==ALIGN_RIGHT:
        return ' '*( width - original_width ) + s

    elif align==ALIGN_CENTER:
        delta = width - original_width
        left_space = delta//2
        right_space = delta - left_space
        return ' '*left_space + s + ' '*right_space

    else:
        assert(0)

## 文字列を改行コードの位置と、決まった幅の位置で分割する
def splitLines( window, src, width, keepends=False ):
    lines = []
    for line in src.splitlines(keepends):
        while window.getStringWidth(line)>width:
            w = 0
            i = 0
            while True:
                char_width = window.getStringWidth(line[i])
                if w + char_width > width:
                    lines.append( line[:i] )
                    line = line[i:]
                    break
                w += char_width
                i += 1     
        else:
            lines.append(line)
    return lines

## TAB文字をスペース文字に置き換える
def expandTab( window, src, tab_width=4, offset=0 ):
    dst = ""
    dst_len = offset
    pos = 0
    while 1:
        new_pos = src.find('\t',pos)
        if new_pos<0:
            dst += src[pos:]
            break
        src_part = src[ pos : new_pos ]
        dst += src_part
        dst_len += window.getStringWidth(src_part)
        dst += ' '
        dst_len += 1
        while dst_len%tab_width :
            dst += ' '
            dst_len += 1
        pos = new_pos+1
    return dst

#--------------------------------------------------------------------

## 単語区切り位置を検索するためのクラス
class WordBreak:

    def __init__( self, char_types, type_bounds ):

        self.char_types = char_types
        self.type_bounds = type_bounds
        
    def _charType( self, c ):

        for i in range(len(self.char_types)):
            if c in self.char_types[i]:
                return i
        return -1

    def __call__( self, s, pos, step, use_type_bounds=True ):

        if step>0:
            pos += 1
            if pos>=len(s) : return len(s)
        else:
            pos -= 1
            if pos<=0 : return 0

        left_char_type = self._charType(s[pos-1])
        right_char_type = self._charType(s[pos])
    
        while True:
       
            if left_char_type!=right_char_type:
                if not use_type_bounds or right_char_type in self.type_bounds[left_char_type]:
                    return pos

            if step>0:
                pos += 1
                if pos>=len(s) : return len(s)
                left_char_type = right_char_type
                right_char_type = self._charType(s[pos])
            else:
                pos -= 1
                if pos<=0 : return 0
                right_char_type = left_char_type
                left_char_type = self._charType(s[pos-1])

## Unicode の範囲を定義するためのクラス
class UnicodeRange:
    
    def __init__( self, start, end ):
        self.start = start
        self.end = end
    
    def __contains__( self, item ):
        return self.start <= item <= self.end


## テキストファイル用の単語区切り
wordbreak_TextFile = WordBreak(
    ( 
        "\"!@#$%^&*()+|~-=\`[]{};:',./<>?",
        " \t",
    ),
    {
        -1 : [ 0 ],
         0 : [ -1 ],
         1 : [ -1, 0 ],
    }
)

## ファイル名用の単語区切り
wordbreak_Filename = WordBreak(
    ( 
        "\"!@#$%^&*()+|~-=\`[]{};:',./<>?",
        " \t",
    ),
    {
        -1 : [ ],
         0 : [ -1, 1 ],
         1 : [ -1, 0 ],
    }
)

#--------------------------------------------------------------------

## ファイルパスのディレクトリ区切り文字を ￥と / で切り替える
#
#  @param use_slash     True:/を使用する False:￥を使用する
#
#  normPath や joinPath で使用されるディレクトリ区切り文字を設定します。
#
def setPathSlash(use_slash):
    global _use_slash
    _use_slash = use_slash
   
## ファイルパスのディレクトリ区切り文字を取得する
#
#  @return True:/ False:￥
#
#  normPath や joinPath で使用されるディレクトリ区切り文字を取得します。
#
#  @sa setPathSlash
#
def pathSlash():
    return _use_slash

def setPathDriveUpper(drive_case_upper):
    global _drive_case_upper
    _drive_case_upper = drive_case_upper
   
def pathDriveUpper():
    return _drive_case_upper

## ファイルパスを連結する
#
#  @param *args     連結される任意の個数のパス
#  @return          連結結果のパス
#
#  ファイルパスの連結処理に加えて、ディレクトリ区切り文字の \\ と / の置き換え処理も行います。
#
def joinPath(*args):
    path = os.path.join( *args )
    path = replacePath(path)
    return path

## ファイルパスをディレクトリ名とファイル名に分離する
#
#  @param path     分離されるパス
#  @return         ( ディレクトリ名, ファイル名 )
#
#  動作は os.path.split と同じです。
#
def splitPath(path):
    return os.path.split( path )


## 拡張子がN文字以下でASCII文字だけからなるときだけ分離する
#
#  @param path     分離されるパス
#  @param maxlen   拡張子とみなす最大の長さ (ピリオドを含む)
#  @return         ( ファイル名本体, 拡張子 )
#
#  この関数の動きは os.path.splitext() に似せてありますが、
#  os.path.splitext() とは違い、長い拡張子を分離しないことが出来ます。
#
def splitExt( path, maxlen=5 ):
    last_period_pos = path.rfind(".")
    if last_period_pos>0 and last_period_pos>=len(path)-maxlen:
        for i in range( last_period_pos+1, len(path) ):
            if path[i]>='\u0100' : return path, None
        return path[:last_period_pos], path[last_period_pos:]
    return path, None

## ファイルパスのルートディレクトリ部分を取得する
#
#  @param path     元になるパス
#  @return         ルートディレクトリのパス
#
def rootPath(path):
    while True:
        new_path, name = os.path.split(path)
        if new_path == path : return path
        path = new_path

## ファイルパスを標準化する
#
#  @param path     元になるパス
#  @return         標準化されたのパス
#
#  os.path.normpath() 相当の処理に加え、ディレクトリ区切り文字の \\ と / の置き換え処理を行います。
#
def normPath(path):
    path = os.path.normpath(path)
    path = replacePath(path)
    return path

## ディレクトリ区切り文字とドライブ名の設定にしたがってパスを整形する
#
#  @param path     元になるパス
#  @return         置換されたパス
#
#  ディレクトリ区切り文字の \\ と / の置き換え処理を行います。
#  ドライブ名の大文字/小文字の変換処理を行います。
#
def replacePath(path):

    if _use_slash:
        path = path.replace("\\","/")
    else:
        path = path.replace("/","\\")
        
    if _drive_case_upper==True:
        drive, tail = os.path.splitdrive(path)
        if drive.endswith(":"):
            drive = drive.upper()
            path = os.path.join(drive, tail)
        
    elif _drive_case_upper==False:
        drive, tail = os.path.splitdrive(path)
        if drive.endswith(":"):
            drive = drive.lower()
            path = os.path.join(drive, tail)
    
    return path

#--------------------------------------------------------------------

import winsound

beep_enabled = True

## 警告音を有効/無効にする
def enableBeep(enable):
    global beep_enabled
    beep_enabled = enable

## 警告音を再生する
def messageBeep():
    if beep_enabled:
        winsound.MessageBeep()

#--------------------------------------------------------------------

import tempfile

_temp_num = 1
_temp_dir = ""

def initTemp( prefix ):
    global _temp_dir
    _temp_dir = tempfile.mkdtemp( prefix=prefix )

def destroyTemp():
    shutil.rmtree( _temp_dir, ignore_errors=True )

## テンポラリディレクトリを作成する
def makeTempDir(prefix):
    global _temp_num
    while 1:
        path = os.path.join( _temp_dir, prefix+"%08x"%_temp_num )
        _temp_num += 1
        if not os.path.exists(path) : break
    os.mkdir(path)
    return path

## テンポラリファイルを作成する
def makeTempFile(prefix,suffix=""):
    global _temp_num
    while 1:
        path = os.path.join( _temp_dir, prefix+"%08x"%_temp_num+suffix )
        _temp_num += 1
        if not os.path.exists(path) : break
    fd = open(path,"wb")
    fd.close()
    return path

#--------------------------------------------------------------------

## @} misc
