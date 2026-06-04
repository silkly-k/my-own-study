/*
 * ===================================================================
 * MultiButton 核心实现 — 学习注释版
 * 原始作者: Zibin Zheng
 * 学习目标: 状态机思想 + 函数指针解耦 + 链表管理 + 消抖策略
 * ===================================================================
 */

#include "multi_button.h"

// ──────────────────────────────────────────────
// 知识点 1: do { } while(0) 宏写法
// 为什么这么写？因为 if/else 下不会出 bug：
//
//    if (cond)
//        EVENT_CB(ev);   // ← 展开后是 do{...}while(0); 加分号正常
//    else
//        ...
//
// 如果不用 do-while 包裹，多语句宏在 if 下会断掉。
// 另外，handle->cb[ev](handle, handle->user_data) 就是
// 「通过函数指针调用回调，把按键自己和自己数据传进去」。
// ──────────────────────────────────────────────
#define EVENT_CB(ev)                                                          \
    do {                                                                      \
        if (handle->cb[ev])                                                   \
            handle->cb[ev](handle, handle->user_data);                        \
    } while (0)

// ──────────────────────────────────────────────
// 知识点 2: 全局链表头
// 所有按键串成一条单向链表，这个指针永远指向第一个按键。
// 用 static 限定，外部不可见——封装。
// ──────────────────────────────────────────────
static Button *head_handle = NULL;

// 前置声明
static void button_handler(Button *handle);
static inline uint8_t button_read_level(Button *handle);

// ╔══════════════════════════════════════════════════════════╗
// ║              核心 API 实现                              ║
// ╚══════════════════════════════════════════════════════════╝

/**
 * @brief  初始化一个按键结构体
 * @param  handle       : 按键句柄（你的 Button 变量地址）
 * @param  pin_level    : 函数指针！你传一个读 GPIO 的函数进来
 * @param  active_level : 按下时的电平，0=低电平有效，1=高电平有效
 * @param  button_id    : 按键编号，会传给 pin_level 函数
 *
 * ── 学习要点 ──
 * 为什么 pin_level 是函数指针？
 *   库不关心你的硬件是什么。你只要告诉它「怎么读电平」，
 *   STM32、ESP32、Arduino 都能用，一行库代码都不用改。
 *   这就是「依赖反转」——库依赖你的接口，而不是你依赖库的实现。
 */
void button_init(Button *handle, uint8_t (*pin_level)(uint8_t),
                 uint8_t active_level, uint8_t button_id)
{
    if (!handle || !pin_level)
        return; // 空指针检查——健壮性

    memset(handle, 0, sizeof(Button)); // 全部清零

    // ── 初始电平设为 active_level 的反 ——
    // 这样第一次读到有效电平时一定会触发 debounce 流程
    handle->button_level   = !active_level;
    handle->hal_button_level = pin_level;  // ← 函数指针赋值！
    handle->active_level   = active_level;
    handle->button_id      = button_id;
    handle->state          = BTN_STATE_IDLE;
    handle->event          = (uint8_t)BTN_NONE_PRESS;
}

/**
 * @brief  绑定事件回调
 * @param  handle    : 按键句柄
 * @param  event     : 你要监听哪种事件（SINGLE_CLICK / LONG_PRESS...）
 * @param  cb        : 事件发生时调哪个函数
 * @param  user_data : 自定义数据指针，会原样传给你的回调
 *
 * ── 学习要点 ──
 * cb[event] 是一个函数指针数组，cb[BTN_SINGLE_CLICK] 存单击回调，
 * cb[BTN_DOUBLE_CLICK] 存双击回调...
 * 一共 BTN_EVENT_COUNT(7) 个槽位，每个槽位存一个函数指针。
 * 如果要解绑，button_detach 里把对应槽位设为 NULL。
 *
 * user_data 让你传自定义上下文——比如 LED 引脚号、计数器地址等。
 * 回调拿到后强制转换类型就能用了。
 */
void button_attach(Button *handle, ButtonEvent event, BtnCallback cb,
                   void *user_data)
{
    if (!handle || event >= BTN_EVENT_COUNT)
        return;
    handle->cb[event] = cb; // ← 函数指针存入数组对应位置
    handle->user_data = user_data;
}

/**
 * @brief  解绑事件回调（把对应槽位清空）
 */
