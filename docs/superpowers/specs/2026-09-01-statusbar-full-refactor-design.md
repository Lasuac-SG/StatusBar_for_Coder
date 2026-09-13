# StatusBar 全量重构设计

## 目标

在保留 Windows 顶部状态栏、系统托盘、可拖拽组件和 CPU 详情界面的前提下，完成一次面向长期演进的重构。结果必须优先满足：空闲时低唤醒、组件按需创建、单一扩展点、平台与业务解耦、配置可靠、代码短小明确。支持 Windows 10/11、Qt 6.8 及以上版本和多个同类型组件实例。

本次不实现运行时 DLL 插件、第三方插件 ABI、设置页面或新的业务组件。组件保持编译期静态注册，以避免动态加载、版本兼容和部署复杂度。

## 总体架构

程序由组合根、Qt 应用层、平台集成层、组件系统和纯核心逻辑组成：

- `main.cpp` 只创建依赖、初始化应用、连接退出回调并返回事件循环状态。
- `UI::QtApplication` 拥有 `QGuiApplication`、`QQmlApplicationEngine`、根 `QQuickWindow` 和 `WidgetModel`。
- `Platform::WindowsShellIntegration` 组合单实例锁、AppBar 和托盘。它接受根窗口的 HWND，不依赖 Widget 或配置模块。
- `Core::ConfigRepository` 负责版本化配置、迁移、校验和原子保存。
- `Core::LayoutEngine` 是无 Qt、无 Win32 状态的纯算法，负责归一化、拖放和碰撞事务。
- `Widgets::WidgetRegistry` 保存完整组件描述符。每个描述符声明稳定类型 ID、默认跨度、QML URL 和 ViewModel 工厂。
- Widget ViewModel 是独立 `QObject` 实例。共享硬件采样由注入服务提供，不使用可变全局单例。

现有职责不完整的 `WindowManager` 和 `IWindowAdapter` 将被移除。Core 层不再包含 `windows.h` 或具体托盘/AppBar 类型。

## 组件模型

`WidgetDescriptor` 包含 `type`、`span`、`qmlUrl` 和工厂。工厂接收 `WidgetContext` 与实例配置，返回一个 QObject ViewModel。`WidgetModel` 向 QML 暴露以下角色：

- `instanceId`
- `type`
- `slot`
- `span`
- `qmlUrl`
- `viewModel`

Loader 直接读取 `qmlUrl`，并在创建组件时传入 `viewModel` 与编辑状态。新增组件只需要新增组件目录及一条自注册描述，不需要修改主 QML、Qt 应用适配器或中央字符串分支。

每个配置条目对应独立 ViewModel，因此允许多个时钟、多个 CPU 组件以及未来的不同实例设置。CPU ViewModel 订阅同一个 `CpuService`，不会因为实例数增加而重复访问系统计数器。

## 更新与性能策略

删除全局一秒 `updateAll()` 轮询：

- `CpuService` 使用一个粗粒度 QTimer，默认每两秒采样一次。
- `ClockViewModel` 根据当前时间计算到下一分钟边界的单次定时器；更新后再次安排。
- CPU 详情弹窗通过 Loader 首次打开时创建。
- 图表 Connections 仅在弹窗可见时启用；静态网格与动态折线分层绘制。
- 属性只在值变化时发送通知；常量硬件属性使用 `CONSTANT`。
- CPU 历史使用固定容量环形缓冲，并缓存供 QML 读取的列表。

不引入后台线程。当前采样调用很短，线程切换和同步成本高于收益；若未来增加昂贵采样器，再由服务层单独异步化。

## 配置与迁移

正式配置路径为 `QStandardPaths::AppConfigLocation/config.json`。格式升级为：

```json
{
  "version": 1,
  "widgets": [
    {
      "id": "稳定 UUID",
      "type": "Clock",
      "slot": 0,
      "settings": {
        "format": "HH:mm",
        "timeZone": "system"
      }
    }
  ]
}
```

首次启动时，如果新位置不存在配置，依次检查可执行文件目录和当前工作目录中的旧格式文件。找到后：

1. 将原始内容复制到新目录中的 `config.legacy.backup.json`，不修改源文件。
2. 在内存中完整解析和校验旧配置。
3. 将 `name` 映射为 `type`，为每项生成 UUID，保留 slot，并创建空 settings。
4. 使用 `QSaveFile` 原子写入新格式。

