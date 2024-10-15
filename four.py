# Copyright (C) 2024 赵鹏 (Peng Zhao) <224712239@csu.edu.cn>, 王振锋 (Zhenfeng Wang) <234711103@csu.edu.cn>, 杨纪琛 (Jichen Yang) <234712186@csu.edu.cn>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import sys
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *
from PyQt5.QtCore import *
import subprocess
import time
import os
import json
import argparse

piup_json_path = ".piup.json"
rec_width = 50
drop_mutex = QMutex()
copy_mutex = QMutex()
first_enter = True
drag_performed = False
drag_mutex = QMutex()
enter_mutex = QMutex()
has_drag_mutex = QMutex()
MOUSE_LEFT_BOUNDARY = 0x1234566
MOUSE_RELEASED_IN_BOUNDARY = 0x1234567

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.flag = False
        self.bar_windows = []

        # 创建按钮
        button = QPushButton("打开/关闭所有窗口")
        button.clicked.connect(self.click_button)

        # button_minimize = QPushButton("minimize")
        # button_minimize.clicked.connect(self.button_minimize_on_click)

        # 设置主窗口布局
        layout = QVBoxLayout()
        layout.addWidget(button)
        # layout.addWidget(button_minimize)

        central_widget = QWidget()
        central_widget.setLayout(layout)
        self.setCentralWidget(central_widget)

        # 设置定时器
        # self.timer = QTimer()

    def create_bar_windows(self):
        # self.bar_windows.append(BarWindow('top'))
        # self.bar_windows.append(BarWindow('bottom'))
        # self.bar_windows.append(BarWindow('left'))
        # self.bar_windows.append(BarWindow('right'))
        self.bar_windows.append(BarWindow('fullscreen'))

        for bar_window in self.bar_windows:
            bar_window.show()

        # 启动定时器
        # self.timer.timeout.connect(self.bar_windows[0].showMaximized)
        # self.timer.start(500)  # 每500毫秒触发一次

    # def button_minimize_on_click(self):
    #     self.showMinimized()

    def close_bar_windows(self):
        for bar_window in self.bar_windows:
            bar_window.close()
            quitApp()
        self.bar_windows = []
        # self.timer.stop()  # 停止定时器

    def click_button(self):
        if self.flag:
            self.close_bar_windows()
            self.flag = False
        else:
            self.create_bar_windows()
            self.flag = True

    def bring_windows_to_front(self):
        for bar_window in self.bar_windows:
            bar_window.raise_()  # 置顶窗口

class minimizeSignalListenerThread(QThread):
    def __init__(self, myBarWindow):
        super().__init__()
        self.myBarWindow = myBarWindow
    def run(self):
        with open(minimize_fifo_path, 'r') as fifo:
            data = fifo.read()
        print("minimize signal received. Minimizing window.")
        self.myBarWindow.showMinimized()

class closeSignalListenerThread(QThread):
    def __init__(self, myBarWindow):
        super().__init__()
        self.myBarWindow = myBarWindow
    def run(self):
        with open(close_fifo_path, 'r') as fifo:
            data = fifo.read()
        print("close signal received. Closing window.")
        self.myBarWindow.close()
        quitApp()


class mouseReleasedListenerThread(QThread):
    def __init__(self, myBarWindow):
        super().__init__()
        self.myBarWindow = myBarWindow
    def run(self):
        with open(release_fifo_path, 'r') as fifo:
            data = fifo.read()
        print(f"{data} received! ")
        # if (int) (data) == int(MOUSE_RELEASED_IN_BOUNDARY):
        if True:
            if(enter_mutex.tryLock()):
                print("Mouse released and no drop would be performed. Closing window.")
                self.myBarWindow.close()
                quitApp()
            else:
                print("Drop performed. Waiting for transfer to complete")
                drop_mutex.lock()
                copy_mutex.lock()
                print("Drop end. Closing window.")
                self.myBarWindow.close()
                quitApp()
                
            # mutex.lock()
            # mutex.unlock()
            # print("Mouse left. Closing window")
            # self.myBarWindow.close()
            # quitApp()

class mouseLeaveListenerThread(QThread):
    def __init__(self, myBarWindow):
        super().__init__()
        self.myBarWindow = myBarWindow
    def run(self):
        with open(enter_fifo_path, 'r') as fifo:
            data = fifo.read()
        print(f"{data} received! ")
        # if (int) (data) == int(MOUSE_LEFT_BOUNDARY):
        if True:
            print("Mouse left. Unlocking enter mutex")
            enter_mutex.unlock()
            # 如果正在drag 阻塞
            drag_mutex.lock()
            if(copy_mutex.tryLock()):
                # 没有在传输文件
                print("No file transfer in progress. Closing window")
                self.myBarWindow.close()
                quitApp()
            else:
                # 有文件传输，等待传输完成
                print("File transfer in progress. Waiting for transfer to complete")
                self.myBarWindow.showMinimize()
                copy_mutex.lock()
                self.myBarWindow.close()
                quitApp()


