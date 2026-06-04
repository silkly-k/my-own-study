/*
 * ===================================================================
 * MultiButton 单元测试 — 学习注释版
 * 本文重点学习：测试框架设计 + Mock 思想 + 边界条件测试
 * ===================================================================
 */

#include "multi_button.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 A：极简测试框架（不到 20 行）                     ║
// ║                                                          ║
// ║  核心思想：                                               ║
// ║    每个测试函数返回 0(通过) 或 1(失败)。                  ║
// ║    ASSERT 宏检查条件，不通过就打印并立即返回 1。           ║
// ║    RUN_TEST 宏给测试编号、记结果。                         ║
// ║                                                          ║
// ║  ASSERT 用 do{}while(0) 包裹的原因：                      ║
// ║    如果在 if 里用 ASSERT，不加 do-while 会语法错误。      ║
// ║                                                          ║
// ║  #expr 是什么？                                          ║
// ║    C 语言的「字符串化」操作符。                            ║
// ║    #expr 把表达式变成字符串。                              ║
// ║    ASSERT(x > 3) → "x > 3" → 打印 "FAIL: x > 3 (line X)"  ║
// ║                                                          ║
// ║  __LINE__ 是什么？                                        ║
// ║    C 预定义的宏，展开为当前行号。                          ║
// ╚══════════════════════════════════════════════════════════╝

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(expr)                                                           \
    do {                                                                       \
        if (!(expr)) {                                                         \
            printf("  FAIL: %s (line %d)\n", #expr, __LINE__);                 \
            return 1;                                                          \
        }                                                                      \
    } while (0)

#define RUN_TEST(fn)                                                           \
    do {                                                                       \
        tests_run++;                                                           \
        printf("  [%d] %s ... ", tests_run, #fn);                              \
        if (fn() == 0) {                                                       \
            tests_passed++;                                                    \
            printf("OK\n");                                                    \
        } else {                                                               \
            tests_failed++;                                                    \
            printf("FAILED\n");                                                \
        }                                                                      \
    } while (0)

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 B：Mock（模拟） — 不用硬件也能测试                 ║
// ║                                                          ║
// ║  真正的嵌入式代码要读 GPIO，但测试时没有板子。              ║
// ║  所以写一个假的 GPIO 函数：                                ║
// ║    mock_read_gpio() 不读硬件，只返回一个全局变量。         ║
// ║                                                          ║
// ║  测试时通过修改 mock_gpio_value 来控制「按键状态」：       ║
// ║    mock_gpio_value = 1  →  按键按下                        ║
// ║    mock_gpio_value = 0  →  按键松开                        ║
// ║                                                          ║
// ║  这是嵌入式开发最重要的测试技巧——                           ║
// ║  把硬件依赖「注入」进去，而不是写死在代码里。              ║
// ╚══════════════════════════════════════════════════════════╝

static uint8_t mock_gpio_value = 0;

static uint8_t mock_read_gpio(uint8_t button_id)
{
    (void)button_id;        // 我们不关心 button_id，只有一个假按键
    return mock_gpio_value; // 返回预设的值
}

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 C：时间推进器 — 模拟 5ms 时钟                      ║
// ║                                                          ║
// ║  真正的嵌入式里，button_ticks() 由定时器中断调用。        ║
// ║  测试里，tick_n(10) 就是连续调 10 次 button_ticks()，     ║
// ║  模拟时间过去了 10×5ms = 50ms。                           ║
// ║                                                          ║
// ║  这样你可以精确控制「按键按了多久」。                       ║
// ╚══════════════════════════════════════════════════════════╝

static void tick_n(int n)
{
    for (int i = 0; i < n; i++) {
        button_ticks();
    }
}

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 D：事件记录器 — 验证回调是否被正确调用             ║
// ║                                                          ║
// ║  每个回调函数在被调用时，把它收到的事件类型存到 event_log  ║
// ║  测试结束时检查 event_log，确认：                          ║
// ║    - 按了事件是否触发？  (has_event)                       ║
// ║    - 触发了多少次？      (count_event)                     ║
// ║    - 不该触发的事件没触发？ (!has_event)                   ║
// ║                                                          ║
// ║  (void)btn; (void)user_data; 的作用：                     ║
// ║    告诉编译器「我知道这两个参数没用，别报警告」。          ║
// ╚══════════════════════════════════════════════════════════╝

#define MAX_EVENTS 64
static ButtonEvent event_log[MAX_EVENTS];
static int        event_count = 0;

static void reset_event_log(void) { event_count = 0; }

// ── 7 种事件各一个回调，各记录各的事件类型 ──
static void log_press_down(Button *btn, void *user_data)
{
    (void)btn;
    (void)user_data;
    if (event_count < MAX_EVENTS)
        event_log[event_count++] = BTN_PRESS_DOWN;
}
static void log_press_up(Button *btn, void *user_data)
{
    (void)btn;
    (void)user_data;
    if (event_count < MAX_EVENTS)
        event_log[event_count++] = BTN_PRESS_UP;
}
static void log_single_click(Button *btn, void *user_data)
{
    (void)btn;
    (void)user_data;
    if (event_count < MAX_EVENTS)
        event_log[event_count++] = BTN_SINGLE_CLICK;
}
static void log_double_click(Button *btn, void *user_data)
{
    (void)btn;
    (void)user_data;
    if (event_count < MAX_EVENTS)
        event_log[event_count++] = BTN_DOUBLE_CLICK;
}
static void log_long_start(Button *btn, void *user_data)
{
    (void)btn;
    (void)user_data;
    if (event_count < MAX_EVENTS)
        event_log[event_count++] = BTN_LONG_PRESS_START;
}
static void log_long_hold(Button *btn, void *user_data)
{
    (void)btn;
    (void)user_data;
    if (event_count < MAX_EVENTS)
        event_log[event_count++] = BTN_LONG_PRESS_HOLD;
}
static void log_repeat(Button *btn, void *user_data)
{
    (void)btn;
    (void)user_data;
    if (event_count < MAX_EVENTS)
        event_log[event_count++] = BTN_PRESS_REPEAT;
}

// ── 查询事件日志 ──
static int has_event(ButtonEvent ev)
{
    for (int i = 0; i < event_count; i++) {
        if (event_log[i] == ev)
            return 1;
    }
    return 0;
}

static int count_event(ButtonEvent ev)
{
    int c = 0;
    for (int i = 0; i < event_count; i++) {
        if (event_log[i] == ev)
            c++;
    }
    return c;
}

// ╔══════════════════════════════════════════════════════════╗
// ║  知识点 E：测试夹具（setup / teardown）                    ║
// ║                                                          ║
// ║  每个测试前：初始化按键 + 绑定所有 7 种事件的日志回调      ║
// ║  每个测试后：停用按键 + 恢复 mock 状态                     ║
// ║                                                          ║
// ║  保证每个测试独立运行，互不干扰。                           ║
// ║                                                          ║
// ║  button_init 的参数含义：                                  ║
// ║    &test_btn          — 按键结构体地址                      ║
// ║    mock_read_gpio     — 读电平的函数（我们假的那个）        ║
// ║    1                  — active_level=1 按下是高电平         ║
// ║    1                  — button_id=1                        ║
// ╚══════════════════════════════════════════════════════════╝

static Button test_btn;

static void setup_button(void)
{
    mock_gpio_value = 0;
    reset_event_log();
    button_init(&test_btn, mock_read_gpio, 1, 1);
    button_attach(&test_btn, BTN_PRESS_DOWN, log_press_down, NULL);
    button_attach(&test_btn, BTN_PRESS_UP, log_press_up, NULL);
    button_attach(&test_btn, BTN_SINGLE_CLICK, log_single_click, NULL);
    button_attach(&test_btn, BTN_DOUBLE_CLICK, log_double_click, NULL);
    button_attach(&test_btn, BTN_LONG_PRESS_START, log_long_start, NULL);
    button_attach(&test_btn, BTN_LONG_PRESS_HOLD, log_long_hold, NULL);
    button_attach(&test_btn, BTN_PRESS_REPEAT, log_repeat, NULL);
    button_start(&test_btn);
}

static void teardown_button(void)
{
    button_stop(&test_btn);
    mock_gpio_value = 0;
}

// ╔══════════════════════════════════════════════════════════╗
// ║                    16 个测试用例                          ║
// ║                                                          ║
// ║  每个测试验证一个具体行为。                                ║
// ║  命名以 test_ 开头，RUN_TEST 宏通过 #fn 自动显示名称。     ║
// ║                                                          ║
// ║  测试结构（每个都一样）：                                  ║
// ║    ① setup           — 初始化                             ║
// ║    ② 模拟按键操作    — 改 mock_gpio_value + 推进时间      ║
// ║    ③ ASSERT 验证     — 检查事件日志                       ║
// ║    ④ teardown        — 清理                               ║
// ╚══════════════════════════════════════════════════════════╝

// ─────────────────────────────────────────────
// 测试 1：单击
//
// 模拟：按 50ms → 松 → 等超时
// 预期：PRESS_DOWN ✓  PRESS_UP ✓  SINGLE_CLICK ✓
//       不应该有 DOUBLE_CLICK 和 LONG_PRESS
//
// DEBOUNCE_TICKS + 10：消抖需要 3 个 tick，
//   多加 10 个保证状态机有足够时间处理。
//   DEBOUNCE_TICKS=3 tick = 15ms，+10 tick = 50ms 实际按了 65ms
//   然后松开等 SHORT_TICKS+10 = 70 tick = 350ms > 300ms 窗口
// ─────────────────────────────────────────────
static int test_single_click(void)
{
    setup_button();

    /* 按下 50ms */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + 10);

    /* 松开，等到超时 */
    mock_gpio_value = 0;
    tick_n(DEBOUNCE_TICKS + SHORT_TICKS + 10);

    ASSERT(has_event(BTN_PRESS_DOWN));
    ASSERT(has_event(BTN_PRESS_UP));
    ASSERT(has_event(BTN_SINGLE_CLICK));
    ASSERT(!has_event(BTN_DOUBLE_CLICK));    // 不是双击
    ASSERT(!has_event(BTN_LONG_PRESS_START)); // 不是长按

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 2：双击
//
// 模拟：按松 → 稍等 → 再按松（在 SHORT_TICKS 窗口内）
// 预期：DOUBLE_CLICK ✓  两次 PRESS_DOWN
//
// 关键：两次按之间间隔很短（只有 5 tick），
//   第二次按下时还在 RELEASE 的等待窗口内，所以判定为连按。
// ─────────────────────────────────────────────
static int test_double_click(void)
{
    setup_button();

    /* 第一次按松 */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + 10);
    mock_gpio_value = 0;
    tick_n(DEBOUNCE_TICKS + 5);

    /* 第二次按松（窗口内） */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + 10);
    mock_gpio_value = 0;
    tick_n(DEBOUNCE_TICKS + SHORT_TICKS + 10);

    ASSERT(has_event(BTN_DOUBLE_CLICK));
    ASSERT(count_event(BTN_PRESS_DOWN) == 2);

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 3：长按
//
// 模拟：按住超过 LONG_TICKS(200 tick = 1秒)
// 预期：PRESS_DOWN ✓  LONG_PRESS_START ✓  LONG_PRESS_HOLD ✓
//       LONG_PRESS_START 只触发一次
//       松开后 PRESS_UP ✓
// ─────────────────────────────────────────────
static int test_long_press(void)
{
    setup_button();

    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + LONG_TICKS + 20); // 按住 > 1 秒

    ASSERT(has_event(BTN_PRESS_DOWN));
    ASSERT(has_event(BTN_LONG_PRESS_START));
    ASSERT(has_event(BTN_LONG_PRESS_HOLD));
    ASSERT(count_event(BTN_LONG_PRESS_START) == 1); // 只触发一次

    /* 松开 */
    mock_gpio_value = 0;
    tick_n(DEBOUNCE_TICKS + 10);
    ASSERT(has_event(BTN_PRESS_UP));

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 4：连按（3 次快速按松）
//
// 模拟：连续 3 次快速按松
// 预期：PRESS_DOWN 触发 3 次
//       PRESS_REPEAT 触发（第二次及以后的通知）
// ─────────────────────────────────────────────
static int test_repeat_press(void)
{
    setup_button();

    for (int i = 0; i < 3; i++) {
        mock_gpio_value = 1;
        tick_n(DEBOUNCE_TICKS + 8);
        mock_gpio_value = 0;
        tick_n(DEBOUNCE_TICKS + 5);
    }

    /* 等超时 */
    tick_n(SHORT_TICKS + 20);

    ASSERT(count_event(BTN_PRESS_DOWN) == 3);
    ASSERT(has_event(BTN_PRESS_REPEAT));

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 5：消抖 — 抖动被过滤
//
// 模拟：快速翻转电平（每次 tick 变化一次）
// 预期：没有 PRESS_DOWN 事件
//
// 关键：消抖需要「连续 3 次」读到同一电平。
//   每次 tick 都翻转的话，debounce_cnt 永远到不了 3。
// ─────────────────────────────────────────────
static int test_debounce(void)
{
    setup_button();

    /* 快速翻转 20 次 —— 模拟抖动 */
    for (int i = 0; i < 20; i++) {
        mock_gpio_value = (i % 2); // 0,1,0,1,0,1...
        tick_n(1);
    }
    mock_gpio_value = 0;
    tick_n(10);

    ASSERT(!has_event(BTN_PRESS_DOWN));     // 没触发按下
    ASSERT(!has_event(BTN_SINGLE_CLICK));    // 没触发单击

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 6：REPEAT→PRESS 过渡（ticks 重置验证）
//
// 这是一个修复过的 bug 场景：
//   快速双击 → 第二下按住不放 → REPEAT 态按太久
//   → 降级为 PRESS → ticks 被重置了
//   → 所以不会立刻触发 LONG_PRESS_START
//
// 验证：降级后必须先等完 LONG_TICKS 才能再次触发长按。
// ─────────────────────────────────────────────
static int test_repeat_to_press_transition(void)
{
    setup_button();

    /* 第一次快速按松 */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + 8);
    mock_gpio_value = 0;
    tick_n(DEBOUNCE_TICKS + 5);

    /* 第二次按下后继续按住 */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + SHORT_TICKS + 20); // 触发 REPEAT→PRESS 降级

    /* ticks 被重置了，所以不该触发长按 */
    ASSERT(!has_event(BTN_LONG_PRESS_START));

    /* 继续等到 LONG_TICKS */
    tick_n(LONG_TICKS + 10);
    ASSERT(has_event(BTN_LONG_PRESS_START)); // 现在该有了

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 7：重复 start 返回 -1
//
// 验证：button_start 对已在链表中的按钮返回 -1
// ─────────────────────────────────────────────
static int test_start_duplicate(void)
{
    setup_button();
    int ret = button_start(&test_btn);
    ASSERT(ret == -1);
    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 8：stop 后 restart 正常工作
//
// 验证生命周期：init → start → stop → reset → start 正常
// ─────────────────────────────────────────────
static int test_stop_and_restart(void)
{
    setup_button();

    button_stop(&test_btn);
    reset_event_log();

    /* stop 后按键不响应 */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + 10);
    mock_gpio_value = 0;
    tick_n(DEBOUNCE_TICKS + SHORT_TICKS + 10);
    ASSERT(!has_event(BTN_PRESS_DOWN)); // ← 确实没响应

    /* restart */
    button_reset(&test_btn);
    button_start(&test_btn);
    reset_event_log();

    /* 现在应该正常响应 */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + 10);
    mock_gpio_value = 0;
    tick_n(DEBOUNCE_TICKS + SHORT_TICKS + 10);
    ASSERT(has_event(BTN_SINGLE_CLICK));

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 9：NULL 句柄安全性
//
// 验证：所有 API 对 NULL 指针不崩溃
// 这是健壮性测试——库不能因为用户传错参数就挂掉。
// ─────────────────────────────────────────────
static int test_null_handle(void)
{
    /* 这些都不应该崩溃 */
    button_init(NULL, mock_read_gpio, 1, 1);
    button_attach(NULL, BTN_SINGLE_CLICK, log_single_click, NULL);
    button_detach(NULL, BTN_SINGLE_CLICK);
    button_stop(NULL);
    button_reset(NULL);

    ASSERT(button_get_event(NULL) == BTN_NONE_PRESS);
    ASSERT(button_get_repeat_count(NULL) == 0);
    ASSERT(button_is_pressed(NULL) == -1);
    ASSERT(button_start(NULL) == -2);

    return 0;
}

// ─────────────────────────────────────────────
// 测试 10：reset 清空状态
//
// 验证：button_reset 后 event 和 repeat 归零
// ─────────────────────────────────────────────
static int test_reset(void)
{
    setup_button();

    /* 产生一些活动 */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + 10);
    ASSERT(has_event(BTN_PRESS_DOWN));

    /* reset 后一切归零 */
    button_reset(&test_btn);
    ASSERT(button_get_event(&test_btn) == BTN_NONE_PRESS);
    ASSERT(button_get_repeat_count(&test_btn) == 0);

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 11：在回调里 stop 自己（链表遍历安全）
//
// 这验证之前讨论的「先取 next 再调 handler」设计：
//   两个按键 A 和 B 都在链表里。
//   A 的回调调用了 button_stop(A) 删除自己。
//   B 的回调仍然被正常调用，链表遍历不会因此断裂。
//
// 如果没有事先保存 next，A 被删后遍历就断了，
// B 就不会被处理。这就是那个精妙的防御设计。
// ─────────────────────────────────────────────
static Button stop_btn_a, stop_btn_b;
static int    stop_cb_called = 0;

static void cb_stop_self(Button *btn, void *user_data)
{
    (void)user_data;
    stop_cb_called++;
    button_stop(btn); // ← 在回调里删除自己！危险操作但又必须支持
}

static int test_stop_in_callback(void)
{
    stop_cb_called = 0;
    mock_gpio_value = 0;

    button_init(&stop_btn_a, mock_read_gpio, 1, 10);
    button_init(&stop_btn_b, mock_read_gpio, 1, 11);
    button_attach(&stop_btn_a, BTN_PRESS_DOWN, cb_stop_self, NULL);
    button_attach(&stop_btn_b, BTN_PRESS_DOWN, log_press_down, NULL);
    button_start(&stop_btn_a);
    button_start(&stop_btn_b);

    reset_event_log();

    /* 按下：两个按键都应该检测到 */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + 5);

    ASSERT(stop_cb_called >= 1);            // A 的回调执行了
    ASSERT(has_event(BTN_PRESS_DOWN));      // B 的回调也正常执行了

    /* 清理 */
    button_stop(&stop_btn_b);
    mock_gpio_value = 0;
    return 0;
}

// ─────────────────────────────────────────────
// 测试 12：ticks 饱和 — 防止溢出
//
// 验证：ticks 到了 UINT16_MAX(65535) 不再增加
// 如果溢出归零，所有时间判断全部失效——
// 按了 327 秒后突然又触发一次 SINGLE_CLICK 甚至不会触发。
// ─────────────────────────────────────────────
static int test_ticks_saturation(void)
{
    setup_button();

    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + 5);

    /* 手动设到接近最大值 */
    test_btn.ticks = UINT16_MAX - 2;

    /* 再推几个 tick */
    tick_n(5);

    /* 应该饱和，不是翻转到 0 */
    ASSERT(test_btn.ticks == UINT16_MAX);

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 13：三击（通过 repeat count 检测）
//
// MultiButton 只内置了单击和双击。
// 三击及以上通过 PRESS_REPEAT + button_get_repeat_count 实现。
// ─────────────────────────────────────────────
static int test_triple_click(void)
{
    setup_button();

    /* 三次快速按松 */
    for (int i = 0; i < 3; i++) {
        mock_gpio_value = 1;
        tick_n(DEBOUNCE_TICKS + 8);
        mock_gpio_value = 0;
        tick_n(DEBOUNCE_TICKS + 5);
    }

    /* 等超时 */
    tick_n(SHORT_TICKS + 20);

    ASSERT(count_event(BTN_PRESS_DOWN) == 3);
    ASSERT(has_event(BTN_PRESS_REPEAT));
    ASSERT(button_get_repeat_count(&test_btn) >= 3);

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 14：user_data 传参验证
//
// 验证：attach 时传入的 user_data，回调里能正确收到。
// 这是嵌入式里非常常用的模式——
// 把 GPIO 端口、引脚号、LED 地址等上下文通过 void* 传进回调。
// ─────────────────────────────────────────────
static int    user_data_received = 0;
static void *user_data_value    = NULL;

static void cb_check_user_data(Button *btn, void *user_data)
{
    (void)btn;
    user_data_received = 1;
    user_data_value    = user_data;
}

static int test_user_data(void)
{
    Button ud_btn;
    int    my_context = 42;

    mock_gpio_value     = 0;
    user_data_received  = 0;
    user_data_value     = NULL;

    button_init(&ud_btn, mock_read_gpio, 1, 99);
    button_attach(&ud_btn, BTN_PRESS_DOWN, cb_check_user_data, &my_context);
    button_start(&ud_btn);

    /* 触发按下 */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS + 5);

    ASSERT(user_data_received == 1);
    ASSERT(user_data_value == &my_context);
    ASSERT(*(int *)user_data_value == 42); // 通过强转取出原始数据

    button_stop(&ud_btn);
    mock_gpio_value = 0;
    return 0;
}

// ─────────────────────────────────────────────
// 测试 15：消抖边界（恰好 DEBOUNCE_TICKS 次）
//
// 验证：恰好连续 DEBOUNCE_TICKS 次读到新电平，
// 应该足以触发电平变化。
// ─────────────────────────────────────────────
static int test_debounce_boundary(void)
{
    setup_button();

    /* 恰好按 DEBOUNCE_TICKS 次 */
    mock_gpio_value = 1;
    tick_n(DEBOUNCE_TICKS);

    /* 再多给几个 tick 让状态机处理 */
    tick_n(5);

    ASSERT(has_event(BTN_PRESS_DOWN));

    teardown_button();
    return 0;
}

// ─────────────────────────────────────────────
// 测试 16：压力测试 — 50 次快速按松
//
// 验证：大量快速操作不会导致崩溃。
// 这是健壮性测试，确认没有内存泄漏或状态混乱。
// ─────────────────────────────────────────────
static int test_rapid_press_release(void)
{
    setup_button();

    /* 50 次快速操作 */
    for (int i = 0; i < 50; i++) {
        mock_gpio_value = 1;
        tick_n(DEBOUNCE_TICKS + 3);
        mock_gpio_value = 0;
        tick_n(DEBOUNCE_TICKS + 3);
    }

    /* 等超时 */
    tick_n(SHORT_TICKS + 20);

    /* 不崩溃 + 有检测到按下 = 通过 */
    ASSERT(count_event(BTN_PRESS_DOWN) > 0);

    teardown_button();
    return 0;
}

// ╔══════════════════════════════════════════════════════════╗
// ║  main — 运行所有测试                                     ║
// ║                                                          ║
// ║  RUN_TEST 宏自动编号、记录结果。                          ║
// ║  最后打印统计：X/Y passed                                ║
// ║  有失败则 exit code = 1（CI 会感知到）                    ║
// ╚══════════════════════════════════════════════════════════╝

int main(void)
{
    printf("MultiButton Unit Tests (v%d.%d.%d)\n",
           MULTIBUTTON_VERSION_MAJOR, MULTIBUTTON_VERSION_MINOR,
           MULTIBUTTON_VERSION_PATCH);
    printf("=====================================\n");

    RUN_TEST(test_single_click);
    RUN_TEST(test_double_click);
    RUN_TEST(test_long_press);
    RUN_TEST(test_repeat_press);
    RUN_TEST(test_debounce);
    RUN_TEST(test_repeat_to_press_transition);
    RUN_TEST(test_start_duplicate);
    RUN_TEST(test_stop_and_restart);
    RUN_TEST(test_null_handle);
    RUN_TEST(test_reset);
    RUN_TEST(test_stop_in_callback);
    RUN_TEST(test_ticks_saturation);
    RUN_TEST(test_triple_click);
    RUN_TEST(test_user_data);
    RUN_TEST(test_debounce_boundary);
    RUN_TEST(test_rapid_press_release);

    printf("\nResults: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED", tests_failed);
    }
    printf("\n");

    return tests_failed > 0 ? 1 : 0;
}
