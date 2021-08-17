#!/usr/bin/python3

import os
import threading
import time
import multiprocessing
import queue
import collections
import socket
import pickle
import importlib
import copy

SERVICE_EXEC = "SERVICE_EXEC"
COMMAND_LINE_EXEC = "COMMAND_LINE_EXEC"

witcherTaskQueue = None
thread_count = None
taskQueueMemoryBarrier = None 
threadWaitCondition = None
witcherParallelMainServer = None
witcherSocketFile = "witcherServer"
witcherThreadList = []

class witcherWorkerThread(threading.Thread):
    def __init__(self,id):
        self.threadID = id
        self.stop = threading.Event()
        self.barrier = threading.Event()
        threading.Thread.__init__(self)
        self.start()

    def stop_thread(self):
        self.stop.set()
        self.join()
        return

    def add_task(self,task):
        witcherTaskQueue.put(task)
        return

    def get_task(self):
        return witcherTaskQueue.get()

    def exec_task(self,job):
        if(job[0] == COMMAND_LINE_EXEC):
            os.system(job[2])
        elif(job[0] == SERVICE_EXEC):
            execObj = pickle.loads(job[1])
            args = job[3]
            getattr(execObj,job[2])(*args)
        return

    def run(self):
        while(not self.stop.isSet()):
            if(witcherTaskQueue.qsize() != 0):
                job = self.get_task()
                if(job is None):
                    taskQueueMemoryBarrier.wait()
                    with threadWaitCondition:
                        threadWaitCondition.notify_all()
                else:
                    self.exec_task(job)
                witcherTaskQueue.task_done()
        return

def _initWitcherParallel():
    global witcherParallelMainServer
    global witcherTaskQueue
    global thread_count
    global taskQueueMemoryBarrier
    global threadWaitCondition
    witcherTaskQueue = queue.Queue()
    thread_count = multiprocessing.cpu_count() - 1
    taskQueueMemoryBarrier = threading.Barrier(thread_count)
    threadWaitCondition = multiprocessing.Condition()
    if os.path.exists(witcherSocketFile):
        os.remove(witcherSocketFile)
    witcherParallelMainServer = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    witcherParallelMainServer.bind(witcherSocketFile)
    return

def getThreadPoolStatus():
    threadPoolStatus = "UNINITIALIZED"
    check_soc = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        check_soc.connect(witcherSocketFile)
    except socket.error:
        pass
    else:
        check_soc.close()
        threadPoolStatus = "INITIALIZED"
    return threadPoolStatus

def threadPoolDispatchTask(taskName,taskObj=None,modName="",taskArgsList=[],taskType=COMMAND_LINE_EXEC):
    pickleTask = pickle.dumps(taskObj)
    pickleTask = pickleTask.replace(b'__main__',modName.encode())
    task = [taskType,pickleTask,taskName,taskArgsList]
    dispatch_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    dispatch_sock.connect(witcherSocketFile)
    try:
        task=pickle.dumps(task)
        dispatch_sock.sendall(task)
    finally:
        dispatch_sock.close()
    return

def threadPoolWait():
    data = None
    barrier_command = "Barrier"
    barrier_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    barrier_sock.connect(witcherSocketFile)
    try:
        barrier_command = pickle.dumps(barrier_command)
        barrier_sock.sendall(barrier_command)
        while(True):
            data = barrier_sock.recv(100)
            data = pickle.loads(data)
            if(data == "BarrierComplete"):
                break;
    finally:
        barrier_sock.close()
    return

def threadPoolQuit():
    quit_task_command = "Quit"
    quit_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    quit_sock.connect(witcherSocketFile)
    try:
        quit_task_command=pickle.dumps(quit_task_command)
        quit_sock.sendall(quit_task_command)
    finally:
        quit_sock.close()
    return

def _terminateThreadPool():
    witcherTaskQueue.join()
    witcherParallelMainServer.close()
    for t in witcherThreadList:
        t.stop_thread()
    return

def _operateThreadPool():
    for i in range(thread_count):
        t = witcherWorkerThread(i)
        witcherThreadList.append(t)
    witcherParallelMainServer.listen()
    loop = True
    while loop:
        connection, client_address = witcherParallelMainServer.accept()
        while True:
            task = connection.recv(4096)
            if not task:
                break
            else:
                task = pickle.loads(task)
                if(task == "Quit"):
                    loop = False
                    _terminateThreadPool()
                    break
                elif(task == "Barrier"):
                    barrier_conn = connection
                    for j in range(thread_count):
                        witcherTaskQueue.put(None)
                    with threadWaitCondition:
                        threadWaitCondition.wait()
                    barrier_conn.sendall(pickle.dumps("BarrierComplete"))
                else:
                    witcherTaskQueue.put(task)
    return

def spawnWitcherThreadPool():
    _initWitcherParallel()
    _operateThreadPool()

def startWitcherThreadPool():
    mainThread = threading.Thread(target=spawnWitcherThreadPool)
    mainThread.start()
    return

if __name__ == "__main__":
    spawnWitcherThreadPool()
    exit(0)
