# CEH Windows UIA

> **木头自动系统控制 / CEH_Windows_Uia**  
> 基于 **C++17 + Qt6 + Windows UI Automation + 本地 LLM/VLM + 视觉定位 + 鼠标键盘控制** 的 Windows 桌面智能控制助手。

CEH Windows UIA 的目标不是为每个软件单独写自动化脚本，而是让用户直接用自然语言告诉程序：

- 打开什么软件
- 在当前软件里做什么
- 分析当前窗口
- 操作某个按钮
- 输入文字
- 和当前联系人聊天
- 分析用户上传的图片或截图

系统根据当前窗口、UIA 可用性、视觉识别结果和本地模型推理结果，动态选择 UIA、Vision、Mouse、Keyboard 等方式执行任务。

---

## 项目目标

一句话：

> **让普通用户通过自然语言控制 Windows 电脑。**

典型使用方式：

```text
打开 qq
```

```text
打开微信
```

```text
和他聊天
```

```text
和他聊天，问出他做什么职业
```

```text
看看这个窗口里有什么
```

```text
分析这张图片
```

程序负责完成：

```text
任务理解
→ 窗口/应用识别
→ 能力判断
→ 视觉/UIA观察
→ 动作执行
→ 效果验证
→ 等待 / 继续 / 完成
```

---

# 核心特性

## 1. 自然语言控制电脑

用户不需要了解：

```text
UIA
XPath
AutomationId
坐标
脚本
宏
RPA
```

只需要正常说：

```text
打开 qq
```

或者：

```text
和他聊天
```

---

## 2. Windows 应用启动

程序支持确定性应用启动流程：

```text
应用解析
→ 已运行窗口检查
→ 启动进程
→ 等待顶层窗口
→ 获取 PID
→ 获取 HWND
→ 获取窗口标题
→ 确认启动完成
```

示例日志：

```text
[APP_LIFECYCLE] 确定性打开流程开始：qq
[APP_LIFECYCLE] 打开确认完成：
pid=12748
hwnd=9835080
title=QQ
```

程序不会仅因为 `CreateProcess` 成功就认为软件已经真正打开。

---

## 3. 指定窗口控制

用户可以手动选择一个窗口。

系统会锁定：

```text
Process
PID
HWND
Window Rect
Window Title
```

之后任务限制在：

```text
WINDOW_CONTROL
```

授权范围中执行。

适用于：

- 微信
- QQ
- 浏览器
- Office
- 普通 Windows 软件
- 自绘 UI
- UIA 信息不完整的软件

---

## 4. 桌面控制

桌面模式允许执行：

```text
打开 qq
打开某个软件
切换窗口
查找目标应用
```

桌面任务使用：

```text
DESKTOP_CONTROL
```

作为授权边界。

---

## 5. Windows UI Automation

程序集成 Windows UI Automation，可读取：

- 控件树
- 控件名称
- 控件类型
- Automation ID
- 可操作节点
- 顶层窗口

程序不会盲目依赖 UIA。

如果扫描结果类似：

```text
nodes=2
actionable=0
```

系统会锁定：

```text
UIA unavailable
```

然后切换到视觉控制。

---

## 6. VLM 视觉理解

UIA 不可用时，可以通过本地视觉模型分析当前窗口。

示例：

```json
{
  "screen_state": "当前处于聊天界面",
  "targets": [
    {
      "label": "消息输入框",
      "confidence": 0.95,
      "norm_x": 0.66,
      "norm_y": 0.93
    },
    {
      "label": "发送按钮",
      "confidence": 0.95,
      "norm_x": 0.89,
      "norm_y": 0.93
    }
  ]
}
```

Vision 负责回答：

```text
当前是什么界面
任务相关内容是什么
哪里可以操作
目标位于哪里
```

---

## 7. 鼠标键盘执行

当前支持：

```text
mouse_move
mouse_click
mouse_drag
mouse_path
keyboard_type
keyboard_hotkey
```

视觉目标通过：

```text
norm_x
norm_y
norm_rect
```

转换为实际屏幕坐标。

---

## 8. 自然聊天

聊天任务当前采用：

