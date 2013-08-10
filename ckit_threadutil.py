import time
import _thread
import threading
import traceback

from ckit import ckitcore

## @addtogroup threadutil スレッドサポート機能
## @{

#-------------------------------------------------------------------------

## 同期呼び出しクラス
#
#  ほかのスレッドの中で同期して関数を呼び出すためのクラスです。
#
#  __call__() に渡された関数が、ほかのスレッドが呼び出した check() のなかで実行され、
#  返値が __call__() の返値として返ります。
#
class SyncCall:

    ## SyncCallのコンストラクタ
    #
    #  SyncCallのコンストラクタを呼び出したスレッドは内部に記録され、
    #  単純な関数呼び出しで済むのか、マルチスレッドによる同期処理が必要なのかを
    #  判断するために利用されます。
    #
    def __init__(self):
        self.requested = False
        self.returned = False
        self.func = None
        self.args = None
        self.result = None
        self.main_thread_id = _thread.get_ident()

    ## 関数を同期呼び出しする
    #
    #  @param self  -
    #  @param func  関数
    #  @param args  引数リスト
    #  @return      func(*args)の返値
    #
    #  SyncCallのコンストラクタを呼び出したスレッドのなかで __call__() を呼び出した場合は、
    #  単純に func(*args) を呼び出します。
    #
    #  SyncCallのコンストラクタを呼び出したスレッドとは別のスレッドで __call__() を呼び出した場合は、
    #  SyncCallのコンストラクタを呼び出したスレッドのなかで check() が呼ばれるまで待って、
    #  check() のなかで同期的に func(*args) が呼び出されます。
    #
    def __call__( self, func, args=() ):
    
        if _thread.get_ident()==self.main_thread_id:
            return func(*args)
        else:
            self.func = func
            self.args = args
            self.result = None
            self.returned = False
            self.requested = True

            while not self.returned:
                time.sleep(0.01)

            self.requested = False
            self.returned = False

            return self.result

    ## 関数を同期呼び出しが要求されているかをチェックし、必要に応じて呼び出しを行う
    #
    #  check() は、SyncCallのコンストラクタを呼び出したスレッドから呼び出す必要があります。
    #
    #  別のスレッドの __call__() 呼び出しに渡された func, args を、
    #  check() のなかで同期的に呼び出します。
    #
    def check(self):
        assert( _thread.get_ident()==self.main_thread_id )
        if self.requested:
            self.requested = False
            self.result = self.func( *self.args )
            self.returned = True

#-------------------------------------------------------------------------

JOB_STATUS_WAITING  = 0
JOB_STATUS_RUNNING  = 1
JOB_STATUS_FINISHED = 2

## ジョブのアイテム
#
#  ジョブの命令１つを意味するクラスです。
#  JobItemオブジェクトを、JobQueue.enqueue() に渡すことで実行することが出来ます。
#
#  @sa JobQueue
#
class JobItem:

    ## コンストラクタ
    #  @param self              -
    #  @param subthread_func    サブスレッドの中で実行される関数
    #  @param finished_func     サブスレッド処理が終了したあとメインスレッドで呼ばれる関数
    def __init__( self, subthread_func=None, finished_func=None ):
        self.status = JOB_STATUS_WAITING
        self.cancel_requested = False
        self.pause_requested = False
        self.pause_waiting = False
        self.subthread_func = subthread_func
        self.finished_func_list = [ finished_func ]

    ## ジョブのキャンセルを要求する
    def cancel(self):
        self.cancel_requested = True

    ## ジョブのキャンセルが要求されているかをチェックする
    #
    #  @return True:キャンセルが要求されている  False:キャンセルが要求されていない
    #
    def isCanceled(self):
        return self.cancel_requested

    ## ジョブの一時停止を要求する
    def pause(self):
        self.pause_requested = True

    ## ジョブの再開を要求する
    def restart(self):
        self.pause_requested = False

    ## ジョブの一時停止が要求されているかをチェックする
    #
    #  @return True:一時停止が要求されている  False:一時停止が要求されていない
    #
    def isPaused(self):
        return self.pause_requested

    ## ジョブの一時停止が要求されている間、処理をブロックする
    #
    #  @return True:一時停止が発生した  False:一時停止しなかった
    #
    def waitPaused(self):
        if self.pause_requested:
            print( 'Suspended …\n' )
            while self.pause_requested and not self.cancel_requested:
                self.pause_waiting = True
                time.sleep(0.1)
            self.pause_waiting = False
            print( '… Resumed' )
            return True
        return False    


