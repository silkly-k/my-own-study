/*
 * ===================================================================
 * MultiButton 头文件 — 学习注释版
 * 本文重点学习：结构体设计 + 位域 + 枚举 + 函数指针类型 + API 设计
 * ===================================================================
 */

#ifndef MULTI_BUTTON_H
#define MULTI_BUTTON_H

#include <stdint.h>
#include <string.h>

// ── 版本号宏 ──
#define MULTIBUTTON_VERSION_MAJOR 1
#define MULTIBUTTON_VERSION_MINOR 1
#define MULTIBUTTON_VERSION_PATCH 1

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 A：可配置常量                                      ║
// ║  这些值定义在 .h 里而不是 .c 里，因为用户可能需要改。        ║
// ║  用 #define 而不是 const，因为 C89/C99 下 const 不是       ║
// ║  编译期常量，不能用来定义数组大小。                         ║
// ╚══════════════════════════════════════════════════════════╝

#define TICKS_INTERVAL        5       // ms — 定时器中断间隔
#define DEBOUNCE_TICKS        3       // 消抖深度：最多 7 (受限于 3-bit 位域)
#define SHORT_TICKS           (300 / TICKS_INTERVAL)   // 短按阈值 = 60 ticks
#define LONG_TICKS            (1000 / TICKS_INTERVAL)  // 长按阈值 = 200 ticks
#define PRESS_REPEAT_MAX_NUM  15      // 连按最大计数值

// ── 编译期检查：如果消抖数超过位域上限，编译直接报错 ──
// 这是静态断言的一种原始但有效的形式。
// 比运行时检查好——还没跑就知道有问题。
#if DEBOUNCE_TICKS > 7
#error "DEBOUNCE_TICKS exceeds 3-bit field maximum (7)"
#endif

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 B：函数指针类型定义                              ║
// ║                                                          ║
// ║  typedef void (*BtnCallback)(Button*, void*);            ║
// ║           │    │      │        │        │                ║
// ║           │    │      │        │        └── 参数列表     ║
// ║           │    │      │        └── 返回类型               ║
// ║           │    │      └── 新类型名                         ║
// ║           │    └── 这是一个函数指针                         ║
// ║           └── void 无返回值                                ║
// ║                                                          ║
// ║  之后 BtnCallback 就是一个类型，和 int、char 一样用：     ║
// ║    BtnCallback cb = my_function;                          ║
// ║    BtnCallback cb_array[8];  ← 这就是回调数组的类型      ║
// ║                                                          ║
// ║  void* user_data 参数：让用户传任意上下文。              ║
// ║  回调里强转后用，比如 (MyContext*)user_data。             ║
// ╚══════════════════════════════════════════════════════════╝

typedef struct _Button Button;  // 前置声明（结构体定义在后面）

typedef void (*BtnCallback)(Button *handle, void *user_data);

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 C：枚举 — 给数字起有意义的名称                    ║
// ║                                                          ║
// ║  第一个值手动 = 0，后面自动 +1。                           ║
// ║  BTN_EVENT_COUNT 不是真实事件，而是「事件种类数」= 7。    ║
// ║  用来开数组：cb[BTN_EVENT_COUNT] 就是 cb[7]。             ║
// ║  BTN_NONE_PRESS 放在最后 = 8，表示「没事件」。            ║
// ╚══════════════════════════════════════════════════════════╝

typedef enum {
    BTN_PRESS_DOWN       = 0,  // 按下
    BTN_PRESS_UP,              // 松开
    BTN_PRESS_REPEAT,          // 重复按压（第 2 次及以后）
    BTN_SINGLE_CLICK,          // 单击（窗口超时 + repeat==1）
    BTN_DOUBLE_CLICK,          // 双击（窗口超时 + repeat==2）
    BTN_LONG_PRESS_START,      // 长按开始（只触发一次）
    BTN_LONG_PRESS_HOLD,       // 长按持续（每 tick 触发一次！）
    BTN_EVENT_COUNT,           // ← 总共 7 种事件（用来声明数组大小）
    BTN_NONE_PRESS             // 无事件
} ButtonEvent;

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 D：状态枚举 — 状态机的 5 个节点                   ║
// ║                                                          ║
// ║  状态之间的跳转条件在 button_handler 的 switch 里。      ║
// ║  记住这 5 个词就能看懂整个状态机。                        ║
// ╚══════════════════════════════════════════════════════════╝