class hasDragWriterThread(QThread):
    def __init__(self):
        super().__init__()

    def run(self):
        has_drag_mutex.lock()
        print("Drag detected!")
        with open(has_drag_fifo_path, 'w') as fifo:
            fifo.write("1")
        has_drag_mutex.unlock()

class BarWindow(QWidget):
            
    def __init__(self, position):
        super().__init__()
        # self.urls = [QUrl('file:///home/tclab/test.txt')]
        self.urls = []
        self.right_ip = "192.168.10.36"
        self.drag = None
        self.data = None
        self.action = None
        self.layout = QVBoxLayout()
        # self.label = QLabel("拖拽文件到这里或从这里拖出文件")
        # self.layout.addWidget(self.label)
        self.setLayout(self.layout)
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)  # 无边框窗口
        # 启用拖放功能
        self.setAcceptDrops(True)
        # 获取屏幕分辨率
        screen = QDesktopWidget().screenGeometry()
        screen_width = screen.width()
        screen_height = screen.height()
        self.g_user = os.getlogin()

        # 设置窗口大小和位置
        print("screen_height: ", screen_height)
        print("screen_width: ", screen_width)
        if position == 'top':
            # self.setGeometry(QRect(0, 0, screen_width, rec_width))  # 上方
            self.setFixedSize(screen_width / 2, rec_width)
            self.setGeometry(QRect(0, 0, 15, 15))  # 上方
            # self.move(0, 500)
        elif position == 'bottom':
            # self.setGeometry(QRect(0, screen_height - rec_width, screen_width, rec_width))  # 下方
            self.setGeometry(QRect(0, 0, screen_width, rec_width))  # 下方
            self.setFixedSize(screen_width, rec_width)
        elif position == 'left':
            # self.setGeometry(QRect(0, 2 * rec_width, rec_width, screen_height - 4 * rec_width))  # 左侧
            self.setGeometry(QRect(0, 0, rec_width, screen_height - 4 * rec_width))  # 左侧
            self.setFixedSize(rec_width, screen_height - 4 * rec_width)
        elif position == 'right':
            self.setGeometry(QRect(0, 0, 2 * rec_width, screen_height - 4 * rec_width))  # 右侧
            # self.setGeometry(QRect(screen_width - rec_width, 2 * rec_width, rec_width, screen_height - 4 * rec_width))  # 右侧
            self.setFixedSize(rec_width, screen_height - 4 * rec_width)
        elif position == 'fullscreen':
            self.setGeometry(QRect(0, 0, screen_width, screen_height))  # 右侧
            # self.setGeometry(QRect(0, 0, 600, 800))  # 右侧
            self.setAttribute(Qt.WA_TranslucentBackground)
            # time.sleep(2)
            # self.showMinimized()
        else:
            print("error")

    def send_file_to_remote(self, file_path, user_name, ip, folder, target_psw):
        print("send message to remote host")
        # 弹出一个对话框，让用户输入密码
        password, ok = target_psw, True
        cmd = f'su {self.g_user} -c "scp {file_path} {user_name}@{ip}:{folder}"'
        cmd = f"sshpass -p '{password}' scp -o StrictHostKeyChecking=no {file_path} {user_name}@{ip}:{folder}"
        if not ok:
            print(cmd)
            return_code = os.system(cmd)
        else:
            if not password:
                QMessageBox.critical(None, "错误", "密码不能为空")
                return
            else:
                print(cmd)
                # 使用 subprocess 来执行 scp 命令，输入密码
                process = subprocess.Popen(
                    cmd,
                    shell=True,
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True
                )

                # # 向进程发送密码（模拟交互式输入）
                # process.stdin.write(f"{password}\n")
                # process.stdin.flush()

                # 等待命令执行完毕
                stdout, stderr = process.communicate()
                return_code = process.returncode
        if return_code == 0:
            print("文件传输成功")

            # QMessageBox.information(None, "成功", "文件传输成功")
        else:
            print(f"文件传输失败")
            QMessageBox.critical(None, "错误", f"文件传输失败")


    def dragEnterEvent(self, event):
        print("dragEnterEvent")
        global first_enter
        for mime_format in event.mimeData().formats():
            print(f"Mime type: {mime_format}")
            print(event.mimeData().data(mime_format))
        if event.mimeData().hasUrls():
            event.acceptProposedAction()  # 接受提议的拖拽操作
            print("drag enter")
            if first_enter:
                first_enter = False
                has_drag_mutex.unlock()
                enter_mutex.tryLock()
                print("enter_mutex locked")
        else:
            event.ignore()

    def dropEvent(self, event):
        piup_data = None
        with open(piup_json_path, 'r') as json_file:
            piup_data = json.load(json_file)

        if piup_data == None:
            print("failed to load piup data")
        target_json = piup_data[target]
        target_ip = target_json["ip"]
        target_user = target_json["user"]
        target_psw = target_json["psw"]
        copy_mutex.lock()
        drop_mutex.unlock()
        self.urls = event.mimeData().urls()
        for file in self.urls:
            # self.label.setText(f"已选择文件: {file.toLocalFile()}")
            print(f"file received: {file}")
            self.send_file_to_remote(file.toLocalFile(), target_user, target_ip, dnd_buf_path, target_psw)
        copy_mutex.unlock()

    def mousePressEvent(self, event):
        global drag_performed
        if event.button() == Qt.LeftButton:
            # 创建 MIME 数据
            mime_data = QMimeData()
            dnd_buf =  os.listdir(dnd_buf_path)
            max_retries = 30
            while(dnd_buf == [] and max_retries):
                print("dnd_buf empty")
                dnd_buf = os.listdir(dnd_buf_path)
                time.sleep(0.1)
                max_retries -= 1
            if dnd_buf != []:
                print(dnd_buf)
                self.urls = [QUrl.fromLocalFile(dnd_buf_path + f"/{file}") for file in dnd_buf]

            mime_data.setUrls(self.urls)  # 这里可以是文件名或路径

            # 创建拖拽对象
            self.drag = QDrag(self)
            self.drag.setMimeData(mime_data)

            # 设置图标
            # pixmap = QPixmap(50, 50)  # 设定拖拽图标的大小
            # pixmap.fill(Qt.red)  # 填充
            # self.drag.setPixmap(pixmap)

        # 将全局位置转换为相对于 label 的局部坐标
            # hotspot = event.pos()
            # hotspot = event.pos() - self.rect().topLeft()
            # self.drag.setHotSpot(hotspot)


            # 执行拖拽
            drag_mutex.lock()
            self.showMinimized()
            result = self.drag.exec_(Qt.CopyAction)
            if result != Qt.IgnoreAction:
                drag_performed = True
                if result == Qt.CopyAction:
                    print(f"Copy action performed. File name: {self.urls}")
                elif result == Qt.MoveAction:
                    print(f"Move action performed. File name: {self.urls}")
    

    def closeEvent(self, event):
        if drag_performed:
            print("drag performed, cleaning buf")
            for f in os.listdir(dnd_buf_path):
                print(f"removing {f}")
                os.remove(dnd_buf_path + f"/{f}")
        quitApp()