_job_queue_list = []
_job_queue = None

## ジョブのキュー
#
#  ジョブのアイテムを順番に処理する機能を持つクラスです。
#
#  @sa JobItem
#
class JobQueue(threading.Thread):

    ## すべてのJobQueueに関してcheck()を呼び出す
    #
    @staticmethod
    def checkAll():
        for job_queue in _job_queue_list:
            job_queue.check()

    ## すべてのJobQueueに関してjoin()を呼び出す
    #
    @staticmethod
    def joinAll():
        for job_queue in _job_queue_list:
            job_queue.join()

    ## すべてのJobQueueに関してcancel()を呼び出す
    #
    @staticmethod
    def cancelAll():
        for job_queue in _job_queue_list:
            job_queue.cancel()

    ## デフォルトキューを作成する
    #
    @staticmethod
    def createDefaultQueue():
        global _job_queue
        _job_queue = JobQueue()

    ## デフォルトキューを取得する
    #
    @staticmethod
    def defaultQueue():
        global _job_queue
        return _job_queue

    ## コンストラクタ
    def __init__(self):
        threading.Thread.__init__(self)
        self.sema = threading.Semaphore(0)
        self.items = []
        self.cancel_requested = False
        self.pause_requested = False
        self.pause_waiting = False
        _job_queue_list.append(self)
        self.start()

    ## キューを破棄する
    def destroy(self):
        _job_queue_list.remove(self)

    def run(self):

        ckitcore.setBlockDetector()

        while True:

            self.sema.acquire()

            if self.cancel_requested : break

            while self.pause_requested and not self.cancel_requested:
                self.pause_waiting = True
                time.sleep(0.1)
            self.pause_waiting = False
     
            item = self.items[0]

            try:
                if item.subthread_func : item.subthread_func( item )
            except:
                traceback.print_exc()

            item.status = JOB_STATUS_FINISHED

    ## キューにジョブを投入する
    #
    #  @param self  -
    #  @param item  JobItemオブジェクト
    #
    def enqueue( self, item ):
        self.items.append(item)

    ## キューの状態をチェックし必要な処理を実行する
    #
    def check(self):

        while True:

            if len(self.items)==0 : return

            item = self.items[0]

            if item.status == JOB_STATUS_WAITING:

                if item.isCanceled():
                    item.status = JOB_STATUS_FINISHED
                else:
                    item.status = JOB_STATUS_RUNNING
                    self.sema.release()
                    return

            elif item.status == JOB_STATUS_RUNNING:
                return

            elif item.status == JOB_STATUS_FINISHED:
                for finished_func in item.finished_func_list:
                    if finished_func:
                        try:
                            finished_func( item )
                        except:
                            traceback.print_exc()
    
                if len(self.items):
                    del self.items[0]

            else:
                assert(0)

    ## キューに登録されているジョブの数を調査する
    def numItems(self):
        return len(self.items)

    ## キューに登録されている全てのジョブをキャンセルする
    def cancel(self):
        for item in self.items:
            item.cancel()

    ## キューに登録されている全てのジョブの終了を待つ
    def join(self):

        while True:
            if len(self.items)==0 : break
            self.check()
            time.sleep(0.1)

        self.cancel_requested = True
        self.sema.release()

        threading.Thread.join(self)

    ## キューの処理を一時停止する
    def pause(self):

        self.pause_requested = True
        for item in self.items:
            item.pause()
            
        while True:
            if len(self.items)==0 : break
            if self.cancel_requested: break
            if self.pause_waiting: break
            if self.items[0].pause_waiting: break
            time.sleep(0.1)
    
    ## 一時停止されたキューの処理を再開する
    def restart(self):
        self.pause_requested = False
        for item in self.items:
            item.restart()


