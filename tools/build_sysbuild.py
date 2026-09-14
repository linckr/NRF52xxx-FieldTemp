#!/usr/bin/env python3
"""sysbuild 构建包装（引号安全版）。

为什么需要这个脚本
------------------
`SB_CONFIG_BOOT_SIGNATURE_KEY_FILE` 是一个 **Kconfig 字符串**，Kconfig 要求值本身带引号，
也就是最终要传给 CMake 的参数必须形如：

    -DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE="C:/path/root-ec-p256.pem"      （含字面双引号）

问题在于这层引号要穿过 **shell → python → west → cmake** 四层：
- 在 Git Bash 里必须整体用单引号包住才不被吃掉；
- 在 PowerShell 里内层双引号会被剥离，于是拿到
  `malformed string literal in assignment to BOOT_SIGNATURE_KEY_FILE` 而构建中止
  （本工程真实踩过，见 build-v5.log）。

本脚本**不让 shell 参与引号处理**：自己设置环境变量、自己拼 sys.argv、直接调用 west 的
main()。无论从 PowerShell、Git Bash 还是 CI 调用，行为都一致。

用法
----
    python tools/build_sysbuild.py <build_dir> [额外 CMake 参数...]

例：
    python tools/build_sysbuild.py build-v5
    python tools/build_sysbuild.py build-stk5 -DCONFIG_INIT_STACKS=y

约定
----
- 源码目录固定为本仓库根（脚本的上一级目录）。
- board 固定 nrf52dk/nrf52810；用 `-p always` 保证干净重建。
- **签名私钥不入库**，按以下顺序解析（见下节）。

需要配置的环境变量
------------------
SIGN_KEY        MCUboot 镜像签名私钥（PEM）的绝对路径。**必填**，
                否则退回 `<仓库根>/keys/root-ec-p256.pem`（该目录已被 .gitignore 排除）。
NCS_ROOT        Nordic Connect SDK 安装根目录（默认 `C:\\ncs`）。
NCS_VERSION     NCS 版本目录名（默认 `v3.2.1`）。
NCS_TOOLCHAIN   工具链目录（默认 `<NCS_ROOT>/toolchains/<hash>/opt`，hash 随安装而异，
                建议在本机用 `west --version` 或 `ls <NCS_ROOT>/toolchains` 查得后显式设置）。
"""

import os
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

NCS_ROOT = os.environ.get("NCS_ROOT", r"C:\ncs")
NCS_VERSION = os.environ.get("NCS_VERSION", "v3.2.1")
TOOLCHAIN = os.environ.get(
    "NCS_TOOLCHAIN", os.path.join(NCS_ROOT, "toolchains", "66cdf9b75e", "opt"))
ZEPHYR_BASE = os.path.join(NCS_ROOT, NCS_VERSION, "zephyr")
CMAKE_PREFIX = os.path.join(NCS_ROOT, NCS_VERSION, "zephyr", "share",
                            "zephyr-package", "cmake").replace("\\", "/")

BOARD = "nrf52dk/nrf52810"

# 签名私钥：**凭据，不入库**。优先 SIGN_KEY，其次 <repo>/keys/root-ec-p256.pem。
DEFAULT_KEY = os.path.join(REPO, "keys", "root-ec-p256.pem")


def _ensure_safe_git_config():
    """给 west/zephyr 的 git 调用准备一个最小 gitconfig。

    Zephyr 的版本生成会调用 git；若仓库属主与当前用户不一致（例如从别的沙箱/账户
    复制过来的工作副本），git 会以 `dubious ownership` 拒绝操作，而版本号会**静默**
    退回 `0.0.0+0`。这里写一个只含 `safe.directory = *` 的临时配置并通过
    GIT_CONFIG_GLOBAL 生效，避免把任何本机绝对路径写进仓库。
    需要覆盖时设置 GIT_CONFIG_GLOBAL_FILE。
    """
    path = os.environ.get("GIT_CONFIG_GLOBAL_FILE")
    if path and os.path.isfile(path):
        os.environ["GIT_CONFIG_GLOBAL"] = path
        return path
    path = os.path.join(tempfile.gettempdir(), "zephyr-safe-gitconfig")
    if not os.path.isfile(path):
        with open(path, "w", encoding="utf-8") as f:
            f.write("[safe]\n\tdirectory = *\n")
    os.environ["GIT_CONFIG_GLOBAL"] = path
    return path


def main():
    args = sys.argv[1:]
    if not args or args[0] in ("-h", "--help"):
        print(__doc__)
        return 0 if args else 2

    build_dir = args[0]
    extra = args[1:]

    key = os.environ.get("SIGN_KEY") or DEFAULT_KEY
    if not os.path.isfile(key):
        sys.exit(
            "签名私钥不存在: %s\n"
            "私钥属凭据、不入库。请用环境变量 SIGN_KEY 指向 PEM 文件，\n"
            "或把它放到 %s（该目录已被 .gitignore 排除）。" % (key, DEFAULT_KEY))

    if not os.path.isabs(build_dir):
        build_dir = os.path.join(REPO, build_dir)

    os.environ["PATH"] = os.pathsep.join(
        [os.path.join(TOOLCHAIN, "bin"),
         os.path.join(TOOLCHAIN, "bin", "Scripts"),
         os.environ.get("PATH", "")])
    os.environ["ZEPHYR_BASE"] = ZEPHYR_BASE
    os.environ["CMAKE_PREFIX_PATH"] = CMAKE_PREFIX
    _ensure_safe_git_config()

    # 关键：这里拼出的字符串**已经带字面双引号**，后续不再经过任何 shell
    sign_arg = '-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE="%s"' % key

    argv = ["west", "build", "-p", "always", "-b", BOARD, "--sysbuild",
            "-d", build_dir, REPO, "--", sign_arg]
    argv.extend(extra)

    print("build_dir = %s" % build_dir)
    print("extra     = %s" % (" ".join(extra) if extra else "(无)"))
    sys.stdout.flush()

    sys.argv = argv
    from west.app.main import main as west_main
    return west_main()


if __name__ == "__main__":
    sys.exit(main())