```text
识别当前会话
→ 找到输入框
→ 生成自然口语
→ 输入文字
→ 定位发送按钮
→ 提交
→ 验证
→ WAIT_FOR_CHANGE
→ 等待下一次界面变化
```

例如：

```text
用户：
和他聊天，问出他做什么职业
```

程序可以生成：

```text
最近在忙什么呀？
```

后续继续：

```text
你现在是做什么工作的呀？
```

---

## 9. 当前会话保护

当用户已经手动选中当前聊天窗口，而且没有明确说要切换联系人时：

```text
和他聊天
继续聊
问他做什么工作
```

默认含义是：

> **操作当前已经打开的会话。**

程序不会擅自：

- 点击第一个联系人
- 点击第一个群聊
- 切换到服务号
- 随机选择聊天对象

只有用户明确说：

```text
打开张三
```

或者：

```text
和张三聊天
```

才允许执行联系人切换。

---

## 10. Pending Commit 事务

`keyboard_type` 成功只代表：

```text
文字输入成功
```

不代表：

```text
消息已经发送
```

所以程序建立：

```text
pending_commit
```

流程：

```text
keyboard_type
↓
pending_commit
↓
定位发送/提交按钮
↓
点击发送
↓
重新观察
↓
验证提交结果
```

用于减少：

- 只输入不发送
- 点击偏移
- 重复发送
- 未提交就进入等待

---

## 11. 发送按钮命中与重试

发送/提交按钮优先使用：

```text
norm_rect
```

几何中心。

如果 Vision 没有返回矩形，则退回：

```text
norm_x
norm_y
```

如果点击后仍检测到刚输入的文字：

```text
发送失败
```

程序不会直接进入等待，而是：

```text
重新定位发送按钮
→ 再次点击
→ 再验证
```

并记录：

```text
[COMMIT_CLICK]
```

日志。

---

## 12. WAIT_FOR_CHANGE

连续任务中，系统支持低频等待外部状态变化。

例如：

```text
发送消息
→ 等待回复
```

等待期间：

```text
没有变化
→ C++继续等待
```

而不是不断调用 Planner。

检测到变化后：

```text
重新观察
→ 重新规划
```

适用于：

- 聊天回复
- 网页加载
- 软件启动
- 弹窗
- 异步任务

---

## 13. 用户图片 / 截图分析

用户直接上传图片或截图时，会进入独立 Vision 流程。

这一模式不会返回 Agent 内部结构：

```text
targets
norm_x
norm_y
UIA
Planner
```

而是直接输出自然语言分析结果。

程序会检查：

- 空结果
- 极短结果
- 无效结果

如果结果无效，会自动重试一次。

---

# 架构

```text
USER GOAL
   ↓
Task Understanding
   ↓
Runtime App Profile
   ↓
Capability Resolver
   ├─ UIA
   ├─ Vision
   ├─ Mouse
   ├─ Keyboard
   └─ Application Lifecycle
   ↓
World State
   ↓
Planner
   ↓
Semantic Action
   ↓
Grounding
   ↓
Executor
   ↓
Verifier
   ↓
Wait / Continue / Complete
```

设计原则：

```text
AI负责理解
C++负责执行
```

---

# 本地模型

当前项目采用本地推理。

测试模型：

```text
gemma-4-E4B-it-Q4_0.gguf
```

视觉投影：

```text
mmproj-gemma-4-E4B-it-Q8_0.gguf
```

推理服务：

```text
llama.cpp
```

---

## LLM

典型配置：

```text
host        127.0.0.1
port        8888
ctx         16384
threads     12
gpu layers  42
batch       256
ubatch      64
```

---

## VLM

典型配置：

```text
host        127.0.0.1
port        8889
ctx         8192
threads     12
gpu layers  42
batch       256
ubatch      64
```

VLM 按需加载。

---

# 当前测试环境

| 项目 | 环境 |
|---|---|
| OS | Windows 10 64-bit |
| CPU | Intel Core i9-9900KF |
| RAM | 32 GB |
| GPU | NVIDIA RTX 3080 10 GB |
| C++ | C++17 |
| Compiler | MSVC |
| Qt | Qt 6.11.x |
| OpenCV | OpenCV 4.x |
| IDE | CLion |
| Local AI | llama.cpp |
| Model | Gemma 4 E4B GGUF |