typedef enum {
    BTN_STATE_IDLE      = 0,  // 空闲，等人按
    BTN_STATE_PRESS,          // 按下中，等松或等超时
    BTN_STATE_RELEASE,        // 松开后，等再次按下（双击窗口）
    BTN_STATE_REPEAT,         // 连按中
    BTN_STATE_LONG_HOLD       // 长按保持中
} ButtonState;

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 E：位域结构体 — 精打细算的内存布局               ║
// ║                                                          ║
// ║  位域语法： type field_name : bit_width;                  ║
// ║                                                          ║
// ║  struct _Button {                                        ║
// ║      uint16_t ticks;                      // 16 位       ║
// ║      uint8_t  repeat : 4;                 // 4 位        ║
// ║      uint8_t  event : 4;                  // 4 位        ║
// ║      uint8_t  state : 3;                  // 3 位        ║
// ║      uint8_t  debounce_cnt : 3;           // 3 位        ║
// ║      uint8_t  active_level : 1;           // 1 位        ║
// ║      uint8_t  button_level : 1;           // 1 位        ║
// ║      ...                                  // 总共 32 位   ║
// ║      ... 指针和函数指针（平台相关 32/64 位）              ║
// ║  };                                                      ║
// ║                                                          ║
// ║  为什么用位域？                                           ║
// ║    嵌入式 RAM 很贵（几 KB 到几十 KB）。                    ║
// ║    如果每个字段都用 uint8_t，单按键就要很多字节。         ║
// ║    这个结构体整个大约 30 字节，支持 10 个按键也就 300 字节。║
// ║                                                          ║
// ║  ⚠ 位域的布局顺序和字节序有关，不同编译器可能不同。       ║
// ║    但这不是问题——库里只在内部用，不对外暴露。             ║
// ╚══════════════════════════════════════════════════════════╝

struct _Button {
    // ── 运行时数据 ──
    uint16_t ticks;                // 计时器（5ms 精度，最大 65535）
    uint8_t  repeat  : 4;          // 连按次数（0~15，最大值 PRESS_REPEAT_MAX_NUM）
    uint8_t  event  : 4;           // 当前事件（存 ButtonEvent 枚举值，4 位够用）
    uint8_t  state  : 3;           // 状态机当前状态（存 ButtonState 枚举值，3 位够用）
    uint8_t  debounce_cnt : 3;     // 消抖计数器（0~7，最大值 DEBOUNCE_TICKS）
    uint8_t  active_level : 1;     // 有效电平：0 = 按下时低电平，1 = 按下时高电平
    uint8_t  button_level : 1;     // 当前消抖后的电平（可靠值，状态机用这个做判断）

    // ── 配置数据 ──
    uint8_t  button_id;            // 按键编号（传给 GPIO 读取函数的参数）

    // ═══════════════════════════════════════════════════
    // 知识点 F：函数指针成员 — 这是解耦的关键
    //
    // 这个成员存一个「能读 GPIO 电平的函数」的地址。
    // 类型是 uint8_t (*)(uint8_t button_id)
    //     返回 uint8_t，参数 uint8_t
    //
    // 初始化时：
    //   handle->hal_button_level = my_gpio_read_func;
    //
    // 调用时：
    //   handle->hal_button_level(handle->button_id);
    //
    // 换一颗芯片？只换 my_gpio_read_func 的实现。
    // 库代码一行不动。——这就是「依赖倒转」。
    // ═══════════════════════════════════════════════════
    uint8_t (*hal_button_level)(uint8_t button_id);

