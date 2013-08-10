
strings_ja = {
    "readonly"                              : "編集禁止です.",
    "search_found"                          : "[%s]が見つかりました.",
    "search_not_found"                      : "[%s]が見つかりません.",
    "modified_line_not_found"               : "変更行が見つかりません.",
    "bookmark_not_found"                    : "ブックマークが見つかりません.",
    "modified_line_or_bookmark_not_found"   : "変更行またはブックマークが見つかりません.",

    "error_config_file_load_failed"         : "ERROR : 設定ファイルの読み込み中にエラーが発生しました.",
    "error_config_file_exec_failed"         : "ERROR : 設定ファイルの実行中にエラーが発生しました.",
}

strings_en = {
    "readonly"                              : "Read only.",
    "search_found"                          : "[%s] is found.",
    "search_not_found"                      : "[%s] is not found.",
    "modified_line_not_found"               : "Modified line not found.",
    "bookmark_not_found"                    : "Bookmark not found.",
    "modified_line_or_bookmark_not_found"   : "Modified line or Bookmark not found.",

    "error_config_file_load_failed"         : "ERROR : loading config file failed.",
    "error_config_file_exec_failed"         : "ERROR : executing config file failed..",
}

strings = {}

def setLocale(locale):

    global strings

    # 日英両方に同じ文言が入っているかチェック
    strings_keys_ja = set( strings_ja.keys() )
    strings_keys_en = set( strings_en.keys() )
    assert( strings_keys_ja==strings_keys_en )

    # 日英の切り替え
    if locale=='ja_JP':
        strings.update(strings_ja)
    else:
        strings.update(strings_en)

