/*
 * CherryTimer - Self-Test Suite Implementation
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "timer_test.h"
#include "cherry_timer.h"
#include "cherry_kal.h"

/* --- 1. Standard Timer Test --- */
static volatile bool s_std_timer_fired = false;
static void std_timer_callback(struct timer_list *t)
{
    (void)t;
    s_std_timer_fired = true;
}

static bool test_standard_timer(void)
{
    s_std_timer_fired = false;
    static struct timer_list s_std_t;
    timer_setup(&s_std_t, std_timer_callback, 0);

    mod_timer(&s_std_t, 5); /* 5ms */

    for (int i = 0; i < 20; i++) {
        cherry_timer_poll();
        if (s_std_timer_fired) break;
        msleep(1);
    }

    return s_std_timer_fired;
}

/* --- 2. High-Resolution Timer Test --- */
static volatile bool s_hrtimer_fired = false;
static enum hrtimer_restart hrtimer_callback(struct hrtimer *h)
{
    (void)h;
    s_hrtimer_fired = true;
    return HRTIMER_NORESTART;
}

static bool test_hrtimer(void)
{
    s_hrtimer_fired = false;
    static struct hrtimer s_hr_h;
    hrtimer_init(&s_hr_h, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    s_hr_h.function = hrtimer_callback;

    hrtimer_start(&s_hr_h, ms_to_ktime(2), HRTIMER_MODE_REL); /* 2ms */

    for (int i = 0; i < 20; i++) {
        cherry_timer_poll();
        if (s_hrtimer_fired) break;
        msleep(1);
    }

    return s_hrtimer_fired;
}

/* --- 3. Multi-Core Concurrent Timer Test --- */
static volatile bool s_mc_fired[2] = { false, false };

static void mc_timer_fn(struct timer_list *t)
{
    int core = (int)(uintptr_t)t->data;
    if (core >= 0 && core < 2) {
        s_mc_fired[core] = true;
    }
}

static bool test_multicore_timers(void)
{
    s_mc_fired[0] = false;
    s_mc_fired[1] = false;

    static struct timer_list s_mc_t0, s_mc_t1;
    timer_setup(&s_mc_t0, mc_timer_fn, TIMER_PINNED);
    s_mc_t0.data = (void *)(uintptr_t)0;
    s_mc_t0.cpu = 0;

    timer_setup(&s_mc_t1, mc_timer_fn, TIMER_PINNED);
    s_mc_t1.data = (void *)(uintptr_t)1;
    s_mc_t1.cpu = 1;

    mod_timer(&s_mc_t0, 3); /* 3ms on CPU 0 */
    mod_timer(&s_mc_t1, 3); /* 3ms on CPU 1 */

    for (int i = 0; i < 20; i++) {
        cherry_timer_poll();
        if (s_mc_fired[0] && s_mc_fired[1]) break;
        msleep(1);
    }

    return (s_mc_fired[0] && s_mc_fired[1]);
}

/* --- 4. Periodic Restart Test --- */
static volatile int s_restart_count = 0;
static enum hrtimer_restart restart_callback(struct hrtimer *h)
{
    s_restart_count++;
    if (s_restart_count < 3) {
        h->expires += ms_to_ktime(2);
        return HRTIMER_RESTART;
    }
    return HRTIMER_NORESTART;
}

static bool test_periodic_restart(void)
{
    s_restart_count = 0;
    static struct hrtimer s_pr_h;
    hrtimer_init(&s_pr_h, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    s_pr_h.function = restart_callback;

    hrtimer_start(&s_pr_h, ms_to_ktime(2), HRTIMER_MODE_REL);

    for (int i = 0; i < 30; i++) {
        cherry_timer_poll();
        if (s_restart_count >= 3) break;
        msleep(1);
    }

    return (s_restart_count >= 3);
}

/* --- 5. Timer Cancel Test --- */
static volatile bool s_cancel_fired = false;
static void cancel_callback(struct timer_list *t)
{
    (void)t;
    s_cancel_fired = true;
}

static bool test_cancel(void)
{
    s_cancel_fired = false;
    static struct timer_list s_cancel_t;
    timer_setup(&s_cancel_t, cancel_callback, 0);

    mod_timer(&s_cancel_t, 50); /* 50ms */
    int ret = del_timer(&s_cancel_t); /* Cancel immediately */

    for (int i = 0; i < 10; i++) {
        cherry_timer_poll();
        msleep(1);
    }

    return (ret == 1 && !s_cancel_fired);
}

int cherry_timer_run_tests(void)
{
    pr_info("==> starting cherrytimer multi-core acceptance test suite...");

    bool p1 = test_standard_timer();
    bool p2 = test_hrtimer();
    bool p3 = test_multicore_timers();
    bool p4 = test_periodic_restart();
    bool p5 = test_cancel();

    bool all_ok = (p1 && p2 && p3 && p4 && p5);
    pr_info("[timer-verify] standard=%s hrtimer=%s multi_core=%s restart=%s cancel=%s all=%s",
            p1 ? "pass" : "fail",
            p2 ? "pass" : "fail",
            p3 ? "pass" : "fail",
            p4 ? "pass" : "fail",
            p5 ? "pass" : "fail",
            all_ok ? "pass" : "fail");

    return all_ok ? 0 : -1;
}
