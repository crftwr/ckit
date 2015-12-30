import locale

class TranslatedStrings:

    en_US = 0
    ja_JP = 1

    def __init__(self):

        self.setLocale(locale.getdefaultlocale()[0])

        self._strings = [
            {}, # en_US
            {}, # ja_JP
        ]
    
    def setLocale(self,locale):
        if locale=="ja_JP":
            self.locale = TranslatedStrings.ja_JP
        else:
            self.locale = TranslatedStrings.en_US

    def __getitem__(self,key):
        return self._strings[self.locale][key]

    def setString( self, key, en_US, ja_JP=None ):

        if ja_JP==None:
            ja_JP=en_US

        self._strings[TranslatedStrings.en_US][key] = en_US
        self._strings[TranslatedStrings.ja_JP][key] = ja_JP


strings = TranslatedStrings()

strings.setString( "readonly", 
    en_US = "Read only.", 
    ja_JP = "編集禁止です." )
strings.setString( "search_found", 
    en_US = "[%s] is found.",
    ja_JP = "[%s]が見つかりました." )
strings.setString( "search_not_found", 
    en_US = "[%s] is not found.",
    ja_JP = "[%s]が見つかりません." )
strings.setString( "modified_line_not_found", 
    en_US = "Modified line not found.",
    ja_JP = "変更行が見つかりません." )
strings.setString( "bookmark_not_found", 
    en_US = "Bookmark not found.",
    ja_JP = "ブックマークが見つかりません." )
strings.setString( "modified_line_or_bookmark_not_found", 
    en_US = "Modified line or Bookmark not found.",
    ja_JP = "変更行またはブックマークが見つかりません." )

strings.setString( "error_config_file_load_failed", 
    en_US = "ERROR : loading config file failed.",
    ja_JP = "ERROR : 設定ファイルの読み込み中にエラーが発生しました." )
strings.setString( "error_config_file_exec_failed", 
    en_US = "ERROR : executing config file failed.",
    ja_JP = "ERROR : 設定ファイルの実行中にエラーが発生しました." )

