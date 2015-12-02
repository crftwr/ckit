import sys
import time
import subprocess

from ckit import ckit_misc

## @addtogroup subprocess サブプロセス実行機能
## @{

#-------------------------------------------------------------------------

## コンソールプログラムをサブプロセスとして実行するためのクラス
#
#  任意のコンソールプログラムを、ファイラのサブプロセスとして実行し、そのプログラムの出力を、ログペインにリダイレクトします。
#
#  SubProcessオブジェクトは __call__メソッドを持っており、関数のように呼び出すことができます。
#
class SubProcess:

    ## コンストラクタ
    #
    #  @param self              -
    #  @param cmd               コマンドと引数のシーケンス
    #  @param cwd               サブプロセスのカレントディレクトリ
    #  @param env               サブプロセスの環境変数
    #  @param stdout_write      サブプロセスの標準出力をリダイレクトするためのwrite関数
    #
    #  引数 cmd には、サブプロセスとして実行するプログラムと引数をリスト形式で渡します。\n
    #  例:  [ "subst", "R:", "//remote-machine/public/" ]
    #
    def __init__( self, cmd, cwd=None, env=None, stdout_write=None ):
    
        if stdout_write==None:
            stdout_write = sys.stdout.write
    
        self.cmd = cmd
        self.cwd = cwd
        self.env = env
        self.stdout_write = stdout_write
        self.p = None

    ## サブプロセス処理を実行する
    #
    def __call__(self):

        if ckit_misc.platform()=="win":

            class StartupInfo:
                pass
            
            STARTF_USESHOWWINDOW = 1
            SW_HIDE = 0

            startupinfo = StartupInfo()
            startupinfo.dwFlags = STARTF_USESHOWWINDOW
            startupinfo.wShowWindow = SW_HIDE
        else:
            startupinfo = None

        self.p = subprocess.Popen( self.cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=False, cwd=self.cwd, env=self.env, startupinfo=startupinfo )

        if ckit_misc.platform()=="win":
            encoding="mbcs"
        else:
            encoding="utf-8"

        while self.p.poll()==None:
            self.stdout_write( self.p.stdout.read().decode(encoding) )
        self.stdout_write( self.p.stdout.read().decode(encoding) )

        return self.p.returncode

    ## サブプロセス処理を中断する
    #
    def cancel(self):
        ckit_misc.terminateProcess(self.p.pid)

#-------------------------------------------------------------------------

## @} subprocess