项目不要求必须使用 RTX 3080。

实际性能取决于：

- 模型大小
- GGUF 量化
- Context 长度
- GPU 显存
- CPU 性能
- GPU Offload

---

# 推荐环境

## 最低建议

```text
Windows 10 / 11 x64
16 GB RAM
现代 6 核以上 CPU
Qt6 x64
```

CPU 推理可以运行，但速度会明显更慢。

## 推荐

```text
Windows 10 / 11 x64
32 GB RAM
8 核 / 16 线程以上 CPU
NVIDIA RTX 3060 或更高
```

---

# 依赖

主要依赖：

```text
C++17
Qt6 Core
Qt6 Gui
Qt6 Widgets
Qt6 Network
Qt6 Sql
Qt6 Concurrent
Windows UI Automation
OpenCV
llama.cpp
```

---

# Qt

当前测试：

```text
Qt 6.11.1
MSVC 2022 x64
```

示例路径：

```text
C:\Qt\6.11.1\msvc2022_64
```

---

# OpenCV

示例：

```text
C:\opencv\opencv\build
```

需要正确配置：

```text
include
lib
bin
```

---

# 项目目录

```text
windows_uia/
│
├─ main.cpp
├─ CMakeLists.txt
├─ resources.qrc
│
├─ assets/
│  ├─ config/
│  │  ├─ agent.json
│  │  ├─ app_capabilities.json
│  │  ├─ tools.json
│  │  └─ permissions.json
│  │
│  ├─ models/
│  │  └─ prompts/
│  │     ├─ planner.txt
│  │     ├─ vision.txt
│  │     ├─ verifier.txt
│  │     ├─ system.txt
│  │     └─ context_summary.txt
│  │
│  ├─ db/
│  │  └─ wood.db
│  │
│  └─ uppic/
│     └─ log/
│
├─ platform/
│  └─ Windows/
│
└─ cmake-build-debug/
```

---

# 配置文件

## `agent.json`

Agent 运行配置，包括：

- Workflow
- Generic Policy
- Vision
- WAIT_FOR_CHANGE
- Pending Commit
- Text Composer
- Current Conversation Protection
- User Image Analysis

## `app_capabilities.json`

应用能力提示。

它不是软件白名单。

未知应用仍可以通过动态 Runtime Profile 工作。

## `planner.txt`

Planner 负责：

```text
下一步做什么
```

## `vision.txt`

Vision 负责：

```text
当前看到了什么
哪里可以操作
哪些内容和任务相关
```

## `verifier.txt`

Verifier 负责：

```text
动作有没有真正产生效果
```

---

# 编译

## 1. 准备

安装：

```text
Visual Studio / MSVC
CMake
Ninja
Qt6
OpenCV
```

## 2. CMake

```bash
cmake -S . -B cmake-build-debug -G Ninja
```

## 3. Build

```bash
cmake --build cmake-build-debug -j 14
```

也可以直接在 CLion：

```text
Build → CEH_Windows_Uia
```

---

# 模型文件

不建议把大型模型直接提交 GitHub：

```text
*.gguf
mmproj-*.gguf
```

建议通过 README 说明模型放置位置。

---

# 建议 `.gitignore`

```gitignore
.idea/

cmake-build-*/
build/
out/

.vs/
*.user
*.suo

*.exe
*.dll
*.lib
*.obj
*.pdb

*_autogen/
moc_*
qrc_*
ui_*

*.log
assets/uppic/log/

*.db-shm
*.db-wal

*.gguf
*.bin

*.tmp
*.cache
Thumbs.db
.DS_Store
```

---

# 使用方式

## 普通聊天

切换到：

```text
普通聊天
```

直接和本地 LLM 对话。

## 用户图片

上传图片，然后问：

```text
这张图是什么？
```

## 指定窗口

选择：

```text
指定识别
```

再选择目标窗口。

例如：

```text
微信
```

然后：

```text
和他聊天
```

或者：

```text
和他聊天，问出他做什么职业
```

## 桌面模式

开启：

```text
桌面识别
```

然后：

```text
打开 qq
```

---

# 日志