解析始终先进入临时对象，只有整份文档通过结构校验才替换当前状态。损坏配置不会被默认值覆盖，而会保留并返回可诊断错误。未知组件条目原样保留；模型不显示它们，但保存已知组件布局时会将未知条目合并回文档。

## 布局

`LayoutEngine` 接收组件区间、目标槽位和总槽数，返回完整的新布局或失败。一次拖放是原子事务：所有组件都必须满足非负、`slot + span <= totalSlots` 且两两不重叠，才能提交并发送模型通知。

窗口宽度或 DPI 改变时，QML 将新的槽位总数提交给模型。布局引擎按稳定顺序将越界或重叠组件放到最近可用位置；只有布局实际改变时才保存。拖动期间禁用位置 Behavior，释放后才播放吸附动画。

## Windows 平台集成

AppBar 使用真实 `QQuickWindow` 的 HWND 注册，不再创建 1×1 代理窗口。实现遵循完整协商流程：

1. `ABM_NEW`
2. 根据窗口所在显示器构造候选矩形
3. `ABM_QUERYPOS`
4. 保留系统返回的 top，设置所需高度
5. `ABM_SETPOS`
6. 使用最终矩形定位实际窗口

平台对象通过 Qt 原生事件过滤器处理 AppBar 回调、`TaskbarCreated`、显示器变化和 DPI 变化。托盘在 Explorer 重启后重新添加并设置通知版本。所有 Win32 返回值都转换为明确的初始化结果；失败会回滚已经获取的资源。

应用使用命名 Mutex 阻止重复实例。所有持有 HANDLE、HWND、菜单或 Shell 注册状态的类型都不可复制，并通过 RAII、幂等 shutdown 管理生命周期。

## QML 与构建

项目改用 `qt_add_executable`、`qt_add_qml_module` 和 `loadFromModule()`。QML 资源由模块生成 qmldir、类型信息和缓存编译产物。主窗口不再依赖 context property；delegate 声明显式 required properties，并使用 Bound component behavior。

主题集中管理颜色、字体和尺寸。CPU 指标卡抽为复用组件；图标取消不必要的 mipmap 和双倍纹理。默认使用 Qt 场景图文本渲染，避免强制 NativeRendering。

CMake 显式列出源文件，按 core、platform、widgets 和应用目标表达依赖；关闭未使用的 AUTOUIC 和 C++ 扩展，启用严格告警。Release 可在编译器支持时启用 IPO/LTO。发布使用 Qt CMake 部署脚本和 CPack，只携带导入扫描确认的运行库；不再跟踪预构建 `dist.zip`。

简单 JSON 读写改用 Qt Core 的 QJson API，删除 nlohmann/json 依赖。无调用者且重复的 `platform/cpu_sampler.h` 将被移除。

## 错误处理

配置加载、QML 根对象创建、AppBar 注册和托盘创建都返回可观察结果。不可恢复的启动错误由 `main` 记录并返回非零退出码；托盘失败可降级运行，但必须记录；AppBar 失败默认视为启动失败，避免显示窗口却未保留桌面区域。

事件循环退出码原样返回。跨 Win32 回调边界不传播 C++ 异常。

## 测试

使用 Qt Test 建立以下自动化测试：

- 旧配置迁移、备份、损坏配置、原子保存和未知组件保留。
- 多实例配置与 ViewModel 独立性。
- 布局交换、宽窄组件、多组件碰撞、越界及分辨率缩小后的归一化。
- CPU 使用率纯计算、首次样本和失败状态。
- AppBar 顶部矩形计算，确认保留系统协商后的 top。
- Registry 描述符和模型角色。
- QML lint、QML 模块加载和无 context property 启动。

Windows Shell 的真实注册保留为手工烟雾测试，因为自动测试不应改变开发机工作区。完整验证包括 Debug/Release 构建、CTest、qmllint、启动/退出、Explorer 重启恢复、DPI/分辨率变化和发布目录检查。

## 验收标准

- 新增 Widget 不修改主 QML、QtApplication 或字符串分派代码。
- 两个同类型组件可以同时存在，CPU 系统采样次数不随实例数增加。
- 空闲时没有全局一秒组件轮询，时钟仅在分钟边界更新。
- 配置损坏不会覆盖原文件，旧配置自动迁移且有备份。
- 拖放和显示器变化后不存在重叠或越界组件。
- AppBar、托盘和单实例资源均由不可复制 RAII 类型管理。
- Release 是 Windows GUI 子系统，QML 已缓存编译，测试和 lint 通过。
- 部署流程可由 CMake 一条命令复现，发布目录不含测试、SQL、QML 调试器或重复插件。
