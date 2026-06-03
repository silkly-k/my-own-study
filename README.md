# 🔌 嵌入式开发学习系统

> 一年项目驱动学习计划 | 2026.06 — 2027.06  
> 方法论：三遍复刻法 × AI 加速  
> 工具链：Claude Code + mattpocock/skills + STM32CubeIDE

---

## 🎯 学习路线

```
项目 1 → MultiButton   (~300行)  状态机 + 事件驱动           [1.5~2月]
项目 2 → letter-shell  (~2000行)  宏魔法 + 自动注册机制       [2~3月]
项目 3 → EasyLogger    (~1500行)  分层架构 + 前后端分离       [2~2.5月]
项目 4 → SFUD          (~2500行)  HAL 抽象层 + 跨平台设计     [2~2.5月]
项目 5 → FreeRTOS 实战           多任务系统设计 + 同步机制     [2~3月]
毕业项 → IoT / 工控 / Linux网关   综合整合                    [2~3月]
```

## 📂 仓库结构

```
embedded-learning/
├── 01-multibutton-rewrite/     ← 项目 1：事件驱动按键库
├── 02-mini-shell/              ← 项目 2：串口命令解析器
├── 03-mini-logger/             ← 项目 3：分层日志系统
├── 04-mini-hal/                ← 项目 4：传感器 HAL 抽象层
├── 05-freertos-multitask/      ← 项目 5：FreeRTOS 多任务系统
├── 06-graduation-project/      ← 毕业项目
├── .agents/skills/             ← 自定义 Agent Skills（9-12月）
├── notes/                      ← 学习笔记和月度复盘
└── 一年嵌入式学习计划.md        ← 完整学习计划文档
```

## 🛠️ 硬件

| 阶段 | 硬件 | 预算 |
|------|------|------|
| 项目 1 | STM32F103C8T6 + ST-Link V2 | ¥50-80 |
| 项目 2 | USB-TTL 模块 | ¥5-10 |
| 项目 3 | SPI Flash W25Q32 | ¥3-5 |
| 项目 4 | 逻辑分析仪 24MHz 8CH | ¥30-50 |
| 项目 5 | OLED 0.96" + DHT22/AHT10 | ¥15-25 |

## 🤖 AI 工具链

- **Claude Code** — AI 编程主引擎
- **mattpocock/skills** (29个) — 工程方法论（diagnose / grill-me / tdd / review / handoff）
- **Tavily WebSearch** — 搜索数据手册
- **自定义 Skills** (9-12月创建) — datasheet / periph-config / freertos-analyze / embedded-code-review

## ⚡ 三遍复刻法

```
第一遍：跑起来 → 理解「做了什么」
第二遍：画地图 → 理解「怎么做的」（状态机图/调用图/数据流图）
第三遍：重写核心 → 变成自己的
```

## 📋 月度复盘

参见 [notes/monthly-review.md](notes/monthly-review.md)

---

> **最重要的规则：每个月都要有代码跑在硬件上。**

*基于 178-agent 深度研究 + 社区交叉验证 + mattpocock 工程方法论*
