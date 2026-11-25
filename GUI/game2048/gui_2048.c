/**
 ****************************************************************************************************
 * @file        gui_2048.c
 * @brief       带滑动动画的 2048
 *
 * 关键点：每个数字块都是独立的 lv_obj，移动时用 lv_anim 把它的 x/y 动画到目标格子，
 *         所以看起来是"滑过去"而不是"瞬移"。
 ****************************************************************************************************
 */

#include "gui_2048.h"
#include "app_shell.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdint.h>

/*********************
 *  布局参数
 *********************/
#define CELL_SIZE       36
#define CELL_GAP        4
#define BOARD_PAD       4
#define BOARD_SIZE      (4 * CELL_SIZE + 3 * CELL_GAP + 2 * BOARD_PAD)   /* 164 */
#define BOARD_X0        6
#define BOARD_Y0        44

#define ANIM_MS         110

/*********************
 *  颜色
 *********************/
static const uint32_t g_tile_bg[] =
{
    0xEEE4DA, 0xEDE0C8, 0xF2B179, 0xF59563, 0xF67C5F,
    0xF65E3B, 0xEDCF72, 0xEDCC61, 0xEDC850, 0xEDC53F, 0xEDC22E
};

/*********************
 *  状态
 *********************/
static uint32_t  g_board[4][4];          /* 当前牌面（0=空） */
static uint32_t  g_next[4][4];           /* 移动后的牌面 */
static lv_obj_t *g_tiles[4][4];          /* 每个格子的块对象 */
static lv_obj_t *g_board_obj;            /* 棋盘容器 */
static lv_obj_t *g_lbl_score;
static lv_obj_t *g_lbl_best;
static lv_obj_t *g_over_label;     /* GAME OVER 提示标签（重开时要删掉） */
static uint32_t  g_score;
static uint32_t  g_best;

static uint16_t  g_pending;              /* 还没播完的动画数 */
static uint8_t   g_busy;

/* 一次移动里，某个块的走向 */
typedef struct
{
    uint8_t  fx, fy, tx, ty;
} tile_move_t;

static tile_move_t g_moves[16];
static uint8_t     g_move_cnt;

/*********************
 *  小工具
 *********************/
static uint8_t value_to_idx(uint32_t v)
{
    switch (v)
    {
        case 2:    return 0;
        case 4:    return 1;
        case 8:    return 2;
        case 16:   return 3;
        case 32:   return 4;
        case 64:   return 5;
        case 128:  return 6;
        case 256:  return 7;
        case 512:  return 8;
        case 1024: return 9;
        case 2048: return 10;
        default:   return 11;
    }
}

static int32_t cell_px_x(uint8_t cx)
{
    return BOARD_PAD + cx * (CELL_SIZE + CELL_GAP);
}

static int32_t cell_px_y(uint8_t cy)
{
    return BOARD_PAD + cy * (CELL_SIZE + CELL_GAP);
}

static void opa_anim_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

static void update_score_label(void)
{
    char buf[16];

    if (g_score > g_best)
    {
        g_best = g_score;
    }
    sprintf(buf, "%lu", (unsigned long)g_score);
    lv_label_set_text(g_lbl_score, buf);
    sprintf(buf, "%lu", (unsigned long)g_best);
    lv_label_set_text(g_lbl_best, buf);
}

