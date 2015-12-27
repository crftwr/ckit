import os
import traceback

from ckit import ckit_resource

## @addtogroup userconfig
## @{

## 設定スクリプトを再読み込みする
#
#  @param filename  ファイル名
#
def reloadConfigScript( filename ):
    
    global user_namespace

    try:
        fd = open( filename, 'rb' )
        fileimage = fd.read()
        user_namespace = {}
        code = compile( fileimage, os.path.basename(filename), 'exec' )
        exec( code, user_namespace, user_namespace )
    except:
        print( ckit_resource.strings["error_config_file_load_failed"] )
        traceback.print_exc()

## 設定スクリプトの関数を呼び出す
#
#  @param funcname  関数の名前
#  @param args      引数
#
def callConfigFunc( funcname, *args ):

    global user_namespace

    try:
        func = user_namespace[funcname]
    except KeyError:
        return

    try:
        func(*args)
    except:
        print( ckit_resource.strings["error_config_file_exec_failed"] )
        traceback.print_exc()

## @} userconfig