void button_detach(Button *handle, ButtonEvent event)
{
    if (!handle || event >= BTN_EVENT_COUNT)
        return;
    handle->cb[event] = NULL;
}

/**
 * @brief  获取当前按键事件（轮询模式用）
 */
ButtonEvent button_get_event(Button *handle)
{
    if (!handle)
        return BTN_NONE_PRESS;
    return (ButtonEvent)(handle->event);
}

/**
 * @brief  获取连按次数
 */
uint8_t button_get_repeat_count(Button *handle)
{
    if (!handle)
        return 0;
    return handle->repeat;
}

/**
 * @brief  重置按键状态
 *
 * ── 学习要点 ──
 * 这个函数把所有运行时状态清掉：
 *   state → IDLE
 *   ticks → 0
 *   repeat → 0
 *   event → NONE
 *   debounce_cnt → 0
 * 相当于一个「软复位」，按键回到刚初始化完的状态。
 * 用在：切换模式、出错恢复、重新开始一轮检测。
 */
void button_reset(Button *handle)
{
    if (!handle)
        return;
    handle->state        = BTN_STATE_IDLE;
    handle->ticks        = 0;
    handle->repeat       = 0;
    handle->event        = (uint8_t)BTN_NONE_PRESS;
    handle->debounce_cnt = 0;
}

/**
 * @brief  查询按键当前是否被按下
 * @retval 1=按下, 0=没按下, -1=参数错误
 *
 * ── 学习要点 ──
 * button_level 是经过消抖后的电平，不是你直接读到的 GPIO 电平。
 * 这就是「可靠数据」——只有消抖确认过的才会更新 button_level。
 */
int button_is_pressed(Button *handle)
{
    if (!handle)
        return -1;
    return (handle->button_level == handle->active_level) ? 1 : 0;
}

// ╔══════════════════════════════════════════════════════════╗
// ║              链表管理（按键的注册/注销）                  ║
// ╚══════════════════════════════════════════════════════════╝

/**
 * @brief  把按键加入处理链表
 * @retval 0=成功, -1=已存在(重复添加), -2=参数无效
 *
 * ── 学习要点：单向链表头插法 ──
 *
 * 初始: head_handle → NULL
 *
 * 插入 btn1:
 *   btn1.next = head_handle (NULL)
 *   head_handle = &btn1
 *   结果: head → btn1 → NULL
 *
 * 插入 btn2:
 *   btn2.next = head_handle (&btn1)
 *   head_handle = &btn2
 *   结果: head → btn2 → btn1 → NULL
 *
 * 先遍历检查是否重复（同样的 handle 已经在链表里了？）
 * 不重复就头插——O(1) 插到最前面，快。
 *
 * ── 学习要点：MULTIBUTTON_LOCK/UNLOCK ──
 * 裸机下展开为空，零开销。
 * RTOS 下展开为互斥锁，保护链表操作不被中断打乱。
 * 条件编译实现「一份代码，两种环境」。
 */
int button_start(Button *handle)
{
    if (!handle)
        return -2;

    MULTIBUTTON_LOCK();
    Button *target = head_handle;
    while (target) {
        if (target == handle) {        // 已经在链表里了
            MULTIBUTTON_UNLOCK();
            return -1;
        }
        target = target->next;
    }
    // 头插法
    handle->next = head_handle;
    head_handle  = handle;
    MULTIBUTTON_UNLOCK();
    return 0;
}

/**
 * @brief  从处理链表中移除按键
 *
 * ── 学习要点：二级指针删除链表节点 ──
 *
 * Button** curr 是指向「指针的指针」。
 *
 *   head → [A] → [B] → [C] → NULL
 *   curr 指向 &head (即指向 head 这个指针本身)
 *   *curr 就是 head 存储的地址（指向 A）
 *
 * 要删除 B：
 *   遍历到 entry==B 时，curr 指向 A.next
 *   *curr = entry->next   →   把 A.next 指向 C
 *   效果：head → [A] → [C] → NULL
 *
 * 用二级指针的好处：不用单独处理「删除的是 head」的情况，
 * 因为 head 本身也是一个指针，curr 指向它和指向 A.next 没区别。
 *
 * ── 学习要点：entry->next = NULL ──
 * 防止野指针。删除后这个按键可能被重新 init 再用，
 * next 悬空可能导致链表损坏。
 */