#-------------------------------------------------------------------------

## 定期的なサブスレッド処理
#
#  Cronのエントリ１つを意味するクラスです。
#  CronItemオブジェクトを、CronTable.add() に渡すことで、実行を予約することが出来ます。
#
#  @sa CronTable
#
class CronItem:

    ## コンストラクタ
    #  @param self      -
    #  @param func      サブスレッドの中で実行される関数
    #  @param interval  呼び出し間隔 (sec)
    def __init__( self, func, interval ):
        self.status = JOB_STATUS_WAITING
        self.cancel_requested = False
        self.func = func
        self.interval = interval
        self.last = time.time()

    ## Cronのキャンセルを要求する
    def cancel(self):
        self.cancel_requested = True

    ## Cronのキャンセルが要求されているかをチェックする
    #
    #  @return True:キャンセルが要求されている  False:キャンセルが要求されていない
    #
    def isCanceled(self):
        return self.cancel_requested

_cron_thread_list = []
_cron_thread = None

## 定期的なサブスレッド処理を管理するためのテーブル
#
#  CronItemを定期的に処理する機能を持つクラスです。
#
#  @sa CronItem
#
class CronTable(threading.Thread):

    ## すべてのCronItemに関してjoin()を呼び出す
    #
    @staticmethod
    def joinAll():
        for cron_thread in _cron_thread_list:
            cron_thread.join()

    ## すべてのCronItemに関してcancel()を呼び出す
    #
    @staticmethod
    def cancelAll():
        for cron_thread in _cron_thread_list:
            cron_thread.cancel()

    ## デフォルトCronTableを作成する
    #
    @staticmethod
    def createDefaultCronTable():
        global _cron_thread
        _cron_thread = CronTable()

    ## デフォルトCronTableを取得する
    #
    @staticmethod
    def defaultCronTable():
        global _cron_thread
        return _cron_thread

    ## コンストラクタ
    def __init__(self):
        threading.Thread.__init__(self)
        self.lock = threading.Lock()
        self.items = []
        self.cancel_requested = False
        _cron_thread_list.append(self)
        self.start()

    ## CronTableを破棄する
    def destroy(self):
        _cron_thread_list.remove(self)

    def run(self):

        ckitcore.setBlockDetector()

        while True:

            if self.cancel_requested : break

            self.lock.acquire()
            try:
                for item in self.items:

                    if self.cancel_requested : break

                    now = time.time()
                    delta = now - item.last

                    if delta > item.interval:
                        try:
                            item.func(item)
                        except:
                            traceback.print_exc()
                        item.last = time.time()
            finally:
                self.lock.release()

            time.sleep(0.1)

    ## CronTableにアイテムを登録する
    #
    #  @param self  -
    #  @param item  CronItemオブジェクト
    #
    def add( self, item ):
        self.lock.acquire()
        try:
            self.items.append(item)
        finally:
            self.lock.release()

    ## CronTableからアイテムを削除する
    #
    #  @param self  -
    #  @param item  CronItemオブジェクト
    #
    def remove( self, item ):
        self.lock.acquire()
        try:
            self.items.remove(item)
        finally:
            self.lock.release()

    ## CronTableから全てのアイテムを削除する
    #
    #  @param self  -
    #
    def clear(self):
        self.lock.acquire()
        try:
            del self.items[:]
        finally:
            self.lock.release()

    ## CronTableに登録されているアイテムの数を調査する
    def numItems(self):
        return len(self.items)

    ## CronTableに登録されている全てのアイテムをキャンセルする
    def cancel(self):
        self.lock.acquire()
        try:
            for item in self.items:
                item.cancel()
        finally:
            self.lock.release()

    ## CronTableに登録されている全てのアイテムの終了を待つ
    def join(self):
        self.cancel_requested = True
        threading.Thread.join(self)

## @} threadutil

#-------------------------------------------------------------------------
