# 小珠看着你

使用 C++17、Qt 5.15 和 CMake 构建的屏幕桌宠项目。当前已完成聊天 MVP。

## 构建

Qt 和编译器必须匹配。当前工程使用 Qt 5.15 的 MSVC 版本，因此请在 **x64 Native Tools Command Prompt for VS** 中执行：

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH=D:/Qt/5.15.2/msvc2019_64 `
  -DZHU_SCREEN_PET_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

如果使用 Visual Studio 生成器，可以省略 `-G Ninja`，并在构建时指定配置：

```powershell
cmake -S . -B build-vs `
  -DCMAKE_PREFIX_PATH=D:/Qt/5.15.2/msvc2019_64 `
  -DZHU_SCREEN_PET_BUILD_TESTS=ON
cmake --build build-vs --config Debug
ctest --test-dir build-vs -C Debug --output-on-failure
```

## Release 发布

Windows 便携发布包必须使用 Release 配置。配置并验证完成后，构建
`zhu_screen_pet_portable` 目标：

```powershell
cmake -S . -B build-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=D:/Qt/5.15.2/msvc2019_64 `
  -DZHU_SCREEN_PET_BUILD_TESTS=ON
cmake --build build-release
ctest --test-dir build-release --output-on-failure
cmake --build build-release --target zhu_screen_pet_portable
```

可发布目录生成在 `build-release/dist/Release/`。该目录包含主程序、默认
JSON 配置、Qt 运行库与插件、OpenSSL 以及可再分发的 VC++ CRT DLL，整个目录一起复制
到其他 64 位 Windows 电脑即可。API Key 保存在 Windows Credential Manager 中，
换电脑后需要重新配置。

## 当前内容

- 最小 Qt Widgets 应用和主窗口
- C++17 与 CMake 配置
- Qt 资源文件和运行时应用图标
- Debug/Release 构建支持
- 分离的 QtTest 主回归测试与旧数据迁移测试
- 基础源码目录：`app`、`infrastructure`、`model`、`memory`、`ui`
- Phase 1 基础服务：路径、JSONL 日志、QSettings、SQLite/WAL、后台任务、异步 HTTP、Windows 凭据存储接口、窗口位置管理和系统托盘
- Phase 2 模型基础：异步 `ChatProvider`、Mock Provider、OpenAI-compatible Provider、DeepSeek Provider、JSON 解析、超时/重试/退避和模型日志
- Phase 3 记忆层：SQLite 会话与消息持久化、记忆检索、最近上下文和 token 预算裁剪
- Phase 4 聊天 MVP：历史恢复、输入发送、SSE 流式显示、取消、失败重试、复制回复、人格提示和桌宠状态
- 从旧 `ScreenPet/Screen Pet` 数据目录和 Credential service 幂等迁移配置、会话与密钥，迁移过程不覆盖新数据且保留旧数据

应用随附的磁盘配置默认启用 Mock Provider，因此无需联网即可测试完整聊天闭环。
真实 Provider 的 API Key 从 Windows Credential Manager 读取，不会写入 JSON 或日志。

当前尚未实现透明桌宠动画、截图、OCR 和屏幕观察功能，这些属于后续阶段。

人格参数和模型错误提示同样由磁盘配置驱动，项目模板位于
[app-settings.json](D:/zhu_screen_pet/config/app-settings.json)。

## 模型配置

模型创建采用 `ChatProviderFactory`，运行时切换由 `ProviderManager` 管理。所有 Provider
参数来自 [model-providers.json](D:/zhu_screen_pet/config/model-providers.json)，例如：

```json
{
  "version": 1,
  "active_profile": "deepseek-chat",
  "profiles": [
    {
      "id": "deepseek-chat",
      "provider_type": "deepseek",
      "display_name": "DeepSeek Chat",
      "base_url": "https://api.deepseek.com",
      "model": "deepseek-v4-flash",
      "credential_service": "zhu_screen_pet",
      "credential_account": "deepseek-api-key",
      "timeout_ms": 30000,
      "max_retries": 3,
      "retry_base_delay_ms": 1000
    }
  ]
}
```

构建时该文件会复制到 `build/config/`。程序首次运行时再复制到用户应用数据目录的
`config/model-providers.json`，后续读取和修改的都是用户目录中的文件。也可以通过环境变量
`ZHU_SCREEN_PET_MODEL_CONFIG` 指定另一个配置文件。

人格和提示文案使用同目录的 `app-settings.json`，也可以通过环境变量
`ZHU_SCREEN_PET_APP_CONFIG` 指定外部文件。修改该文件后重启程序即可生效。
该文件的 `memory` 段控制最近消息条数、相关历史条数、长期记忆条数和总上下文 token 预算。

`app-settings.json` 的 `ui` 段支持替换外部外观资源：

```json
{
  "ui": {
    "app_icon_path": "assets/app-icon.png",
    "pet_avatar_path": "assets/pet-avatar.png",
    "conversation_avatar_path": "assets/history-avatar.png"
  }
}
```

`app_icon_path` 控制运行时窗口和系统托盘图标，`pet_avatar_path` 控制桌宠主体，
`conversation_avatar_path` 单独控制会话历史中的桌宠头像。可使用绝对路径，或使用相对于程序所在目录的路径；Windows 下推荐
在 JSON 中使用 `D:/images/pet.png` 形式。路径为空或图片无法读取时，程序会回退到
内置图标或默认文字形象。修改运行时配置后需重启程序。

`ModelProviderConfig` 不补 URL、模型名或厂商默认值；磁盘配置缺少必填字段时会明确报错。
API Key 使用 `credential_service + credential_account` 从 Windows Credential Manager 获取。
`ProviderManager::switchProvider()` 仅在没有运行中请求时切换。