void button_stop(Button *handle)
{
    if (!handle)
        return;

    MULTIBUTTON_LOCK();
    Button **curr;
    for (curr = &head_handle; *curr;) {
        Button *entry = *curr;
        if (entry == handle) {
            *curr       = entry->next; // 把当前节点从链表中摘除
            entry->next = NULL;        // 清空被删除节点的指针
            MULTIBUTTON_UNLOCK();
            return;
        } else {
            curr = &entry->next; // 继续往后找
        }
    }
    MULTIBUTTON_UNLOCK();
}

// ╔══════════════════════════════════════════════════════════╗
// ║              读取电平 & 状态机核心                        ║
// ╚══════════════════════════════════════════════════════════╝

/**
 * @brief  读取按键电平（通过函数指针间接调用）
 *
 * ── 学习要点：inline 的作用 ──
 * static inline 告诉编译器：这个函数只在当前文件用，尽量内联展开。
 * 这里就是一个简单的转发调用，内联后减少了函数调用开销。
 * 在 5ms 定时器中断里跑的代码，能省一点是一点。
 */
static inline uint8_t button_read_level(Button *handle)
{
    return handle->hal_button_level(handle->button_id);
    //     ↑                    ↑
    //     │                    └── 传按键编号
    //     └── 调用你注册的函数（可能是 HAL_GPIO_ReadPin）
}

/**
 * @brief  按键状态机——整个库的心脏
 *
 * 每 5ms 被 button_ticks() 调用一次。
 * 执行顺序：
 *   ① 读当前 GPIO 电平
 *   ② ticks 计时（不在 IDLE 就 +1，最多到 65535 不再加）
 *   ③ 消抖判断（连续 N 次读到同一新电平才算有效变化）
 *   ④ 根据 state 和条件决定下一步（switch 状态机）
 *
 * ═══════════════════════════════════════════════════════
 *  状态机图示（5 个状态，记住这个图就看懂了代码）
 * ═══════════════════════════════════════════════════════
 *
 *                   按太久(>LONG_TICKS)
 *              ┌── PRESS ────────────────▶ LONG_HOLD ──┐
 *             │     │                        │  松开     │
 *     按下      │     │ 松开                     │          │
 *   ┌──────────┘     ▼                        ▼          │
 *  IDLE            RELEASE ◀─────────────────────────────┘
 *   ▲              │      ▲
 *   │      超时     │      │ 又按了（<SHORT_TICKS 窗口内）
 *   │    → 单击/双击 │      │
 *   │              ▼      │
 *   └──────── ─ ─ ─ ─ ─ REPEAT ──按太久──▶ PRESS(重新开始)
 *          (回到 IDLE)        └── 松开 < SHORT_TICKS
 *                                  → 回到 RELEASE (继续等)
 *
 *  关键时间阈值：
 *    SHORT_TICKS = 300ms/5ms = 60 tick   ← 双击判断窗口
 *    LONG_TICKS  = 1000ms/5ms = 200 tick  ← 长按触发阈值
 *    DEBOUNCE_TICKS = 3                  ← 需要连续 3 次读到同一电平
 */
