import os


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
    # 给予执行权限
    os.chmod(desktop_file_path, 0o755)
    print(f"创建桌面图标: {desktop_file_path}")
    # 写入桌面文件
    with open(app_desktop_path, 'w') as desktop_file:
        desktop_file.write(desktop_file_content)
    # 给予执行权限
    os.chmod(app_desktop_path, 0o755)
    print(f"创建桌面图标: {app_desktop_path}")


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
