# 小珠看着你

一个使用 C++17、Qt 6 和 CMake 开发的 Windows 屏幕桌宠。项目目前达到
**v0.1.0 首个可用便携版**状态：可以解压运行、离线体验，也可以配置兼容
OpenAI Chat Completions 的远程文本或视觉模型。

> 当前版本适合首轮发布和小范围使用。尚未提供安装器、自动更新和代码签名，
> Windows SmartScreen 可能会对未签名程序显示提醒。

## 功能

### 桌宠与交互

- 透明置顶桌宠、系统托盘、可拖动和等比例缩放。
- 自动隐藏的操作栏和聊天输入栏，附属窗口会跟随桌宠移动并在屏幕边缘换位。
- 流式回复气泡、完成后自动关闭、悬停暂停、手动关闭和长内容滚动。
- 根据本地时间显示启动问候，称呼取自设置中的“对你的称呼”。
- 空闲状态每分钟随机切换，思考、回复、错误状态会优先显示。
- 首次启动向导会引导配置称呼、模型、API Key、截图权限和开机自启。
- 支持按状态加载 PNG 序列：空闲眨眼、思考疑惑、回复张嘴和升级开心；缺少
  对应素材时自动回退静态立绘。
- 将全局键盘按下和鼠标点击统一累计为一个输入次数，不记录滚轮、具体按键、文本、
  坐标或活动窗口；上限 100000，每 10 分钟和正常退出时保存。
- 0–100 级在线挂机等级，圆环显示进度；每分钟刷新、每 5 分钟及退出时保存，
  离线期间不增长。
- 0–10 级好感度圆环，固定每 1000 经验升一级，满级显示 `LV.MAX`；好感 1/4/7
  级分别解锁人格主动程度 1/2/3，赠与礼物可增加好感经验。
- 操作栏提供独立的礼物/食物商店和背包窗口；输入次数作为购买余额，购买后立即
  扣减余额并保存库存，赠与后立即消耗物品并应用效果。
- 0–100 饱食度黄色圆环，在线期间每 30 分钟降低 5 点；赠与食物可恢复配置中
  指定的饱食度。

### 聊天与模型

- 默认提供 Mock 模型，无需网络或 API Key 即可体验聊天闭环。
- 支持 OpenAI-compatible 和 DeepSeek 配置、流式/非流式响应、取消、超时、
  重试和错误分类。
- 支持在设置中切换模型、测试连接并保存 API Key。
- API Key 保存到 Windows Credential Manager，不写入 JSON 配置、SQLite 或日志。
- 可在首次启动向导或设置页启用当前 Windows 用户的登录自启，无需管理员权限。

### 屏幕视觉

- 支持定时截图、手动截图和随本次用户消息附带截图。
- 图片在发送前缩放压缩，并按照 OpenAI-compatible 的 Base64 `image_url` 格式提交。
- 自动视觉请求具有画面去重、8 MiB 请求体限制、超限二次压缩、后台队列上限和
  连续失败熔断。
- 截图功能默认关闭；启用时会明确提醒截图将发送给当前配置的远程模型。

### 会话与记忆

- SQLite/WAL 保存会话、消息、短期记忆、长期记忆和屏幕观察摘要。
- 会话历史支持滚动加载、切换、归档和永久删除。
- 上下文按最近消息、相关历史、长期事实和最新观察组织，并受消息数和 Token
  预算约束。
- 消息达到条数或 Token 阈值后，后台生成滚动摘要并提取长期事实；失败时保留
  原消息并延迟重试。
- 长期事实支持分类、置信度/重要性过滤和精确去重。
- 记忆管理界面支持搜索、分类过滤、查看来源、编辑、单条删除以及清空短期或
  长期记忆。
- 短期记忆默认保留 7 天、最多 1000 条；成功摘要后的旧原始消息按策略清理。

## 普通用户使用

### 系统要求

- Windows 10/11 64 位。
- 不需要单独安装 Qt、OpenSSL 或 Visual C++ 运行库；便携包已包含运行所需文件。
- 使用远程模型时需要网络、对应服务的 API Key，以及支持所需输入类型的模型。

### 开始使用

1. 下载 `小珠看着你-v0.1.0-win64-portable.zip`。
2. 将压缩包完整解压到一个可写目录，不要只取出 EXE。
3. 运行 `小珠看着你.exe`。
4. 按首次启动向导配置称呼和模型；默认 Mock 模型可以直接离线聊天。
5. 如需远程模型，也可以稍后打开“设置”，选择或填写模型配置和 API Key，先测试连接，
   再保存设置。
6. 只有在确认当前模型支持图片输入并理解隐私风险后，才开启截图或随消息附图。

用户数据不会写入便携包目录，而是保存在 Windows 用户应用数据目录中，通常为：