日志默认保存：

```text
assets/uppic/log/
```

记录：

```text
启动
模型加载
资源监控
任务理解
Runtime Profile
UIA
Vision
Planner
Grounding
Mouse / Keyboard
Pending Commit
Verifier
WAIT_FOR_CHANGE
任务结束
```

---

# 资源监控

程序会记录：

```text
RAM
CPU
GPU显存
GPU利用率
GPU温度
```

---

# 安全边界

系统区分：

```text
DESKTOP_CONTROL
```

和：

```text
WINDOW_CONTROL
```

指定窗口模式只允许操作用户选定的 HWND 范围。

任务切换时：

```text
旧 generation 作废
```

旧推理结果和后续动作不会继续执行。

---

# 当前已实际跑通的主要链路

## 打开应用

```text
打开 QQ
→ PID
→ HWND
→ Window Title
→ Completed
```

## 指定窗口聊天

```text
锁定微信窗口
→ Vision找到输入框
→ 自然语言生成
→ keyboard_type
→ pending_commit
→ 定位发送按钮
→ mouse_click
→ 验证
```

## UIA 自动降级

```text
UIA信息不足
→ UIA unavailable
→ VisionMouse
```

## 用户图片分析

```text
用户图片
→ VLM
→ 自然语言结果
```

---

# 当前限制

项目仍处于持续开发阶段。

目前主要限制：

## VLM OCR

可能出现：

- 漏字
- 错字
- 联系人姓名识别错误
- 自己消息与对方消息混淆

## 自绘 UI

Chromium、DirectUI、Canvas、GPU UI 等软件的 UIA 信息可能非常少。

## 视觉坐标

小按钮、DPI 缩放、多显示器、窗口边框可能影响点击精度。

## 本地模型

较小模型可能出现：

- JSON字段遗漏
- Planner误判
- Vision文字不完整
- 长上下文信息丢失

因此项目在 C++ Runtime 中保留大量确定性校验和容错。

---

# 设计原则

## AI 理解，C++ 执行

```text
AI:
理解任务
理解画面
生成动作意图

C++:
权限检查
坐标检查
鼠标键盘
进程控制
效果验证
```

## 不为单个软件写死流程

项目尽量使用：

```text
输入框
按钮
列表
窗口
确认
发送
提交
```

等通用语义。

## 不相信单次模型结论

模型说：

```text
成功
```

不代表真的成功。

必须通过：

```text
窗口
UIA
Vision
输入框状态
进程
HWND
```

进一步验证。

---

# Roadmap

- [ ] 更稳定区分自己消息 / 对方消息
- [ ] 更稳定的聊天正文 OCR
- [ ] 更准确的按钮矩形 Grounding
- [ ] DPI 缩放增强
- [ ] 多显示器坐标增强
- [ ] UIA + Vision 融合
- [ ] 更快的模型切换
- [ ] 更稳定的长任务上下文
- [ ] 更多普通 Windows 软件适配
- [ ] Application Profile 缓存
- [ ] 更完善的失败恢复

---

# 项目信息

项目名：

```text
CEH_Windows_Uia
```

中文名：

```text
木头自动系统控制
```

方向：

```text
Windows Local AI Desktop Agent
```

核心技术：

```text
C++
Qt6
Windows UI Automation
OpenCV
Local LLM
Local VLM
llama.cpp
Visual Grounding
Mouse / Keyboard Automation
```

---

# License

仓库如果还没有 `LICENSE` 文件，公开发布前请自行选择许可证，例如：

```text
MIT
Apache-2.0
GPL-3.0
```

模型文件、第三方 SDK、第三方资源应分别遵守其原始许可证。

---

# Disclaimer

本项目包含桌面自动控制能力。

仅应在：

- 自己的设备
- 已授权的窗口
- 已授权的软件
- 合法用途

中使用。

自动化操作可能产生：

- 错误点击
- 错误发送
- 文件覆盖
- 数据变化

当前项目仍处于开发阶段，不建议直接用于无人值守的高风险生产环境。

---

# CEH Windows UIA

**Natural Language → Windows Control**

让用户不用写自动化脚本，只需要告诉电脑：

> **“我要做什么。”**