/* 创建/重画一个格子里的块 */
static void tile_draw(uint8_t cx, uint8_t cy, uint32_t value)
{
    lv_obj_t *t;
    lv_obj_t *lb;
    char      buf[16];
    uint8_t   idx = value_to_idx(value);

    t = lv_obj_create(g_board_obj);
    lv_obj_set_pos(t, cell_px_x(cx), cell_px_y(cy));
    lv_obj_set_size(t, CELL_SIZE, CELL_SIZE);
    lv_obj_set_style_radius(t, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(t, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(t, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(t, lv_color_hex(idx < 11 ? g_tile_bg[idx] : 0x3C3A32), LV_PART_MAIN);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);

    lb = lv_label_create(t);
    sprintf(buf, "%lu", (unsigned long)value);
    lv_label_set_text(lb, buf);
    lv_obj_set_style_text_color(lb, lv_color_hex(idx <= 1 ? 0x776E65 : 0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(lb);

    g_tiles[cx][cy] = t;
}

/* 清空所有块对象 */
static void tiles_clear(void)
{
    uint8_t x, y;

    for (x = 0; x < 4; x++)
    {
        for (y = 0; y < 4; y++)
        {
            if (g_tiles[x][y] != NULL)
            {
                lv_anim_delete(g_tiles[x][y], NULL);   /* 先掐掉它身上的动画（含淡入），否则动画会访问已释放内存 → 死机 */
                lv_obj_delete(g_tiles[x][y]);
                g_tiles[x][y] = NULL;
            }
        }
    }
}

/* 按 g_board 重建整盘 */
static void board_redraw(void)
{
    uint8_t x, y;

    tiles_clear();
    for (x = 0; x < 4; x++)
    {
        for (y = 0; y < 4; y++)
        {
            if (g_board[x][y] != 0)
            {
                tile_draw(x, y, g_board[x][y]);
            }
        }
    }
}

/* 找一个空格，随机放 2 或 4（带淡入动画） */
static void spawn_random(void)
{
    uint8_t  empty[16][2];
    uint8_t  n = 0, x, y;
    uint32_t pick, val;
    lv_obj_t *t;

    for (x = 0; x < 4; x++)
    {
        for (y = 0; y < 4; y++)
        {
            if (g_board[x][y] == 0)
            {
                empty[n][0] = x;
                empty[n][1] = y;
                n++;
            }
        }
    }
    if (n == 0)
    {
        return;
    }

    pick = lv_rand(0, (uint32_t)n - 1U);
    val  = (lv_rand(0, 9) == 0) ? 4U : 2U;

    g_board[empty[pick][0]][empty[pick][1]] = val;
    tile_draw(empty[pick][0], empty[pick][1], val);

    /* 淡入 */
    t = g_tiles[empty[pick][0]][empty[pick][1]];
    if (t != NULL)
    {
        lv_obj_set_style_opa(t, LV_OPA_0, LV_PART_MAIN);
        {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, t);
            lv_anim_set_exec_cb(&a, opa_anim_cb);
            lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
            lv_anim_set_duration(&a, 120);
            lv_anim_start(&a);
        }
    }
}

static uint8_t board_has_move(void)
{
    uint8_t x, y;

    for (x = 0; x < 4; x++)
    {
        for (y = 0; y < 4; y++)
        {
            if (g_board[x][y] == 0)
            {
                return 1;
            }
            if ((x < 3) && (g_board[x][y] == g_board[x + 1][y]))
            {
                return 1;
            }
            if ((y < 3) && (g_board[x][y] == g_board[x][y + 1]))
            {
                return 1;
            }
        }
    }
    return 0;
}

/* 动画播完 → 结算：换成新牌面 */
static void settle_async(void *p)
{
    uint8_t x, y;

    (void)p;

    for (x = 0; x < 4; x++)
    {
        for (y = 0; y < 4; y++)
        {
            g_board[x][y] = g_next[x][y];
        }
    }
    board_redraw();
    spawn_random();
    update_score_label();
    g_busy = 0;

    if (!board_has_move() && (g_over_label == NULL) && (g_board_obj != NULL))
    {
        g_over_label = lv_label_create(lv_obj_get_screen(g_board_obj));
        lv_label_set_text(g_over_label, "GAME OVER");
        lv_obj_set_style_text_color(g_over_label, lv_color_hex(0xEE0000), LV_PART_MAIN);
        lv_obj_align(g_over_label, LV_ALIGN_CENTER, 0, 0);
    }
}
static void anim_ready_cb(lv_anim_t *a)
{
    (void)a;
    if (g_pending > 0)
    {
        g_pending--;
    }
    if (g_pending == 0)
    {
        /* 不在动画回调里删对象（危险），交给下一次 lv_timer_handler 处理 */
        lv_async_call(settle_async, NULL);
    }
}

static void start_anim(lv_obj_t *t, int32_t to_x, int32_t to_y)
{
    lv_anim_t a;

    lv_anim_init(&a);
    lv_anim_set_var(&a, t);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, lv_obj_get_x(t), to_x);
    lv_anim_set_duration(&a, ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, t);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, lv_obj_get_y(t), to_y);
    lv_anim_set_duration(&a, ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a, anim_ready_cb);
    lv_anim_start(&a);

    g_pending++;
}

/* 把一条线上的块按方向压缩 + 合并，生成 g_next 和 g_moves */
static void layout_line(uint8_t line, uint8_t dir)
{
    uint8_t  i, t = 0;
    uint8_t  cnt = 0;
    uint8_t  px[4], py[4];
    uint32_t pv[4];

    /* 按"从目标侧数起"的顺序收集 */
    for (i = 0; i < 4; i++)
    {
        uint8_t x, y;
        uint8_t k = (dir == G2048_LEFT || dir == G2048_UP) ? i : (3 - i);

        if (dir == G2048_LEFT || dir == G2048_RIGHT)
        {
            x = k;
            y = line;
        }
        else
        {
            x = line;
            y = k;
        }

        if (g_board[x][y] != 0)
        {
            px[cnt] = x;
            py[cnt] = y;
            pv[cnt] = g_board[x][y];
            cnt++;
        }
    }

    i = 0;
    while (i < cnt)
    {
        uint8_t tx, ty;

        if ((dir == G2048_LEFT || dir == G2048_RIGHT))
        {
            tx = (dir == G2048_LEFT) ? t : (uint8_t)(3 - t);
            ty = line;
        }
        else
        {
            tx = line;
            ty = (dir == G2048_UP) ? t : (uint8_t)(3 - t);
        }

        if (((i + 1) < cnt) && (pv[i] == pv[i + 1]))
        {
            /* 合并：两个都滑到同一格，值翻倍 */
            g_next[tx][ty] = pv[i] * 2U;
            g_score += pv[i] * 2U;

            g_moves[g_move_cnt].fx = px[i];
            g_moves[g_move_cnt].fy = py[i];
            g_moves[g_move_cnt].tx = tx;
            g_moves[g_move_cnt].ty = ty;
            g_move_cnt++;

            g_moves[g_move_cnt].fx = px[i + 1];
            g_moves[g_move_cnt].fy = py[i + 1];
            g_moves[g_move_cnt].tx = tx;
            g_moves[g_move_cnt].ty = ty;
            g_move_cnt++;

            i += 2;
        }
        else
        {
            g_next[tx][ty] = pv[i];

            g_moves[g_move_cnt].fx = px[i];
            g_moves[g_move_cnt].fy = py[i];
            g_moves[g_move_cnt].tx = tx;
            g_moves[g_move_cnt].ty = ty;
            g_move_cnt++;

            i++;
        }
        t++;
    }
}

void game2048_move(g2048_dir_t dir)
{
    uint8_t x, y, i;
    uint8_t changed = 0;

    if (g_busy)
    {
        return;                              /* 动画中忽略新输入，避免错乱 */
    }

    /* 清空目标 */
    for (x = 0; x < 4; x++)
    {
        for (y = 0; y < 4; y++)
        {
            g_next[x][y] = 0;
        }
    }
    g_move_cnt = 0;

    for (i = 0; i < 4; i++)
    {
        layout_line(i, (uint8_t)dir);
    }

    /* 判断是否有变化 */
    for (x = 0; x < 4; x++)
    {
        for (y = 0; y < 4; y++)
        {
            if (g_next[x][y] != g_board[x][y])
            {
                changed = 1;
            }
        }
    }
    if (!changed)
    {
        return;
    }

    /* 逐个块启动滑动动画 */
    g_pending = 0;
    g_busy    = 1;
    for (i = 0; i < g_move_cnt; i++)
    {
        lv_obj_t *t = g_tiles[g_moves[i].fx][g_moves[i].fy];

        if (t == NULL)
        {
            continue;
        }
        if ((g_moves[i].fx == g_moves[i].tx) && (g_moves[i].fy == g_moves[i].ty))
        {
            continue;                         /* 原地不动，不用动画 */
        }
        start_anim(t, cell_px_x(g_moves[i].tx), cell_px_y(g_moves[i].ty));
    }

    if (g_pending == 0)
    {
        anim_ready_cb(NULL);                  /* 没有动画也要结算 */
    }
}

void game2048_new(void)
{
    uint8_t x, y;

    /* 先清掉上一局的 GAME OVER 提示 */
    if (g_over_label != NULL)
    {
        lv_obj_delete(g_over_label);
        g_over_label = NULL;
    }

    tiles_clear();
    for (x = 0; x < 4; x++)
    {
        for (y = 0; y < 4; y++)
        {
            g_board[x][y] = 0;
        }
    }
    g_score = 0;
    g_busy  = 0;
    g_pending = 0;

    spawn_random();
    spawn_random();
    update_score_label();
}

/*********************
 *  事件
 *********************/
static void btn_dir_cb(lv_event_t *e)
{
    g2048_dir_t dir = (g2048_dir_t)(uint32_t)(uintptr_t)lv_event_get_user_data(e);

    game2048_move(dir);
}

static void btn_new_cb(lv_event_t *e)
{
    (void)e;
    game2048_new();
}

static void scr_gesture_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_indev(e);

    if (indev == NULL)
    {
        return;
    }
    switch (lv_indev_get_gesture_dir(indev))
    {
        case LV_DIR_LEFT:  game2048_move(G2048_LEFT);  break;
        case LV_DIR_RIGHT: game2048_move(G2048_RIGHT); break;
        case LV_DIR_TOP:   game2048_move(G2048_UP);    break;
        case LV_DIR_BOTTOM:game2048_move(G2048_DOWN);  break;
        default: break;
    }
}

