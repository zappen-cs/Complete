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

import grp
import os
import pwd


def create_desktop_file(executable_path):
    # 获取文件名和图标路径
    file_name = os.path.basename(executable_path)
    image_path = os.path.join(os.getcwd(), "image", "icon.ico")
    desktop_file_content = f"""[Desktop Entry]
Version=1.0
Type=Application
Name={file_name}
Exec={executable_path}
Icon={image_path}
Path={os.getcwd()}
Terminal=false
"""
    # 定义桌面文件的路径
    desktop_file_name = f"{file_name}.desktop"
    desktop_path = os.path.join(os.path.expanduser("~"), "Desktop")
    desktop_path_zh = os.path.join(os.path.expanduser("~"), "桌面")
    app_desktop_path = os.path.join(os.path.expanduser("~"), ".local", "share", "applications", desktop_file_name)
    desktop_file_path = ""
    print(desktop_path, desktop_path_zh)
    if os.path.exists(desktop_path):
        desktop_file_path = os.path.join(desktop_path, desktop_file_name)
    if os.path.exists(desktop_path_zh):
        desktop_file_path = os.path.join(desktop_path_zh, desktop_file_name)
    # 写入桌面文件
    with open(desktop_file_path, 'w') as desktop_file:
        desktop_file.write(desktop_file_content)
    change_file_owner_and_permissions(desktop_file_path, os.getlogin(), "root")
    print(f"创建桌面图标: {desktop_file_path}")
    # 写入桌面图标
    with open(app_desktop_path, 'w') as desktop_file:
        desktop_file.write(desktop_file_content)
    print(os.getlogin())
    change_file_owner_and_permissions(app_desktop_path, os.getlogin(), "root")
    print(f"创建桌面图标: {app_desktop_path}")


def change_file_owner_and_permissions(file_path, user_name, group_name):
    try:
        # 设置文件权限为 755 (rwxr-xr-x)
        os.chmod(file_path, 0o755)
        print(f"成功更改文件权限为 755。")
    except Exception as e:
        print(f"操作失败: {e}")
def create_desktop_icons_for_executables():
    current_directory = os.getcwd()

    # 遍历当前目录
    for item in os.listdir(current_directory):
        item_path = os.path.join(current_directory, item)

        # 检查是否是可执行文件
        if os.path.isfile(item_path) and os.access(item_path, os.X_OK) and not (os.path.splitext(item_path)[1] == '.so'):
            create_desktop_file(item_path)

def create_X():
    os.system("pyinstaller -w -F Seamless.py --distpath .")
    os.system("rm -rf build *.spec")
if __name__ == "__main__":
    create_X()
    create_desktop_icons_for_executables()