## @addtogroup command コマンド機能
## @{

## コマンドの引数に与えられる情報を格納するクラス
class CommandInfo:
    
    ## コンストラクタ
    def __init__(self):
        
        self.result = True
        
        ## コマンドに与えられた引数
        self.args = []
        
        ## コマンド実行時に押されていたモディファイアキー
        self.mod = 0


class NamedCommand:
    
    def __init__( self, owner, name ):
        self.owner = owner
        self.name = name
    
    def __call__( self, info=None ):
        if info==None:
            info = CommandInfo()
        if not self.owner.executeCommand( self.name, info ):
            info.result=False


## 複数のコマンドを連続実行するためのコマンドクラス
class CommandSequence:

    def __init__( self, *commands ):
        self.commands = commands
        
    def __call__( self, info=None ):
        if info==None:
            info = CommandInfo()
        for command in self.commands:
            command(info)
            if info.result:
                return


class CommandMap:

    def __init__( self, owner ):
        self.owner = owner

    def __getattr__( self, name ):
        return NamedCommand( self.owner, name )


## @} command