static void button_handler(Button *handle)
{
    // ─── 第①步：读电平 ───
    uint8_t read_gpio_level = button_read_level(handle);

    // ─── 第②步：计时器累加（不在 IDLE 就计时）───
    //   饱和处理：到了 UINT16_MAX(65535) 就不再增加
    //   防溢出翻转，约 327 秒的连续操作不会出问题
    if (handle->state > BTN_STATE_IDLE) {
        if (handle->ticks < UINT16_MAX) {
            handle->ticks++;
        }
    }

    // ─── 第③步：消抖 ────────────────────────────
    //   原理：不是延时！而是「连续 DEBOUNCE_TICKS 次读到同一新电平」
    //   才认为电平真的变了。
    //
    //   举例（DEBOUNCE_TICKS=3）：
    //     读数序列: 0 0 1 1 0 1 1 1 1 ...
    //     debounce: 0 0 1 2 0 1 2 3 ✓ (确认变高)
    //
    //   中间那个 0 让计数器重置了——这就是消抖的效果：
    //   抖动产生的短暂跳变无法积累到 DEBOUNCE_TICKS
    //
    //   注意：button_level 是被消抖保护后的「可靠电平」，
    //   后续状态机只用 button_level 做判断，不用原始 GPIO 值。
    if (read_gpio_level != handle->button_level) {
        // 读到的电平与当前记录的不同 → 开始计数
        if (++(handle->debounce_cnt) >= DEBOUNCE_TICKS) {
            // 连续 N 次都一样 → 确认电平改变了
            handle->button_level = read_gpio_level;
            handle->debounce_cnt = 0;
        }
    } else {
        // 电平没变 → 重置消抖计数（抖动被打断了）
        handle->debounce_cnt = 0;
    }

    // ─── 第④步：状态机 ───
    //   根据当前状态 + 消抖后的电平 + 计时 → 决定跳转到哪
    switch (handle->state) {

    // ─────────────────────────────────────────
    // 状态 0: IDLE — 空闲，等待按下
    // ─────────────────────────────────────────
    case BTN_STATE_IDLE:
        if (handle->button_level == handle->active_level) {
            // 有人按了！
            handle->event   = (uint8_t)BTN_PRESS_DOWN;
            EVENT_CB(BTN_PRESS_DOWN);  // 立刻通知：按下了
            handle->ticks   = 0;       // 开始计时
            handle->repeat  = 1;       // 这是第 1 次按
            handle->state   = BTN_STATE_PRESS;  // → 进入 PRESS
        } else {
            handle->event = (uint8_t)BTN_NONE_PRESS;
        }
        break;

    // ─────────────────────────────────────────
    // 状态 1: PRESS — 正在按下，等两件事
    //   ① 松开了？ → RELEASE（快速松开）
    //   ② 按太久了？ → LONG_HOLD（长按来了）
    //
    //   注意：先判断「松了」再判断「按久了」，
    //   所以松开永远优先于长按。
    // ─────────────────────────────────────────
    case BTN_STATE_PRESS:
        if (handle->button_level != handle->active_level) {
            // ✅ 松开了 → 还没到长按阈值，属于短按序列
            handle->event = (uint8_t)BTN_PRESS_UP;
            EVENT_CB(BTN_PRESS_UP);
            handle->ticks = 0;                  // 计时归零
            handle->state = BTN_STATE_RELEASE;   // → 进入等待判断
        } else if (handle->ticks > LONG_TICKS) {
            // ✅ 按住超过 LONG_TICKS(200, 即1秒) → 长按！
            handle->event = (uint8_t)BTN_LONG_PRESS_START;
            EVENT_CB(BTN_LONG_PRESS_START);      // 只触发一次
            handle->state = BTN_STATE_LONG_HOLD; // → 进入长按保持
        }
        break;

    // ─────────────────────────────────────────
    // 状态 2: RELEASE — 松开后等一会
    //   这是一个「判断窗口」，长度 = SHORT_TICKS(300ms)
    //
    //   在此期间：
    //     又按了？ → REPEAT（连按，计数 +1）
    //     超时了？ → 根据 repeat 次数判定单击/双击，然后回 IDLE
    //
    //   这是整个设计最巧妙的地方：
    //     单击和双击不是立刻判断的，而是「等窗口关闭」后
    //     根据这段时间内按了几次来决定的。
    // ─────────────────────────────────────────
    case BTN_STATE_RELEASE:
        if (handle->button_level == handle->active_level) {
            // ✅ 窗口内又按了 → 连按！
            handle->event = (uint8_t)BTN_PRESS_DOWN;
            EVENT_CB(BTN_PRESS_DOWN);
            if (handle->repeat < PRESS_REPEAT_MAX_NUM) {
                handle->repeat++;  // 连按计数 +1
            }
            handle->event = (uint8_t)BTN_PRESS_REPEAT;
            EVENT_CB(BTN_PRESS_REPEAT);  // 通知：重复按压
            handle->ticks = 0;
            handle->state = BTN_STATE_REPEAT; // → 连按状态
        } else if (handle->ticks > SHORT_TICKS) {
            // ✅ 等了 300ms 没再按 → 窗口关闭，判定结果
            if (handle->repeat == 1) {
                handle->event = (uint8_t)BTN_SINGLE_CLICK;
                EVENT_CB(BTN_SINGLE_CLICK);  // → 单击！
            } else if (handle->repeat == 2) {
                handle->event = (uint8_t)BTN_DOUBLE_CLICK;
                EVENT_CB(BTN_DOUBLE_CLICK);  // → 双击！
            }
            // repeat >= 3 不触发事件，用户自己通过 BTN_PRESS_REPEAT 处理
            handle->state = BTN_STATE_IDLE;  // → 回到空闲
        }
        break;

    // ─────────────────────────────────────────
    // 状态 3: REPEAT — 连按中
    //   你在快速连续按，每次松开后还在等。
    //
    //   两种走向：
    //     又松开了？→ 如果松得快（<SHORT_TICKS），回 RELEASE 继续等下一击
    //     按太久了？→ 降级为普通 PRESS（重新开始一轮判断）
    //
    //   「降级」的意图：你连续按了三下，第四下按住不放了，
    //   那这次就是一个新的普通按下，而不是连按的延续。
    //   所以 reset ticks，clear repeat，从 PRESS 重新开始。
    // ─────────────────────────────────────────
    case BTN_STATE_REPEAT:
        if (handle->button_level != handle->active_level) {
            // ✅ 又松开了
            handle->event = (uint8_t)BTN_PRESS_UP;
            EVENT_CB(BTN_PRESS_UP);
            if (handle->ticks < SHORT_TICKS) {
                // 松得快 → 回 RELEASE，继续等下一次按
                handle->ticks = 0;
                handle->state = BTN_STATE_RELEASE;
            } else {
                // 这次从按到松一共超过了 300ms → 不再等了
                handle->state = BTN_STATE_IDLE;
            }
        } else if (handle->ticks > SHORT_TICKS) {
            // ✅ 按太久了 → 降级为普通 PRESS
            handle->ticks  = 0;      // 重置计时（给了新一次按下的满额时间）
            handle->repeat = 0;      // 清空连按计数（不再属于这次连按序列）
            handle->state  = BTN_STATE_PRESS;
        }
        break;

    // ─────────────────────────────────────────
    // 状态 4: LONG_HOLD — 长按保持
    //   已经判定为长按了，还在继续按住。
    //
    //   只要还按着 → 每个周期都触发 BTN_LONG_PRESS_HOLD
    //     注意：5ms 一次！你的回调要做节流（throttle）
    //   松开了 → 触发松开事件，回 IDLE
    // ─────────────────────────────────────────
    case BTN_STATE_LONG_HOLD:
        if (handle->button_level == handle->active_level) {
            // 还在按 → 持续触发（⚠ 5ms 一次，很频繁）
            handle->event = (uint8_t)BTN_LONG_PRESS_HOLD;
            EVENT_CB(BTN_LONG_PRESS_HOLD);
        } else {
            // 终于松了
            handle->event = (uint8_t)BTN_PRESS_UP;
            EVENT_CB(BTN_PRESS_UP);
            handle->state = BTN_STATE_IDLE; // → 回到空闲
        }
        break;

    // ─────────────────────────────────────────
    // 安全兜底：不可能到达，但万一到了就复位
    // ─────────────────────────────────────────
    default:
        handle->state = BTN_STATE_IDLE;
        break;
    }
}