def quitApp():
    QApplication.quit()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-target", choices=["left", "right", "center", "up", "down", None], default=None)
    parser.add_argument("-place", choices=["center", "edge"], default="center")
    target = parser.parse_args().target
    place = parser.parse_args().place
    print(f"current user is in {place} place")

    if place != "center":
        target = "center"
    


    username = os.getlogin()
    home_path = "/home/"+username
    print("home_path: ",home_path)
    print("four.py pid: ",os.getpid())
    dnd_buf_path = home_path + "/.seamlessdnd"
    if not os.path.exists(dnd_buf_path):
        os.mkdir(dnd_buf_path)
    release_fifo_path = home_path + "/.releasefifo"
    if not os.path.exists(release_fifo_path):
        os.mkfifo(release_fifo_path)
    enter_fifo_path = home_path + "/.enterfifo"
    if not os.path.exists(enter_fifo_path):
        os.mkfifo(enter_fifo_path)
    close_fifo_path = home_path + "/.closefifo"
    if not os.path.exists(close_fifo_path):
        os.mkfifo(close_fifo_path)
    minimize_fifo_path = home_path + "/.minimizefifo"
    if not os.path.exists(minimize_fifo_path):
        os.mkfifo(minimize_fifo_path)
    has_drag_fifo_path = home_path + "/.hasdragfifo"
    if not os.path.exists(has_drag_fifo_path):
        os.mkfifo(has_drag_fifo_path)
    print("FIFO CREATED")



    app = QApplication(sys.argv)
    # main_window = MainWindow()
    # main_window.show()
    drop_mutex.lock()
    has_drag_mutex.lock()
    print("CREATING BARWINDOW")
    bw = BarWindow("fullscreen")
    movementListener = mouseReleasedListenerThread(bw)
    enterListener = mouseLeaveListenerThread(bw)
    closeListener = closeSignalListenerThread(bw)
    minimizeListener = minimizeSignalListenerThread(bw)
    hasDragWriter = hasDragWriterThread()
    print("STARTING LISTENER")
    movementListener.start()
    enterListener.start()
    closeListener.start()
    minimizeListener.start()
    hasDragWriter.start()

    bw.show()

    sys.exit(app.exec_())