    // ── 回调函数数组 ──
    // cb[BTN_PRESS_DOWN]     = 按下回调
    // cb[BTN_PRESS_UP]       = 松开回调
    // cb[BTN_PRESS_REPEAT]   = 重复按回调
    // cb[BTN_SINGLE_CLICK]   = 单击回调
    // cb[BTN_DOUBLE_CLICK]   = 双击回调
    // cb[BTN_LONG_PRESS_START] = 长按开始回调
    // cb[BTN_LONG_PRESS_HOLD]  = 长按保持回调
    // 未绑定的槽位 = NULL，调用前会检查
    BtnCallback cb[BTN_EVENT_COUNT];

    // ── 用户上下文指针 ──
    // attach 时传入，回调时原样返回。
    // 用 void* 可以指向任何类型的数据。
    void *user_data;

    // ── 链表指针 ──
    // 指向下一个按键，构成单向链表。
    // NULL = 链表末尾。
    Button *next;
};

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 G：条件编译 — 一份代码，两种环境                   ║
// ║                                                          ║
// ║  裸机（默认）:                                            ║
// ║    MULTIBUTTON_LOCK()   → 空（什么都没有）                ║
// ║    MULTIBUTTON_UNLOCK() → 空                              ║
// ║    开销：0。                                              ║
// ║                                                          ║
// ║  RTOS 环境：                                              ║
// ║    在 #include 之前定义 MULTIBUTTON_THREAD_SAFE           ║
// ║    并提供锁宏，例如：                                     ║
// ║      #define MULTIBUTTON_LOCK()   osMutexAcquire(...)    ║
// ║      #define MULTIBUTTON_UNLOCK() osMutexRelease(...)    ║
// ║                                                          ║
// ║  这是嵌入式开发中非常常见的模式：                          ║
// ║  接口不变，底层实现编译期切换。                           ║
// ╚══════════════════════════════════════════════════════════╝

#ifdef MULTIBUTTON_THREAD_SAFE
#if !defined(MULTIBUTTON_LOCK) || !defined(MULTIBUTTON_UNLOCK)
#error                                                                         \
    "Define MULTIBUTTON_LOCK() and MULTIBUTTON_UNLOCK() when using MULTIBUTTON_THREAD_SAFE"
#endif
#else
#define MULTIBUTTON_LOCK()
#define MULTIBUTTON_UNLOCK()
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ╔══════════════════════════════════════════════════════════╗
// ║  公共 API — 总共 10 个函数                                ║
// ║                                                          ║
// ║  核心 6 个：                                              ║
// ║    button_init()    — 初始化结构体                        ║
// ║    button_attach()  — 绑定事件回调                        ║
// ║    button_detach()  — 解绑事件回调                        ║
// ║    button_start()   — 激活按键（加入处理链表）            ║
// ║    button_stop()    — 停用按键（移出处理链表）            ║
// ║    button_ticks()   — 每 5ms 调用一次（驱动状态机）       ║
// ║                                                          ║
// ║  辅助 4 个：                                              ║
// ║    button_get_event()         — 获取当前事件（轮询用）    ║
// ║    button_get_repeat_count()  — 获取连按次数              ║
// ║    button_reset()             — 软复位到 IDLE             ║
// ║    button_is_pressed()        — 是否按下                  ║
// ╚══════════════════════════════════════════════════════════╝

void button_init(Button *handle, uint8_t (*pin_level)(uint8_t),
                 uint8_t active_level, uint8_t button_id);
void button_attach(Button *handle, ButtonEvent event, BtnCallback cb,
                   void *user_data);
void button_detach(Button *handle, ButtonEvent event);
ButtonEvent button_get_event(Button *handle);
int  button_start(Button *handle);
void button_stop(Button *handle);
void button_ticks(void);

uint8_t button_get_repeat_count(Button *handle);
void    button_reset(Button *handle);
int     button_is_pressed(Button *handle);

#ifdef __cplusplus
}
#endif

#endif
