# Android 客户端

本目录是直接可构建的 Gradle 根目录，合并来源为 [linckr/PandaThemperature-Android](https://github.com/linckr/PandaThemperature-Android/commit/be12069abae643085b3ed9773ce1591d462d1e2d) 的 source/。源码及测试逐文件一致；不包含 .git、构建产物、手机数据、私有配置或源码压缩包。

使用 JDK17 和本机 Android SDK，在本目录运行 `gradlew.bat testDebugUnitTest assembleDebug`。SDK 路径可由 ANDROID_HOME 或本地忽略的 local.properties 设置。OTA/签名凭据通过 ota.properties / keystore.properties 注入，模板已提供；不可提交真实文件。

接手与真机回归见 [HANDOFF.md](HANDOFF.md)，Firmware/Android 公共协议和架构见根目录 [CODEX_START_HERE.md](../CODEX_START_HERE.md) 与 [PROTOCOL.md](../PROTOCOL.md)。后续开发以本统一项目为入口；向独立 Android 上游回传时同步对应 source/ 文件，不在两份副本各自开发。
