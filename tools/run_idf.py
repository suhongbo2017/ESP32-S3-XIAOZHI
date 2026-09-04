"""Windows 下运行 ESP-IDF idf.py 的包装脚本。

解决 Git-Bash/MSYS 环境下 idf.py 拒绝运行的问题：
- 自动导出 IDF_PATH / IDF_TOOLS_PATH / PATH / ESP_IDF_VERSION 等环境变量
- 通过系统代理下载组件（可被 HTTP_PROXY / HTTPS_PROXY 覆盖）

用法：
    python tools/run_idf.py -DSDKCONFIG_DEFAULTS="..." set-target esp32s3
    python tools/run_idf.py build
    python tools/run_idf.py -p COM4 flash

可用环境变量覆盖默认路径：
    IDF_PATH       (默认 C:\\esp\\v6.1\\esp-idf)
    IDF_TOOLS_PATH (默认 D:\\Espressif)
"""
import os
import subprocess
import sys

IDF_PATH = os.environ.get("IDF_PATH", r"C:\esp\v6.1\esp-idf")
IDF_TOOLS_PATH = os.environ.get("IDF_TOOLS_PATH", r"D:\Espressif")
PYTHON = os.environ.get(
    "IDF_PYTHON_ENV_PATH",
    os.path.join(IDF_TOOLS_PATH, "python_env", "idf6.1_py3.11_env", "Scripts", "python.exe"),
)

base_env = {k: v for k, v in os.environ.items() if k != "MSYSTEM"}
base_env["IDF_PATH"] = IDF_PATH
base_env["IDF_TOOLS_PATH"] = IDF_TOOLS_PATH
base_env.setdefault("HTTP_PROXY", "http://127.0.0.1:7897")
base_env.setdefault("HTTPS_PROXY", "http://127.0.0.1:7897")

export = subprocess.run(
    [PYTHON, os.path.join(IDF_PATH, "tools", "idf_tools.py"), "export", "--format=key-value"],
    env=base_env, capture_output=True, text=True,
)
if export.returncode != 0:
    print(export.stdout[-3000:], file=sys.stderr)
    print(export.stderr[-2000:], file=sys.stderr)
    sys.exit(export.returncode)

env = dict(base_env)
for line in export.stdout.splitlines():
    if "=" not in line:
        continue
    k, v = line.split("=", 1)
    env[k.strip()] = v.strip().strip('"')

cmd = [PYTHON, os.path.join(IDF_PATH, "tools", "idf.py")] + sys.argv[1:]
sys.exit(subprocess.call(cmd, env=env, cwd=os.getcwd()))