# StatusBar Full Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the Windows top status bar around testable core logic, reliable versioned configuration, per-instance widgets, shared low-frequency services, a real HWND-backed AppBar, typed cached QML, and reproducible deployment.

**Architecture:** `main.cpp` is the composition root. Pure `Core` units own layout and configuration rules, `Widgets` owns descriptors and per-instance QObject view models, `Platform` owns non-copyable Windows resources, and `UI::QtApplication` owns Qt/QML lifecycle. The implementation removes mutable global singletons and the global one-second widget poll.

**Tech Stack:** C++20 without compiler extensions, Qt 6 Core/Gui/Qml/Quick/Test, Win32 Shell API, CMake Qt QML module support, CTest, QSaveFile/QJson.

---

## File map

New focused units:

- `src/core/layout_engine.{h,cpp}` — pure slot normalization and transactional drops.
- `src/core/config_repository.{h,cpp}` — versioned JSON, migration, backup, atomic persistence.
- `src/platform/cpu_service.{h,cpp}` — one shared sampler, timer, topology and history.
- `src/platform/cpu_usage.h` — pure CPU delta calculation.
- `src/platform/cpu_data_source.h` — injectable sampler boundary used by CpuService.
- `src/platform/appbar.{h,cpp}` — HWND AppBar registration and pure rectangle helper.
- `src/platform/single_instance.{h,cpp}` — named mutex guard.
- `src/platform/windows_shell_integration.{h,cpp}` — native event routing and shell lifecycle.
- `src/ui/qt_application.{h,cpp}` — Qt/QML lifecycle and root window access.
- `src/widgets/widget_descriptor.h` — descriptor and construction context.
- `src/widgets/widget_view_model.h` — QObject base metadata contract.
- `src/widgets/clock/clock_view_model.{h,cpp}` — per-instance minute-aligned clock.
- `src/widgets/cpu/cpu_view_model.{h,cpp}` — per-instance proxy over shared CpuService.
- `src/ui/qml/Main.qml`, `WidgetHost.qml`, `MetricCard.qml` — typed root, dynamic host and reusable card.
- `tests/*_test.cpp` — deterministic Qt Test coverage.

Removed obsolete units:

- `src/core/window_manager.{h,cpp}`
- `src/core/i_window_adapter.h`
- `src/core/config_manager.{h,cpp}`
- `src/ui/qt_window_adapter.{h,cpp}`
- `src/platform/appbar_proxy.{h,cpp}`
- `src/platform/cpu_sampler.h`
- `third_party/nlohmann/json.hpp`
- hand-written `resources.qrc`, `src/ui/qml/qmldir`, and tracked `dist.zip`

## Execution preflight: preserve the current workspace

Before Task 1, review `git status` and create one checkpoint commit on `codex/full-refactor` containing the current first-party source/configuration changes but no ignored build, cache or deployment directories. Use commit message `chore: checkpoint pre-refactor workspace`. This is not a squash or cleanup; it preserves the user's existing work as an independently recoverable baseline before overlapping files are replaced. After the checkpoint, verify `main` still points to `cf9f6bd` and all implementation commits remain on `codex/full-refactor`.

## Task 1: Establish testable build boundaries

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/CMakeLists.txt`
- Create: `tests/smoke_test.cpp`

- [ ] **Step 1: Add a failing Qt Test smoke target**

Create `tests/smoke_test.cpp`:

```cpp
#include <QtTest>

class SmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void testHarnessRuns() { QCOMPARE(2 + 2, 4); }
};

QTEST_GUILESS_MAIN(SmokeTest)
#include "smoke_test.moc"
```

Create `tests/CMakeLists.txt` with a reference to a not-yet-created library so configuration fails for the intended reason:

```cmake
if(NOT TARGET statusbar_core)
    message(FATAL_ERROR "statusbar_core target is required")
endif()
qt_add_executable(statusbar_smoke_test smoke_test.cpp)
target_link_libraries(statusbar_smoke_test PRIVATE statusbar_core Qt6::Test)
add_test(NAME statusbar_smoke_test COMMAND statusbar_smoke_test)
```

Temporarily add `include(CTest)`, `find_package(Qt6 REQUIRED COMPONENTS Test)` and `add_subdirectory(tests)` to the existing root file so the failing guard is evaluated.

- [ ] **Step 2: Verify RED**

Run: `cmake -S . -B build-refactor -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`

Expected: configuration fails because `statusbar_core` does not exist.

- [ ] **Step 3: Replace the root CMake structure**

Use target-scoped configuration:

```cmake
cmake_minimum_required(VERSION 3.21)
project(StatusBar_for_Coder VERSION 0.2.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)
include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()