// ╔══════════════════════════════════════════════════════════╗
// ║              定时器入口                                  ║
// ╚══════════════════════════════════════════════════════════╝

/**
 * @brief  每 5ms 调用一次（放在定时器中断或主循环中）
 *
 * ── 学习要点：为什么先取 next 再调 handler？ ──
 *
 *   ！！！这是整个库最精妙的防御性设计 ！！！
 *
 *   Button *target = head_handle;
 *   while (target) {
 *       next = target->next;          // ① 先记住下一个
 *       button_handler(target);       // ② 再处理当前
 *       target = next;                // ③ 用记住的值继续
 *   }
 *
 *   如果在 button_handler 里，用户回调调用了 button_stop(this)
 *   把自己从链表中删除了。如果不事先保存 next，此时 target->next
 *   已经被设为 NULL，遍历就断了。
 *
 *   保存了 next 之后，无论当前节点死活，遍历都能正确继续。
 *
 * ── 学习要点：MULTIBUTTON_LOCK 的粒度 ──
 *   锁只保护链表指针的读取（从 head 取值、从 next 取值），
 *   不保护 button_handler 的执行。这样回调里可以安全调
 *   button_start/stop 而不会死锁。
 */
void button_ticks(void)
{
    Button *target;
    Button *next;

    MULTIBUTTON_LOCK();
    target = head_handle;
    MULTIBUTTON_UNLOCK();

    while (target) {
        MULTIBUTTON_LOCK();
        next = target->next; // ← 先保存下一步，防止回调中自删导致断裂
        MULTIBUTTON_UNLOCK();

        button_handler(target); // 调用状态机
        target = next;
    }
}