```text
%APPDATA%\zhu_screen_pet\zhu_screen_pet\
├── config\
├── database\
├── logs\
└── captures\
```

更换电脑时 API Key 不会随便携包迁移，需要重新配置。

## 隐私说明

- 普通会话、记忆和观察摘要保存在本机 SQLite 数据库。
- API Key 保存在 Windows Credential Manager。
- API Key、Authorization Header、原始截图和 Base64 图片不会写入应用日志。
- 输入统计只保存一个累计数字，不保存键盘内容、鼠标位置或所操作的软件名称。
- 开启截图后，截图内容会发送给当前选择的远程模型服务；发送前请关闭或遮挡
  隐私页面。
- 当前版本不具备自动识别密码、聊天窗口、支付页面等敏感内容的能力。

## 当前限制

- 仅提供 Windows x64 便携版，没有安装器、卸载器、自动更新和代码签名。
- 当前只捕获主显示器，不支持指定显示器、指定窗口或完整多屏工作流。
- 没有本地视觉模型和 OCR，视觉能力取决于用户配置的远程模型。
- 自动摘要和长期事实提取同样需要远程模型；Mock 模式不会执行模型摘要。
- PNG 动作播放框架已经接入，但仓库暂未包含四组正式动作素材；素材目录和命名
  规范见 `resources/animations/README.md`。
- 当前只做精确去重，不包含向量检索、语义冲突合并或长期记忆版本历史。
- 暂不提供数据库备份恢复和进程崩溃后的未完成请求恢复。
- 自动化测试使用 Mock/本地模拟服务；不同远程服务的模型名称、限流和视觉格式
  仍需用户按照服务商文档确认。

## 从源码构建

### 依赖

- CMake 3.22 或更高版本。
- Visual Studio 2022 C++ x64 工具链。
- 与 MSVC 工具链匹配的 Qt 6.8，组件包括 Core、Gui、Widgets、Network、Sql、Test。
- Ninja（使用下列命令时）。

请在 **x64 Native Tools Command Prompt for VS 2022** 或已经加载 MSVC 环境的
PowerShell 中执行。根据本机安装位置调整 `CMAKE_PREFIX_PATH`。

### Debug

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64 `
  -DZHU_SCREEN_PET_BUILD_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

### Release

```powershell
cmake -S . -B build-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64 `
  -DZHU_SCREEN_PET_BUILD_TESTS=ON
cmake --build build-release --parallel 4
ctest --test-dir build-release --output-on-failure
```

## 生成便携发布包

构建前先退出正在运行的 Release 程序，否则 Windows 会锁定 EXE，导致链接时报
`LNK1104` 或 `LNK1168`。

```powershell
cmake --build build-release --target zhu_screen_pet_portable --parallel 4
```

发布目录为：

```text
build-release/dist/Release/
```

该目录包含主程序、默认配置、头像资源、Qt 运行库和插件、OpenSSL 以及 MSVC x64
运行库。发布时必须压缩并发送目录内的全部内容：

```powershell
Compress-Archive `
  -Path ".\build-release\dist\Release\*" `
  -DestinationPath ".\小珠看着你-v0.1.0-win64-portable.zip" `
  -Force
```

`windeployqt` 如果提示缺少 `translations/catalogs.json`，在本项目使用
`--no-translations` 的情况下不影响便携包运行。

## 配置文件

发布模板位于：

- [config/model-providers.json](config/model-providers.json)：模型列表和当前配置。
- [config/app-settings.json](config/app-settings.json)：人格、记忆阈值、界面和截图设置。
- [config/shop-catalog.json](config/shop-catalog.json)：礼物和食物商品目录；当前模板
  内置基础礼物和食物，也可以按需要调整商品与数值。

首次运行时，程序会将缺少的模板复制到用户应用数据目录。后续设置修改的是用户
目录中的配置，不会修改便携包模板。

模型 JSON 只保存 Credential Manager 的 service/account 标识，不允许保存
`api_key` 或 `token` 字段。也可以用以下环境变量指定外部配置：

- `ZHU_SCREEN_PET_MODEL_CONFIG`
- `ZHU_SCREEN_PET_APP_CONFIG`

商店配置固定使用 `version: 1`，并包含 `gifts` 和 `foods` 两个数组。两类商品都需要
`id`、`emoji`、`name`、`price`；礼物额外使用 `affection_gain`，食物额外使用
`satiety_gain`。例如：

```json
{
  "version": 1,
  "gifts": [
    {"id": "flower", "emoji": "🌸", "name": "小花", "price": 100,
     "affection_gain": 50}
  ],
  "foods": [
    {"id": "cake", "emoji": "🍰", "name": "蛋糕", "price": 80,
     "satiety_gain": 20}
  ]
}
```