/* 造一个方向按钮 */
static lv_obj_t *make_btn(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                          const char *txt, lv_event_cb_t cb, void *user)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_t *l;

    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x9C8678), LV_PART_MAIN);
    lv_obj_add_event_cb(b, cb, LV_EVENT_PRESSED, user);   /* 电阻屏抖动，用 PRESSED 更可靠 */

    l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(l);

    return b;
}

/* 造一个分数卡片 */
static lv_obj_t *make_card(lv_obj_t *parent, int32_t x, int32_t w, const char *title, lv_obj_t **out_val)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_t *t;
    lv_obj_t *v;

    lv_obj_set_pos(c, x, 4);
    lv_obj_set_size(c, w, 34);
    lv_obj_set_style_radius(c, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(c, lv_color_hex(0xBDAC97), LV_PART_MAIN);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);

    t = lv_label_create(c);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(0x776E65), LV_PART_MAIN);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 1);

    v = lv_label_create(c);
    lv_label_set_text(v, "0");
    lv_obj_set_style_text_color(v, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(v, LV_ALIGN_BOTTOM_MID, 0, -1);

    *out_val = v;
    return c;
}

lv_obj_t *app_2048_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *title;

    lv_rand_set_seed(lv_tick_get());

    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFEDCF), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    title = lv_label_create(scr);
    lv_label_set_text(title, "2048");
    lv_obj_set_style_text_color(title, lv_color_hex(0x776E65), LV_PART_MAIN);
    lv_obj_set_pos(title, 8, 8);

    /* 分数卡 */
    make_card(scr, 52,  62, "SCORE", &g_lbl_score);
    make_card(scr, 118, 62, "BEST",  &g_lbl_best);

    /* 棋盘 */
    g_board_obj = lv_obj_create(scr);
    lv_obj_set_pos(g_board_obj, BOARD_X0, BOARD_Y0);
    lv_obj_set_size(g_board_obj, BOARD_SIZE, BOARD_SIZE);
    lv_obj_set_style_radius(g_board_obj, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_board_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_board_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_board_obj, lv_color_hex(0x9C8678), LV_PART_MAIN);
    lv_obj_clear_flag(g_board_obj, LV_OBJ_FLAG_SCROLLABLE);

    /* 底格（4x4 淡色格子） */
    {
        uint8_t x, y;
        for (x = 0; x < 4; x++)
        {
            for (y = 0; y < 4; y++)
            {
                lv_obj_t *c = lv_obj_create(g_board_obj);
                lv_obj_set_pos(c, cell_px_x(x), cell_px_y(y));
                lv_obj_set_size(c, CELL_SIZE, CELL_SIZE);
                lv_obj_set_style_radius(c, 4, LV_PART_MAIN);
                lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
                lv_obj_set_style_bg_color(c, lv_color_hex(0xBDAC97), LV_PART_MAIN);
                lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
            }
        }
    }

    /* 右侧方向键 */
    make_btn(scr, 226, 56,  44, 34, LV_SYMBOL_UP,    btn_dir_cb, (void *)(uintptr_t)G2048_UP);
    make_btn(scr, 180, 96,  44, 34, LV_SYMBOL_LEFT,  btn_dir_cb, (void *)(uintptr_t)G2048_LEFT);
    make_btn(scr, 272, 96,  44, 34, LV_SYMBOL_RIGHT, btn_dir_cb, (void *)(uintptr_t)G2048_RIGHT);
    make_btn(scr, 226, 136, 44, 34, LV_SYMBOL_DOWN,  btn_dir_cb, (void *)(uintptr_t)G2048_DOWN);

    /* New Game */
    make_btn(scr, 180, 186, 136, 34, "NEW GAME", btn_new_cb, NULL);

    /* 滑动手势 */
    lv_obj_add_event_cb(scr, scr_gesture_cb, LV_EVENT_GESTURE, NULL);

    g_score = 0;
    g_best  = 0;
    game2048_new();

    app_make_back_btn(scr);          /* 右上角返回主菜单 */

    return scr;
}
