import ctypes
import sys
import os
import json
import socket
import time
import inspect
from ctypes import *
import subprocess
import signal
import netifaces
import threading
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *
# 一些全局参数
load_so = 1
GRID_NUM = 3  # 最好是奇数
GRID_SIZE_H = 90  # 每个网格高度的像素大小
GRID_SIZE_W = 160  # 每个网格高度的像素大小
port = 5000

# 自定义可拖拽标签类
class DraggableLabel(QLabel):
    """自定义可拖动的标签，用于显示客户端和服务器的IP地址"""
    def __init__(self, name, x, y, main_window, movable=True, user=None):
        super().__init__(name, main_window)  # 父类构造
        self.x = x
        self.y = y
        self.ip = name
        self.server_ip = None
        self.movable = movable
        self.remote_folder = "~/share_files/"
        self.user = user
        self.main_window = main_window  # 这里确保是 ServerClientApp 的实例
        self.setFixedSize(GRID_SIZE_W, GRID_SIZE_H)
        if movable:
            self.setStyleSheet("background-color: lightblue; border: 1px solid black; padding: 5px;")
        else:
            self.setStyleSheet("background-color: orange; border: 1px solid black; padding: 5px;")
        self.setAlignment(Qt.AlignCenter)
        self.set_position(x, y)
        self.setAcceptDrops(True)  # 允许拖拽操作

    def set_position(self, x, y):
        """根据网格坐标设置图标的绝对位置"""
        self.x = x
        self.y = y
        pixel_x = x * GRID_SIZE_W
        pixel_y = y * GRID_SIZE_H
        self.move(pixel_x, pixel_y)

    def mousePressEvent(self, event):
        if self.movable and event.button() == Qt.LeftButton:
            self.drag_start_position = event.pos()

    def mouseMoveEvent(self, event):
        if self.movable and event.buttons() & Qt.LeftButton:
            new_position = self.mapToParent(event.pos() - self.drag_start_position)
            self.x, self.y = new_position.x(), new_position.y()
            self.move(self.x, self.y)

    def mouseReleaseEvent(self, event):
        if self.movable:
            # 当鼠标释放时，计算最近的网格位置
            new_x = self.x + self.width() / 2
            new_y = self.y + self.height() / 2
            new_x = max(0, min(GRID_NUM - 1, int(new_x // GRID_SIZE_W)))
            new_y = max(0, min(GRID_NUM - 1, int(new_y // GRID_SIZE_H)))
            self.set_position(new_x, new_y)

            # 更新label
            self.main_window.update_label(self)

    def dragEnterEvent(self, event):
        # 如果拖入的是文件，则接受拖拽事件
        if event.mimeData().hasUrls():
            event.acceptProposedAction()

    def dropEvent(self, event):
        if (not self.movable) and (not self.user):
            QMessageBox.information(self, '失败', f'不能给自己发送文件')
            return

        # 获取文件路径
        if event.mimeData().hasUrls():
            file_paths = [url.toLocalFile() for url in event.mimeData().urls()]
            if self.movable:
                remote_ip = self.ip
            else:
                remote_ip = self.server_ip
            # 弹出确认窗口，询问用户是否确定传输文件
            reply = QMessageBox.question(self, '确认',
                                         f"你确定要将文件发送到 {remote_ip} 吗？",
                                         QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
            if reply == QMessageBox.Yes:
                # 获取所有文件路径
                for file_path in file_paths:
                    try:
                        self.send_file_to_remote(file_path, self.user, remote_ip, self.remote_folder)
                        QMessageBox.information(self, '成功', f'文件{file_path}成功传输到 {remote_ip}:{self.remote_folder}')
                    except Exception as e:
                        QMessageBox.critical(self, '错误', f'文件{file_path}传输失败:\n{str(e)}')
            else:
                print("取消传输文件")
    def send_file_to_remote(self, file_path, user_name, ip, folder):
        print("send message to remote host")
        command = f'su {g_user} -c scp "{file_path} {user_name}@{ip}:{folder}"'
        print(command)
        return_code = os.system(command)
        print(f"The return_code:{return_code}")
# 共享端用服务器线程
class ServerThread(QThread):
    # 定义一个信号来传递接收到的数据
    message_received = pyqtSignal(dict)

    def __init__(self):
        super().__init__()
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        try:
            self.server_socket.bind(("", port))  # 监听 port 端口
            print(f"成功绑定到端口 {port}")
        except OSError as e:
            print(f"绑定端口 {port} 失败: {e}")
            sys.exit()

    def run(self):
        print("共享端开始处于在线状态...")
        while True:
            data, addr = self.server_socket.recvfrom(1024)
            try:
                recv_data = json.loads(data.decode('utf-8'))
            except json.JSONDecodeError:
                print("接收到的数据不是有效的 JSON 格式")
                continue
            if recv_data["operation"] in ["online", "login", "logout"]:
                if(recv_data["operation"] == "login"):
                    self.send()
                message = {'ip': addr[0], 'operation': recv_data["operation"], 'user': recv_data["user"]}
                # 发出信号通知主线程更新 UI
                self.message_received.emit(message)
            elif recv_data["operation"] == "who_is_online":
                pass
            else:
                print(f"error message: {recv_data}")
    def send(self):
        broadcast_address = (get_broadcast_address(), port)  # 向子网广播
        print(f"向{broadcast_address}发送广播,查找在线设备")
        try:
            self.server_socket.sendto(f'{{"operation":"who_is_online", "user":"{g_user}"}}'.encode('utf-8'), broadcast_address)
        except OSError as e:
            print(f"发送错误: {e}")
    def stop(self):
        self.terminate()
# 使用端用客户器线程
class ClientThread(QThread):
    # 定义一个信号来传递接收到的数据
    message_received = pyqtSignal(dict)

    def __init__(self):
        super().__init__()
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self.client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        try:
            self.client_socket.bind(("", port))  # 监听 port 端口
            print(f"成功绑定到端口 {port}")
        except OSError as e:
            print(f"绑定端口 {port} 失败: {e}")
            sys.exit()

    def run(self):
        print("使用端开始处于在线状态...")
        self.send(f'{{"operation":"login", "user":"{g_user}"}}')
        while True:
            data, addr = self.client_socket.recvfrom(1024)
            try:
                recv_data = json.loads(data.decode('utf-8'))
            except json.JSONDecodeError:
                print("接收到的数据不是有效的 JSON 格式")
                continue
            if recv_data["operation"] in ["online", "login", "logout", "who_is_online"]:
                if recv_data["operation"] == "who_is_online":
                    self.send(f'{{"operation":"online", "user":"{g_user}"}}')
                    message = {'ip': addr[0], 'operation': recv_data["operation"], 'user': recv_data["user"]}
                    # 发出信号通知主线程更新 UI
                    self.message_received.emit(message)
            else:
                print(f"error message: {recv_data}")
    def send(self,content):
        broadcast_address = (get_broadcast_address(), port)  # 向子网广播
        print(content)
        try:
            self.client_socket.sendto(content.encode('utf-8'), broadcast_address)
        except OSError as e:
            print(f"发送错误: {e}")
    def stop(self):
        self.terminate()

# 启动.so库用线程
class StartThread(QThread):
    def __init__(self, so_name):
        super().__init__()
        self.so_name = so_name
    def run(self):
        my_function = CDLL(self.so_name)
        my_function.main()
    def stop(self):
        self.terminate()

# 自定义主界面类
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("软件的名字")
        self.resize(640, 480)
        self.center()
        self.setMaximumSize(640,480)
        self.identityChoose()

        self.setWindowOpacity(0.9)

    def center(self):
        """将窗口移动到屏幕中心"""
        screen_geometry = QDesktopWidget().availableGeometry()
        window_geometry = self.frameGeometry()
        center_point = screen_geometry.center()
        window_geometry.moveCenter(center_point)
        self.move(window_geometry.topLeft())
    def identityChoose(self):
        self.headlabel = QLabel("请选择该主机的角色", self)
        self.headlabel.setGeometry(175, 30, 530, 40)
        self.headlabel.setStyleSheet(
            "color:rgba(10, 10, 30, 255);"
            "font-size:30px;"
            "font-family:微软雅黑;"
        )
        self.btn_master_control = QPushButton('共享端', self)
        self.btn_master_control.setGeometry(50, 150, 250, 250)
        self.btn_master_control.setStyleSheet(
            "QPushButton{color:rgba(255,255,255,255)}"
            "QPushButton:hover{color:black}"
            "QPushButton{background-color:rgba(119,136,153,100)}"
            "QPushButton{border:10px}"
            "QPushButton{border-radius:20px}"
            "QPushButton{padding:2px 4px}"
            "QPushButton{font-weight:bold}"
            "QPushButton{font-size:40px}")
        self.btn_master_control.setToolTip("如果您拥有鼠标或键盘，愿意将自己的鼠标和键盘共享给其他设备，请选择该身份")

        self.btn_servant = QPushButton('使用端', self)
        self.btn_servant.setGeometry(350, 150, 250, 250)
        self.btn_servant.setStyleSheet(
            "QPushButton{color:rgba(255,255,255,255)}"
            "QPushButton:hover{color:black}"
            "QPushButton{background-color:rgba(192,192,192,175)}"
            "QPushButton{border:10px}"
            "QPushButton{border-radius:20px}"
            "QPushButton{padding:2px 4px}"
            "QPushButton{font-weight:bold}"
            "QPushButton{font-size:40px}")
        self.btn_servant.setToolTip("如果您不具有鼠标或键盘，想要使用其他设备的鼠标或键盘，请选择该身份")

# 自定义共享端界面类
class MasterControl(QWidget):

    def __init__(self, parent):
        super().__init__()
        self.parent = parent
        self.initUI()

    def initUI(self):
        # 参数初始化
        self.config_file = "config.json"
        self.Draglabel=[]
        # 初始化配置文件
        self.set_config()
        self.start_server_thread()# 开启服务器线程
        # 主窗口设置
        self.main_window()
        self.setup_server()  # 设置服务器图标
        # 设置按钮
        self.button()
        if load_so:
            # 加载动态库
            print("create a new thread to load .so")
            self.thr = StartThread("./input_device_shared.so")
            self.thr.start()

    def set_config(self):
        """加载配置文件，如果不存在则创建默认配置"""
        json_output = generate_json({}, [])
        with open('config.json', 'r+') as json_file:
            json_file.seek(0)
            json_file.truncate()
            json_file.write(json_output)
        self.clients = []  # 初始化客户端列表
        self.old_clients = []  # 初始化旧客户端列表
        self.old_device_info = {}  # 初始化设备信息

    def start_server_thread(self):
        # 启动服务器线程
        self.server_thread = ServerThread()
        self.server_thread.message_received.connect(self.message_handle)  # 连接信号到槽函数
        self.server_thread.start()
        self.server_thread.send()
    def find_available_position(self):
        """查找一个未被占用的位置"""
        taken_positions = {(client['x'], client['y']) for client in self.clients}

        # 遍历整个网格，找到第一个空闲位置
        for x in range(GRID_NUM):
            for y in range(GRID_NUM):
                if (x,y) == (GRID_NUM//2,GRID_NUM//2):
                    continue
                if (x, y) not in taken_positions:
                    return x, y

        return None, None  # 如果没有找到可用的位置
    def message_handle(self, message):
        """槽函数：接收到消息后更新 UI"""
        if message["operation"] in ["online", "login"]:
            print(message["operation"])
            ip = message["ip"]
            # 检查 clients 列表中是否已有该 IP
            if any(client['ip'] == ip for client in self.clients):
                print(f"用户 {ip} 已经在线，跳过添加。")
            else:
                # 查找可用的位置
                new_x, new_y = self.find_available_position()
                if new_x is not None and new_y is not None:
                    # 添加新客户端到 clients 列表
                    new_client = {'x': new_x, 'y': new_y, 'ip': ip}
                    self.clients.append(new_client)
                    label = DraggableLabel(ip, new_x, new_y, self, movable=True, user=message["user"])
                    label.show()
                    self.Draglabel.append(label)
                    print(f"用户 {ip} 已添加到在线列表，位置: ({new_x}, {new_y})")
                    print(self.clients)
                else:
                    print("没有可用的位置，无法添加新用户。")

        elif message["operation"] == "logout":
            ip = message["ip"]
            # 从 clients 列表中删除相应的 IP
            self.clients = [client for client in self.clients if client['ip'] != ip]
            print(f"用户 {ip} 已从在线列表中移除。")
        self.update_clients()  # 更新界面
        self.update_config()  # 更新配置文件
    def main_window(self):
        self.setWindowTitle('共享端')
        self.resize(GRID_NUM * GRID_SIZE_W,  (GRID_NUM+1)* GRID_SIZE_H)
        self.setMaximumSize( GRID_NUM * GRID_SIZE_W,  (GRID_NUM+1)* GRID_SIZE_H)
        # 阻塞主窗口
        self.setWindowModality(Qt.ApplicationModal)
    def stop_thread(self):
        self.thr.stop()
        self.server_thread.stop()  # 停止线程
    def closeEvent(self, event):
        # 执行特定操作
        reply = QMessageBox.question(self, '确认', '你确定要退出吗?', QMessageBox.Yes | QMessageBox.No, QMessageBox.No)

        if reply == QMessageBox.Yes:
            # 这里可以添加你希望执行的操作
            self.stop_thread()
            os.kill(os.getpid(), signal.SIGINT)
            self.parent.show()
            event.accept()  # 关闭窗口
        else:
            event.ignore()  # 忽略关闭事件
    # def paintEvent(self, event):
    #     """绘制网格"""
    #     painter = QPainter(self)
    #     pen = QPen(Qt.black, 0.5)
    #     painter.setPen(pen)
    #
    #     for i in range(GRID_NUM):
    #         # 垂直线
    #         painter.drawLine(i * GRID_SIZE_W, 0, i * GRID_SIZE_W, self.height())
    #         # 水平线
    #         painter.drawLine(0, i * GRID_SIZE_H, self.width(), i * GRID_SIZE_H)
    #     painter.drawLine(0, GRID_NUM * GRID_SIZE_H, self.width(), GRID_NUM * GRID_SIZE_H)
    def setup_server(self):
        """根据配置文件添加服务器图标"""
        local_ip = get_host_ip()
        self.server_label = DraggableLabel(local_ip, GRID_NUM//2, GRID_NUM//2, self, movable=False)
    def update_clients(self):
        """更新客户端图标位置"""
        to_delete_list=[]
        #比对Draglabel跟clients,多余的label删除,并将相应label移动来更新位置
        for label in self.Draglabel:
            for client in self.clients:
                if label.ip == client["ip"]:
                    label.set_position(client["x"], client["y"])
                    break
            else:
                to_delete_list.append(label)
        self.Draglabel = [label for label in self.Draglabel if label not in to_delete_list]#在Draglabel里面删除掉相应label
        for label in to_delete_list:
            label.deleteLater()#在界面中删除相应label

    def update_label(self, label):
        """更新配置文件"""
        if label.movable:
            for client in self.clients:
                if client['ip'] == label.text():
                    client['x'] = label.x
                    client['y'] = label.y
                    break
            # self.save_config() # 让每次拖动释放的时候都保存配置
    def button(self):
        center_num = GRID_NUM//2
        # 搜索按钮
        self.btn_search = QPushButton('搜索在线设备', self)
        self.btn_search.setGeometry(10+GRID_SIZE_W*(center_num-1),  GRID_SIZE_H*GRID_NUM+10,GRID_SIZE_W-20, GRID_SIZE_H-20)
        self.btn_search.setStyleSheet(
            "QPushButton{color:rgba(255,255,255,255)}"
            "QPushButton:hover{color:black}"
            "QPushButton{background-color:rgba(70,130,180,120)}"
            "QPushButton{border:10px}"
            "QPushButton{border-radius:20px}"
            "QPushButton{padding:2px 4px}"
            "QPushButton{font-weight:bold}"
            "QPushButton{font-size:20px}")
        self.btn_search.setToolTip('这是用来搜索在线设备的')
        self.btn_search.clicked.connect(self.btn_search_event)
        # 保存配置按钮
        self.btn_save = QPushButton('保存配置', self)
        self.btn_save.setGeometry(10+GRID_SIZE_W*center_num, GRID_SIZE_H * GRID_NUM + 10, GRID_SIZE_W - 20, GRID_SIZE_H - 20)
        self.btn_save.setStyleSheet(
            "QPushButton{color:rgba(255,255,255,255)}"
            "QPushButton:hover{color:black}"
            "QPushButton{background-color:rgba(70,130,180,120)}"
            "QPushButton{border:10px}"
            "QPushButton{border-radius:20px}"
            "QPushButton{padding:2px 4px}"
            "QPushButton{font-weight:bold}"
            "QPushButton{font-size:20px}")
        self.btn_save.setToolTip('这是用来保存配置的')
        self.btn_save.clicked.connect(self.btn_save_event)
        # 返回按钮
        self.btn_return = QPushButton('返回', self)
        self.btn_return.setGeometry(10+GRID_SIZE_W*(center_num+1),  GRID_SIZE_H*GRID_NUM+10, GRID_SIZE_W-20, GRID_SIZE_H-20)
        self.btn_return.setStyleSheet(
            "QPushButton{color:rgba(255,255,255,255)}"
            "QPushButton:hover{color:black}"
            "QPushButton{background-color:rgba(70,130,180,120)}"
            "QPushButton{border:10px}"
            "QPushButton{border-radius:20px}"
            "QPushButton{padding:2px 4px}"
            "QPushButton{font-weight:bold}"
            "QPushButton{font-size:20px}")
        self.btn_return.clicked.connect(self.btn_return_event)
        self.btn_return.setToolTip('这是用来返回界面的')

    def update_config(self):
        device_info = {}
        #
        for client in self.clients:
            if (client["x"] == GRID_NUM // 2 + 1 and client["y"] == GRID_NUM // 2):
                device_info[client["ip"]] = {"position": "right"}
            if (client["x"] == GRID_NUM // 2 and client["y"] == GRID_NUM // 2 + 1):
                device_info[client["ip"]] = {"position": "down"}
            if (client["x"] == GRID_NUM // 2 - 1 and client["y"] == GRID_NUM // 2):
                device_info[client["ip"]] = {"position": "left"}
            if (client["x"] == GRID_NUM // 2 and client["y"] == GRID_NUM // 2 - 1):
                device_info[client["ip"]] = {"position": "up"}
        if device_info != self.old_device_info:
            new_clients = list(device_info.keys())
            removed_device_ip = list(set(self.old_clients) - set(new_clients))
            # print(self.old_clients)
            # print(new_clients)
            json_output = generate_json(device_info, removed_device_ip)
            with open('config.json', 'r+') as json_file:
                json_file.seek(0)
                json_file.truncate()
                json_file.write(json_output)
            self.old_clients = new_clients
            self.old_device_info = device_info
        else:
            # print(self.old_device_info)
            # print(device_info)
            pass
    def btn_search_event(self):
        self.server_thread.send()
    def btn_save_event(self):
        self.update_config()
        print("配置已保存")
    def btn_return_event(self):
        self.close()
        # exit(0)

# 自定义使用端界面类
class Servant(QWidget):

    def __init__(self, parent):
        super().__init__()
        self.parent = parent
        self.initUI()
        if load_so:
            # 加载动态库并运行
            print("create a new thread to load .so")
            self.thr = StartThread("./input_device_server.so")
            self.thr.start()
    def initUI(self):
        self.start_client_thread()  # 开启使用端线程

        # 主窗口设置
        self.main_window()
        self.set_a_label()

        # 按钮
        self.button()
    def set_a_label(self):
        """根据配置文件添加服务器图标"""
        local_ip = get_host_ip()
        self.label = DraggableLabel(local_ip, GRID_NUM//2, GRID_NUM//2, self, movable=False)
    def main_window(self):
        self.setWindowTitle('使用端')
        self.resize(GRID_NUM * GRID_SIZE_W, (GRID_NUM + 1) * GRID_SIZE_H)
        self.setMaximumSize(GRID_NUM * GRID_SIZE_W, (GRID_NUM + 1) * GRID_SIZE_H)
        # 阻塞主窗口
        self.setWindowModality(Qt.ApplicationModal)
    def stop_thread(self):
        self.thr.stop()
        self.client_thread.stop()  # 停止线程
    def closeEvent(self, event):
        # 执行特定操作
        reply = QMessageBox.question(self, '确认', '你确定要退出吗?', QMessageBox.Yes | QMessageBox.No, QMessageBox.No)

        if reply == QMessageBox.Yes:
            # 这里可以添加你希望执行的操作
            if self.client_thread:
                self.client_thread.send(f'{{"operation":"logout", "user":"{g_user}"}}')

            else:
                print("client线程没有启动")
            # self.stop_thread()
            os.kill(os.getpid(), signal.SIGINT)
            self.parent.show()
            event.accept()  # 关闭窗口
        else:
            event.ignore()  # 忽略关闭事件
    def start_client_thread(self):
        # 启动服务器线程
        self.client_thread = ClientThread()
        self.client_thread.message_received.connect(self.message_handle)  # 连接信号到槽函数
        self.client_thread.start()
    def message_handle(self, message):
        """槽函数：接收到消息后更新 UI"""
        self.label.user = message["user"]
        self.label.server_ip = message["ip"]
        print(message)

    def button(self):
        center_num = GRID_NUM // 2
        # 连接按钮
        self.btn_connect = QPushButton('连接', self)
        self.btn_connect.setGeometry(10 + GRID_SIZE_W * (center_num - 1), GRID_SIZE_H * GRID_NUM + 10, GRID_SIZE_W - 20,
                                  GRID_SIZE_H - 20)
        self.btn_connect.setStyleSheet(
            "QPushButton{color:rgba(255,255,255,255)}"
            "QPushButton:hover{color:black}"
            "QPushButton{background-color:rgba(70,130,180,120)}"
            "QPushButton{border:10px}"
            "QPushButton{border-radius:20px}"
            "QPushButton{padding:2px 4px}"
            "QPushButton{font-weight:bold}"
            "QPushButton{font-size:20px}")
        self.btn_connect.setToolTip('这是用来广播自己,并连接共享端的')
        self.btn_connect.clicked.connect(self.btn_connect_event)
        # 返回按钮
        self.btn_return = QPushButton('退出', self)
        self.btn_return.setGeometry(10 + GRID_SIZE_W * (center_num + 1), GRID_SIZE_H * GRID_NUM + 10, GRID_SIZE_W - 20,
                                    GRID_SIZE_H - 20)
        self.btn_return.setStyleSheet(
            "QPushButton{color:rgba(255,255,255,255)}"
            "QPushButton:hover{color:black}"
            "QPushButton{background-color:rgba(70,130,180,120)}"
            "QPushButton{border:10px}"
            "QPushButton{border-radius:20px}"
            "QPushButton{padding:2px 4px}"
            "QPushButton{font-weight:bold}"
            "QPushButton{font-size:20px}")
        self.btn_return.clicked.connect(self.btn_return_event)
        self.btn_return.setToolTip('这是用来退出')

    def btn_connect_event(self):
        self.client_thread.send("online")
    def btn_return_event(self):
        self.close()
        # exit(0)

# 一些全局函数
def get_broadcast_address():
    # 获取所有网络接口
    interfaces = netifaces.interfaces()
    for interface in interfaces:
        # 获取接口的信息
        addrs = netifaces.ifaddresses(interface)
        if netifaces.AF_INET in addrs:
            # 获取IPv4地址
            ipv4_info = addrs[netifaces.AF_INET][0]
            ip = ipv4_info['addr']
            # 获取广播地址
            if 'broadcast' in ipv4_info:
                broadcast = ipv4_info['broadcast']
                return broadcast
    return None
def get_host_ip():
    """
    查询本机ip地址
    :return: ip
    """
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
    finally:
        s.close()
    return ip
def generate_json(device_info, ip_removed):
    # 从 device_info 中提取 IP 列表
    ip_list_full = list(device_info.keys())

    # 创建结果字典
    result = {
        "ip_list": ip_list_full,
        "device_info": device_info,
        "removed_device_ip": list(set(ip_removed))  # 使用 set 来确保没有重复
    }

    return json.dumps(result, indent=4)  # 返回格式化的 JSON 字符串
def btn_master_control_event():
    main_window.hide()
    master_control = MasterControl(main_window)
    master_control.show()
def btn_Servant_event():
    main_window.hide()
    servant = Servant(main_window)
    servant.show()

if __name__ == '__main__':
    if not os.geteuid() == 0:

        # 获取当前工作目录
        current_directory = os.getcwd()

        # 获取 DISPLAY, XAUTHORITY, 和 DBUS_SESSION_BUS_ADDRESS 环境变量
        display_env = os.getenv('DISPLAY')
        xauthority_env = os.getenv('XAUTHORITY')
        dbus_env = os.getenv('DBUS_SESSION_BUS_ADDRESS')


        # 构建要执行的命令
        command = f"cd {current_directory} && {sys.executable} " + " ".join(sys.argv)


        pkexec_args = ["pkexec", "--user", "root", "env"]
        kv_pair_arr = [f"{key}={value}" for key, value in os.environ.copy().items()]
        pkexec_args.extend(kv_pair_arr)
        pkexec_args.extend(["bash", "-c", command])

        os.execvp("pkexec", pkexec_args)

    else:
        g_user = os.getlogin()
        app = QApplication(sys.argv)
        main_window = MainWindow()
        main_window.show()
        main_window.btn_master_control.clicked.connect(btn_master_control_event)
        main_window.btn_servant.clicked.connect(btn_Servant_event)
        sys.exit(app.exec_())