add_library(statusbar_core STATIC)
target_link_libraries(statusbar_core PUBLIC Qt6::Core)
target_include_directories(statusbar_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

qt_add_executable(StatusBar_for_Coder WIN32)
target_link_libraries(StatusBar_for_Coder PRIVATE
    statusbar_core Qt6::Gui Qt6::Qml Qt6::Quick Shell32 User32 Pdh Advapi32)

if(MSVC)
    target_compile_options(statusbar_core PRIVATE /W4 /permissive-)
    target_compile_options(StatusBar_for_Coder PRIVATE /W4 /permissive-)
else()
    target_compile_options(statusbar_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    target_compile_options(StatusBar_for_Coder PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

Temporarily add the existing compilable core source list with `target_sources`; later tasks replace it file by file. Do not use `GLOB_RECURSE`, global AUTOMOC/AUTORCC/AUTOUIC, or root include paths.

- [ ] **Step 4: Verify GREEN**

Run: `cmake -S . -B build-refactor -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON && cmake --build build-refactor --parallel && ctest --test-dir build-refactor --output-on-failure`

Expected: one smoke test passes.

- [ ] **Step 5: Commit**

```text
build: establish modular Qt test harness
```

## Task 2: Implement transactional layout logic

**Files:**
- Create: `src/core/layout_engine.h`
- Create: `src/core/layout_engine.cpp`
- Create: `tests/layout_engine_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing layout tests**

Define the public contract in the test:

```cpp
#include "core/layout_engine.h"
#include <QtTest>

using Core::LayoutItem;
using Core::LayoutEngine;

class LayoutEngineTest final : public QObject {
    Q_OBJECT
private slots:
    void swapsDifferentSpansWithoutOverlap() {
        const std::vector<LayoutItem> items{{"clock", 0, 3}, {"cpu", 5, 2}};
        const auto result = LayoutEngine::drop(items, "clock", 5, 9);
        QVERIFY(result.has_value());
        QCOMPARE(result->at(0).slot, 5);
        QCOMPARE(result->at(1).slot, 0);
        QVERIFY(LayoutEngine::isValid(*result, 9));
    }

    void rejectsMultiCollisionTransaction() {
        const std::vector<LayoutItem> items{{"wide", 0, 3}, {"a", 4, 2}, {"b", 6, 2}};
        const auto result = LayoutEngine::drop(items, "wide", 4, 8);
        QVERIFY(!result.has_value());
    }

    void normalizesAfterScreenShrink() {
        const std::vector<LayoutItem> items{{"a", 8, 2}, {"b", 1, 3}};
        const auto result = LayoutEngine::normalize(items, 7);
        QVERIFY(LayoutEngine::isValid(result, 7));
        QCOMPARE(result.size(), items.size());
    }
};

QTEST_GUILESS_MAIN(LayoutEngineTest)
#include "layout_engine_test.moc"
```

- [ ] **Step 2: Verify RED**

Run: `cmake --build build-refactor --target layout_engine_test`

Expected: compilation fails because `core/layout_engine.h` is missing.

- [ ] **Step 3: Implement the minimal pure API**

Create this complete interface:

```cpp
#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Core {
struct LayoutItem final {
    std::string id;
    int slot{};
    int span{1};
    friend bool operator==(const LayoutItem&, const LayoutItem&) = default;
};

class LayoutEngine final {
public:
    [[nodiscard]] static bool isValid(const std::vector<LayoutItem>& items, int totalSlots) noexcept;
    [[nodiscard]] static std::vector<LayoutItem> normalize(std::vector<LayoutItem> items, int totalSlots);
    [[nodiscard]] static std::optional<std::vector<LayoutItem>> drop(
        const std::vector<LayoutItem>& items,
        std::string_view draggedId,
        int targetSlot,
        int totalSlots);
};
}
```

Implementation rules: validate positive spans and unique IDs; clamp the dragged item; permit a direct swap only when the displaced item fits the old interval; reject transactions that still collide; normalize in stable input order by selecting the nearest free slot, preferring the original slot and then lower slots.

- [ ] **Step 4: Verify GREEN**

Run: `cmake --build build-refactor --target layout_engine_test && ctest --test-dir build-refactor -R layout_engine --output-on-failure`

Expected: all layout tests pass.

- [ ] **Step 5: Commit**

```text
core: add transactional widget layout engine
```

## Task 3: Replace configuration with versioned atomic repository

**Files:**
- Create: `src/core/widget_config.h`
- Create: `src/core/config_repository.h`
- Create: `src/core/config_repository.cpp`
- Create: `tests/config_repository_test.cpp`
- Delete: `src/core/config_manager.h`
- Delete: `src/core/config_manager.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing migration and safety tests**

Cover these concrete cases in `config_repository_test.cpp` using `QTemporaryDir`:

```cpp
void migratesLegacyAndCreatesBackup();
void leavesMalformedSourceUntouched();
void atomicallyRoundTripsVersionOne();
void preservesUnknownWidgetEntries();
void rejectsDuplicateIdsAndInvalidSlots();
```

The migration test writes legacy `{ "widgets": [{"name":"Clock","slot":2}] }`, loads it, verifies a non-empty UUID, `type == "Clock"`, unchanged slot, version 1 output, and byte-identical `config.legacy.backup.json`.

- [ ] **Step 2: Verify RED**

Run: `cmake --build build-refactor --target config_repository_test`

Expected: compilation fails because `ConfigRepository` is missing.

- [ ] **Step 3: Implement data and repository contracts**

Use this interface:

```cpp
struct WidgetConfig final {
    QString id;
    QString type;
    int slot{};
    QJsonObject settings;
    friend bool operator==(const WidgetConfig&, const WidgetConfig&) = default;
};

struct ConfigDocument final {
    int version{1};
    QList<WidgetConfig> widgets;
};

class ConfigRepository final {
public:
    explicit ConfigRepository(QString configPath, QStringList legacyCandidates = {});
    [[nodiscard]] Result<ConfigDocument> load() const;
    [[nodiscard]] Result<void> save(const ConfigDocument& document) const;
    [[nodiscard]] const QString& path() const noexcept;
private:
    QString configPath_;
    QStringList legacyCandidates_;
};
```

Implement the referenced project-local `Core::Result<T>` in `src/core/result.h` using `std::variant<T, QString>`. Provide `hasValue()`, `value()`, and `error()`; specialize `Result<void>` with a success flag and QString error.

Use QJson only. Parse into a temporary document, validate the complete document, then return it. Save exclusively through `QSaveFile::commit()`. Migrate only when the destination is absent. Copy the exact legacy bytes to the backup before writing migrated JSON. Do not modify or delete the legacy source.

- [ ] **Step 4: Verify GREEN**

Run: `cmake --build build-refactor --target config_repository_test && ctest --test-dir build-refactor -R config_repository --output-on-failure`

Expected: all configuration tests pass.

- [ ] **Step 5: Commit**

```text
core: add atomic versioned configuration repository
```

## Task 4: Introduce shared CPU service and per-instance view models

**Files:**
- Create: `src/platform/cpu_usage.h`
- Create: `src/platform/cpu_data_source.h`
- Create: `src/platform/cpu_service.h`
- Create: `src/platform/cpu_service.cpp`
- Replace: `src/widgets/cpu/cpu_view_model.h`
- Create: `src/widgets/cpu/cpu_view_model.cpp`
- Replace: `src/widgets/clock/clock_view_model.h`
- Replace: `src/widgets/clock/clock_view_model.cpp`
- Create: `tests/cpu_usage_test.cpp`
- Create: `tests/widget_instances_test.cpp`
- Delete: `src/widgets/cpu/cpu_adapter.h`
- Delete: `src/widgets/cpu/cpu_adapter.cpp`
- Delete: `src/widgets/clock/clock_adapter.h`
- Delete: `src/platform/cpu_sampler.h`

- [ ] **Step 1: Write failing CPU calculation tests**

Test `calculateCpuUsage(previous, current)` for 50%, idle, zero total delta, and counter regression. The public value type is:

```cpp
struct CpuTimes final { std::uint64_t idle; std::uint64_t kernel; std::uint64_t user; };
[[nodiscard]] std::optional<int> calculateCpuUsage(CpuTimes previous, CpuTimes current) noexcept;
```

Zero delta and regressed counters return `std::nullopt`; valid values are clamped to 0–100.

- [ ] **Step 2: Verify RED**

Run: `cmake --build build-refactor --target cpu_usage_test`

Expected: missing `platform/cpu_usage.h`.

- [ ] **Step 3: Implement CpuService**

Define `CpuDataSource` as a non-copyable interface with `sampleTimes()`, `samplePerformanceRatio()` and `topology()` results. The production `WindowsCpuDataSource` owns the PDH query; tests provide a counting fake. `CpuService` is a final QObject with `cpuPercent`, `currentFrequencyMHz`, `maxFrequencyMHz`, physical/logical core constants, and cached `QVariantList history`. Its constructor receives `std::unique_ptr<CpuDataSource>`. It owns one coarse two-second QTimer and one fixed 30-entry ring buffer, and exposes `start()`, `stop()`, and `sampleNow()`. It emits each property signal only when that property changes; history changes after every valid CPU sample. PDH values are accepted only when both the API call and `CStatus` succeed.

- [ ] **Step 4: Write and verify failing multi-instance tests**

Use one counting `CpuDataSource` fake inside one shared CpuService to construct two `CpuViewModel` objects. Verify the objects have different QObject addresses and instance IDs, but one `CpuService::sampleNow()` produces matching values and increments the fake exactly once. Verify two ClockViewModels can use different `format` or `timeZone` settings.

- [ ] **Step 5: Implement per-instance view models**

ClockViewModel owns a single-shot coarse timer, updates immediately, schedules the next minute boundary, and reads `format` plus `timeZone` from settings. CpuViewModel stores instance metadata and forwards the shared CpuService properties/signals without owning a sampling timer.

- [ ] **Step 6: Verify GREEN**

Run: `cmake --build build-refactor --target cpu_usage_test widget_instances_test && ctest --test-dir build-refactor -R "cpu_usage|widget_instances" --output-on-failure`

Expected: both test executables pass.

- [ ] **Step 7: Commit**

```text
widgets: add shared CPU service and independent view models
```

## Task 5: Make the registry the single widget extension point

**Files:**
- Create: `src/widgets/widget_descriptor.h`
- Create: `src/widgets/widget_view_model.h`
- Replace: `src/widgets/widget_registry.h`
- Replace: `src/widgets/registry_setup.cpp`
- Replace: `src/ui/widget_model.h`
- Replace: `src/ui/widget_model.cpp`
- Create: `tests/widget_registry_test.cpp`
- Create: `tests/widget_model_test.cpp`
- Delete: `src/widgets/i_widget_view_model.h`

- [ ] **Step 1: Write failing descriptor/model tests**

Verify Clock and Cpu descriptors each contain non-empty `type`, positive span, `qrc:/qt/qml/StatusBar/...` URL, and a factory. Load two Clock configs and assert two separate viewModel QObjects. Include one unknown config, change a known slot, save, and verify the unknown entry remains unchanged.

- [ ] **Step 2: Verify RED**

Run: `cmake --build build-refactor --target widget_registry_test widget_model_test`

Expected: missing descriptor/model roles.

- [ ] **Step 3: Implement registry contract**

Use a descriptor with this exact shape:

```cpp
struct WidgetContext final { Platform::CpuService& cpuService; };
using WidgetFactory = std::function<std::unique_ptr<WidgetViewModel>(
    const Core::WidgetConfig&, WidgetContext&)>;

struct WidgetDescriptor final {
    QString type;
    int span{};
    QUrl qmlUrl;
    WidgetFactory create;
};
```

Registry construction receives a descriptor list; duplicate or empty type registration returns an error. Avoid a mutable process-global singleton. `registerAllWidgets()` returns a ready value registry.

- [ ] **Step 4: Implement model roles and transactional layout**

Roles are `InstanceIdRole`, `TypeRole`, `SlotRole`, `SpanRole`, `QmlUrlRole`, and `ViewModelRole`. The model owns known instances and separately retains unavailable configs. `setTotalSlots(int)` calls `LayoutEngine::normalize`; `dropWidget(...)` calls `LayoutEngine::drop`. Save only after a successful changed transaction. Emit the smallest valid `dataChanged` range.

- [ ] **Step 5: Verify GREEN**

Run: `cmake --build build-refactor --target widget_registry_test widget_model_test && ctest --test-dir build-refactor -R "widget_registry|widget_model" --output-on-failure`

Expected: registry and model tests pass.

- [ ] **Step 6: Commit**

```text
widgets: make descriptors the single extension point
```

## Task 6: Convert UI to a typed cached QML module

**Files:**
- Create: `src/ui/qml/Main.qml`
- Create: `src/ui/qml/WidgetHost.qml`
- Create: `src/ui/qml/MetricCard.qml`
- Replace: `src/ui/qml/Theme.qml`
- Replace: `src/widgets/clock/ClockWidget.qml`
- Replace: `src/widgets/cpu/CpuWidget.qml`
- Replace: `src/widgets/cpu/CpuDetailPopup.qml`
- Modify: `CMakeLists.txt`
- Delete: `src/ui/qml/main.qml`
- Delete: `src/ui/qml/qmldir`
- Delete: `resources.qrc`

- [ ] **Step 1: Add QML module build rules and verify RED**

Declare:

```cmake
set_source_files_properties(src/ui/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(StatusBar_for_Coder
    URI StatusBar
    VERSION 1.0
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/Theme.qml
        src/ui/qml/WidgetHost.qml
        src/ui/qml/MetricCard.qml
        src/widgets/clock/ClockWidget.qml
        src/widgets/cpu/CpuWidget.qml
        src/widgets/cpu/CpuDetailPopup.qml
    RESOURCES src/widgets/cpu/cpu.svg)
```

Run: `cmake -S . -B build-refactor -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`

Expected: configuration fails until the new QML files exist.

- [ ] **Step 2: Implement the typed root and host**

Register `WidgetModel` and the abstract `WidgetViewModel` as named uncreatable QML types in the StatusBar module. `Main.qml` declares `required property WidgetModel widgetModel`, starts invisible, derives layout dimensions from its actual height, declares required delegate role properties, and calls `widgetModel.setTotalSlots(totalSlots)` when the grid changes. It contains no Clock/Cpu string branch and no context property access.

`WidgetHost.qml` owns the Loader. On source or ViewModel changes it calls `loader.setSource(qmlUrl, { "viewModel": viewModel, "editingWindow": host.Window.window })`. ClockWidget declares `required property ClockViewModel viewModel`; CpuWidget and CpuDetailPopup declare `required property CpuViewModel viewModel`. All three derived types are registered as named uncreatable QML types. Dragging disables the x Behavior while active and commits only on release.

- [ ] **Step 3: Implement lazy CPU detail and reusable cards**

CpuWidget uses a Loader with `active` set only when detail is first requested. CpuDetailPopup uses `MetricCard` four times, enables history Connections only while visible, draws the static grid once in a separate Canvas, and repaints only the wave Canvas on history changes. Remove SVG mipmap/double source size and all `Text.NativeRendering` overrides.

- [ ] **Step 4: Centralize theme values**

Theme defines single-family font names plus all repeated foreground/background/accent/error/border colors, radii and animation durations. Metric and widget QML contain no repeated literal palette values except transparent.

- [ ] **Step 5: Verify GREEN and lint**

Run: `cmake --build build-refactor --parallel && cmake --build build-refactor --target all_qmltyperegistrations && qmllint -I build-refactor src/ui/qml/Main.qml src/ui/qml/Theme.qml src/ui/qml/WidgetHost.qml src/ui/qml/MetricCard.qml src/widgets/clock/ClockWidget.qml src/widgets/cpu/CpuWidget.qml src/widgets/cpu/CpuDetailPopup.qml`

Expected: build and qmllint exit 0 without unqualified-access, missing-property, or missing-import warnings.

- [ ] **Step 6: Commit**

```text
ui: move widgets to typed cached QML module
```

## Task 7: Replace Qt adapter with explicit application lifecycle

**Files:**
- Create: `src/ui/qt_application.h`
- Create: `src/ui/qt_application.cpp`
- Replace: `src/main.cpp`
- Delete: `src/ui/qt_window_adapter.h`
- Delete: `src/ui/qt_window_adapter.cpp`
- Delete: `src/core/window_manager.h`
- Delete: `src/core/window_manager.cpp`
- Delete: `src/core/i_window_adapter.h`
- Create: `tests/qt_application_test.cpp`

- [ ] **Step 1: Write a failing offscreen QML creation test**

The test sets `QT_QPA_PLATFORM=offscreen`, constructs QtApplication with a temporary ConfigRepository and registry, calls `initialize()`, verifies a non-null root QQuickWindow, no context properties named `widgetModel`, `clockAdapter`, or `cpuAdapter`, and checks the root starts hidden.

- [ ] **Step 2: Verify RED**

Run: `cmake --build build-refactor --target qt_application_test`

Expected: missing `ui/qt_application.h`.

- [ ] **Step 3: Implement QtApplication**

Provide direct-value RAII members in declaration order so the engine dies before QGuiApplication. `initialize()` loads from module `StatusBar`, type `Main`, passes WidgetModel through `setInitialProperties`, checks `rootObjects()` and type-casts the root to QQuickWindow. Expose `window()`, `show()`, `run()`, and `quit()`. Return Result errors rather than logging and continuing.

- [ ] **Step 4: Simplify main composition**

Main creates QGuiApplication lifecycle, LocalAppData path and legacy candidates, ConfigRepository, CpuService, registry, WidgetModel, and platform shell. It exits non-zero on configuration/QML/AppBar failure, permits logged tray degradation, starts CpuService only after the model is ready, shows after AppBar positioning, and returns the actual event-loop code.

- [ ] **Step 5: Verify GREEN**

Run: `cmake --build build-refactor --target qt_application_test && ctest --test-dir build-refactor -R qt_application --output-on-failure`

Expected: offscreen application test passes.

- [ ] **Step 6: Commit**

```text
app: make Qt lifecycle an explicit composition root
```

## Task 8: Rebuild Windows shell integration as RAII

**Files:**
- Create: `src/platform/appbar.h`
- Create: `src/platform/appbar.cpp`
- Create: `src/platform/single_instance.h`
- Create: `src/platform/single_instance.cpp`
- Create: `src/platform/windows_shell_integration.h`
- Create: `src/platform/windows_shell_integration.cpp`
- Replace: `src/platform/tray_icon.h`
- Replace: `src/platform/tray_icon.cpp`
- Create: `tests/appbar_geometry_test.cpp`
- Delete: `src/platform/appbar_proxy.h`
- Delete: `src/platform/appbar_proxy.cpp`
- Delete: `src/platform/display.h`
- Delete: `src/platform/display.cpp`

- [ ] **Step 1: Write failing AppBar geometry tests**

Extract and test:

```cpp
[[nodiscard]] RECT topAppBarRect(RECT monitorRect, RECT shellAdjustedRect, LONG desiredHeight) noexcept;
```

Verify the result preserves `shellAdjustedRect.top`, keeps monitor left/right, and sets bottom to `top + desiredHeight`; include a non-zero/negative monitor origin.

- [ ] **Step 2: Verify RED**

Run: `cmake --build build-refactor --target appbar_geometry_test`

Expected: missing `platform/appbar.h`.

- [ ] **Step 3: Implement AppBar RAII**

AppBar accepts the actual root HWND. `initialize()` performs `ABM_NEW`, query, set and `SetWindowPos` using the final RECT. `reposition()` obtains the current monitor via `MonitorFromWindow`, DPI via `GetDpiForWindow`, and logical desired height converted with `MulDiv`. `shutdown()` is idempotent. Copy and move are deleted because the registered HWND identity must remain stable.

- [ ] **Step 4: Implement tray recovery and single instance**

TrayIcon checks every Win32/Shell result, stores the registered `TaskbarCreated` message, re-adds itself after Explorer restart, calls `NIM_SETVERSION`, clears GWLP_USERDATA on destruction, and posts WM_NULL after closing the popup menu. SingleInstance owns a named mutex and reports `alreadyRunning()` separately from creation failure.

- [ ] **Step 5: Implement native event integration**

WindowsShellIntegration derives from QAbstractNativeEventFilter, forwards AppBar callback/display/DPI/TaskbarCreated messages, owns AppBar and TrayIcon, and removes itself from QCoreApplication before members are destroyed. Catch all exceptions before returning across native callbacks.

- [ ] **Step 6: Verify GREEN**

Run: `cmake --build build-refactor --target appbar_geometry_test && ctest --test-dir build-refactor -R appbar_geometry --output-on-failure`

Expected: geometry tests pass and the full application links.

- [ ] **Step 7: Commit**

```text
platform: rebuild Windows shell integration with RAII
```

## Task 9: Deployment, dependency cleanup and release optimization

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `.gitignore`
- Modify: `README.md`
- Delete: `third_party/nlohmann/json.hpp`
- Delete: `dist.zip`

- [ ] **Step 1: Add a failing deployment verification**

Add CMake install/deploy rules that reference a not-yet-configured deployment script, then run `cmake --install build-refactor --prefix package-refactor` and confirm it lacks the executable/runtime before completing the rules.

- [ ] **Step 2: Implement reproducible deployment**

Use `install(TARGETS StatusBar_for_Coder RUNTIME DESTINATION .)` followed by `qt_generate_deploy_qml_app_script(TARGET StatusBar_for_Coder OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)` and `install(SCRIPT ${deploy_script})`. Include CPack ZIP generation. Enable `INTERPROCEDURAL_OPTIMIZATION_RELEASE` only after `check_ipo_supported()` succeeds.

- [ ] **Step 3: Remove stale and unused dependencies**

Delete nlohmann/json, hand deployment archives and obsolete source files. Remove unused Gdi32/Powrprof links. Ignore `build-*`, `package-*`, CPack outputs and generated archives while keeping source configuration examples under a distinct `examples/` name if needed.

- [ ] **Step 4: Document build, test, migration and deployment commands**

README must contain exact Debug/Release configure commands, CTest command, package command, LocalAppData configuration location, legacy backup name, supported Windows/Qt versions and the four-file recipe for adding a Widget module.

- [ ] **Step 5: Verify package contents**

Run: `cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF && cmake --build build-release --parallel && cmake --install build-release --prefix package-refactor`

Expected: GUI executable and required Qt libraries are present; no `QtTest`, SQL drivers, qmltooling debugger, source QML test modules, duplicated `plugins/plugins` tree, or nlohmann header is present.

- [ ] **Step 6: Commit**

```text
build: add minimal reproducible Windows deployment
```

## Task 10: Full verification and manual shell smoke test

**Files:**
- Modify only files required to fix verification failures

- [ ] **Step 1: Run all automated checks**

Run:

```text
cmake -S . -B build-refactor -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-refactor --parallel
ctest --test-dir build-refactor --output-on-failure
qmllint -I build-refactor src/ui/qml/Main.qml src/ui/qml/Theme.qml src/ui/qml/WidgetHost.qml src/ui/qml/MetricCard.qml src/widgets/clock/ClockWidget.qml src/widgets/cpu/CpuWidget.qml src/widgets/cpu/CpuDetailPopup.qml
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-release --parallel
```

Expected: zero build, test or lint failures.

- [ ] **Step 2: Verify binary properties**

Use `objdump -x build-release/StatusBar_for_Coder.exe` and confirm subsystem `Windows GUI`. Search build rules for `qmlcache`/`qmlsc` and confirm generated cache sources exist. Confirm there is no one-second global update timer and no process-global mutable Widget/Config/Adapter singleton.

- [ ] **Step 3: Perform controlled manual smoke test**

Launch the packaged application once. Verify one top bar, no console, correct work-area reservation, tray quit, two Clock and two CPU instances, one CPU sampling cadence, drag swap, resize/DPI reposition and clean exit. Explorer restart changes the user's desktop session, so request explicit approval immediately before that one smoke-test action; if approved, restart Explorer and verify AppBar/tray recovery. End the application before continuing.

- [ ] **Step 4: Review scope and diffs**

Confirm every acceptance criterion in `docs/superpowers/specs/2026-09-01-statusbar-full-refactor-design.md` maps to passing evidence. Confirm unrelated user files were preserved and only intended tracked artifacts were removed.

- [ ] **Step 5: Commit final corrections**

```text
test: verify full status bar refactor
```
