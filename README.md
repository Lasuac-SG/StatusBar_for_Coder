# StatusBar for Coder

使用 C++20、Qt Quick 与 Windows AppBar API 实现的 Windows 顶部状态栏。项目强调低空闲开销、显式所有权、模块化组件和可复现部署。

## 环境要求

- Windows 10 或 Windows 11。
- Qt 6.8 或更高版本，包含 Core、Gui、Qml、Quick；运行测试还需要 Qt Test。
- CMake 3.21 或更高版本、Ninja，以及与所安装 Qt ABI 匹配的 C++20 编译器（MSVC 或 MinGW-w64 GCC/Clang）。
- `cmake`、`ninja`、`git` 和 Qt 工具应可从命令行找到。以下 PowerShell 命令会从 `qtpaths6` 自动取得 Qt 安装前缀：

```powershell
$qtRoot = Split-Path -Parent (Split-Path -Parent (Get-Command qtpaths6).Source)
```

以下命令均在仓库根目录执行。

## Debug 构建与测试

```powershell
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON "-DCMAKE_PREFIX_PATH=$qtRoot"
cmake --build build-debug --parallel
ctest --test-dir build-debug --output-on-failure
cmake --build build-debug --target all_qmltyperegistrations
cmake --build build-debug --target all_qmllint
```

## Release 构建、安装与打包

```powershell
$env:SOURCE_DATE_EPOCH = (git show -s --format=%ct HEAD).Trim()
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF "-DCMAKE_PREFIX_PATH=$qtRoot"
cmake --build build-release --parallel
cmake --install build-release --prefix package-release --strip
cmake --build build-release --target package
```

`SOURCE_DATE_EPOCH` 固定为当前 Git 提交时间，并由后续构建和 `package` 命令继承，用于生成可比较的发行物。`package-release/` 是可直接分发的精简目录，包含 `StatusBar_for_Coder.exe`、所需 Qt 运行库、QML 模块及平台插件。`package` 目标在 `build-release/` 中生成 `StatusBar_for_Coder-0.2.0-windows-<架构>.zip`。部署内容由 Qt 的 QML 导入扫描与 CMake 部署脚本生成，不需要手工复制 DLL。

## 配置与迁移

程序设置组织名 `StatusBarForCoder`、应用名 `StatusBar_for_Coder`。正式配置是 `QStandardPaths::AppConfigLocation/config.json`；在 Windows 上具体为：

```text
%LOCALAPPDATA%\StatusBarForCoder\StatusBar_for_Coder\config.json
```

若正式配置不存在，程序依次查找：

1. `StatusBar_for_Coder.exe` 所在目录的 `config.json`；
2. 启动时当前工作目录的 `config.json`。

找到旧格式后，原始字节会先保存为正式配置目录中的 `config.legacy.backup.json`，源文件不被修改，再将迁移后的版本化配置原子写入 `config.json`。已有但损坏、不可读或结构非法的配置不会被默认值覆盖；程序保留原文件、记录错误并终止启动。未知 Widget 条目也会保留，以免新旧版本往返时丢失数据。

## 架构与性能

- `Core` 负责无 UI 的配置与布局事务；`Widgets` 负责描述符和每实例 ViewModel；`Platform` 以不可复制 RAII 类型管理 Mutex、AppBar、托盘和采样资源；`UI` 管理 Qt/QML 生命周期。
- 每个配置项创建独立 ViewModel，因此多个 Clock 或多个 Cpu 实例可同时存在并持有各自设置。
- 所有 Cpu 实例共享一个按需 `CpuService`；首个消费者出现时开始低频采样，最后一个消费者离开时停止，不会因实例数量增加而重复访问系统计数器。
- Clock 仅在分钟边界更新；CPU 详情与图表按需创建和刷新，不存在全局一秒 Widget 轮询。
- QML 作为静态模块编译并生成缓存代码；新增内置 Widget 不需要修改主 QML 或 Qt 应用层。

## 添加一个编译期 Widget

以类型 `Memory` 为例，需要接入四类制品：

1. C++ ViewModel：新增 `src/widgets/memory/memory_view_model.h` 和 `memory_view_model.cpp`，继承 `Widgets::WidgetViewModel`。每个配置实例保存独立状态；共享采样通过 `WidgetContext` 注入服务，不新增全局单例。
2. QML 视图：新增 `src/widgets/memory/MemoryWidget.qml`。`WidgetHost` 总会通过 `Loader.setSource()` 传入 `viewModel`、`editingWindow`、`editing` 和 `requestEditing`，因此根对象必须完整声明这四个属性；推荐契约如下：

   ```qml
   import QtQuick
   import QtQuick.Window
   import StatusBar

   Item {
       required property MemoryViewModel viewModel
       required property Window editingWindow
       required property bool editing
       required property var requestEditing
   }
   ```

   `viewModel` 必须使用本组件的具体 QML 类型；其余属性即使暂时不用也必须声明，以保持宿主协议稳定。
3. QML 类型声明：在 `src/ui/qml_types.h` 中使用 `QML_FOREIGN`、`QML_NAMED_ELEMENT(MemoryViewModel)` 和 `QML_UNCREATABLE` 暴露 ViewModel。
4. Widget 描述符：在 `src/widgets/registry_setup.cpp` 的描述符列表中注册稳定类型 ID、默认跨度、资源 URL `qrc:/qt/qml/StatusBar/MemoryWidget.qml` 和 ViewModel 工厂。这是业务层唯一的 Widget 注册入口。

最后必须在根 `CMakeLists.txt` 中显式登记构建输入：把 ViewModel 的 `.h/.cpp` 加入 `statusbar_core` 的 `target_sources()`，把 `MemoryWidget.qml` 加入 `qt_add_qml_module(StatusBarQml ... QML_FILES ...)`。随后执行 Debug 构建、CTest、`all_qmltyperegistrations` 和 `all_qmllint`。

本项目只支持编译期静态 Widget 注册；运行时 DLL 插件及第三方插件 ABI 不在当前范围内。
