/**
 * render.c - 渲染系统：屏幕绘制、ASCII美术、HUD
 * Soul Survivor: 地下生存
 * 画风参考: UNDERTALE
 *
 * 性能优化：全帧缓冲 → 单次 fwrite，消除逐字符 printf 的系统调用开销
 */
#include "game.h"

/* 帧缓冲：竞技场格子 */
static char frame_buffer[ARENA_H][ARENA_W + 1];

/* 渲染缓冲：整屏内容拼好后一次输出 */
#define RENDER_BUF_SIZE 24576
static char render_buf[RENDER_BUF_SIZE];

/* ANSI 风格字符缓存：避免 arena 循环内反复 snprintf */
static char style_buf[256][32];  /* 每种字符的 ANSI 包装 */
static int  style_initialized = 0;

/* buf_append_str: 追加字符串，返回新位置 */
static char* buf_append_str(char *dst, const char *end, const char *src) {
    while (dst < end && *src) *dst++ = *src++;
    return dst;
}

/* 外部声明 */
extern Weapon weapon_library[MAX_WEAPONS];

/* ============ 风格字符初始化 ============ */

static void style_init(void) {
    if (style_initialized) return;
    style_initialized = 1;

    /* 玩家 ♥ */
    sprintf(style_buf['@'], RED "\xe2\x99\xa5" RESET);
    /* 玩家攻击闪烁：亮白粗体 ♥ */
    sprintf(style_buf['#'], WHITE BOLD "\xe2\x99\xa5" RESET);
    /* 边框 */
    sprintf(style_buf['+'], WHITE "+" RESET);
    sprintf(style_buf['-'], WHITE "-" RESET);
    sprintf(style_buf['|'], WHITE BOLD "|" RESET);            /* 粗体竖线（远程弹幕） */
    /* 攻击效果：弹幕 */
    sprintf(style_buf['~'], YELLOW BOLD "~" RESET);           /* 闪电弹幕 */
    /* 攻击效果：命中火花（多色 BOLD 粒子） */
    sprintf(style_buf['*'], YELLOW BOLD "*" RESET);           /* 爆发/命中 */
    sprintf(style_buf['.'], CYAN BOLD "." RESET);             /* 青色火花 */
    sprintf(style_buf['x'], MAGENTA BOLD "x" RESET);          /* 紫色火花 */
    sprintf(style_buf['o'], GREEN BOLD "o" RESET);            /* 绿色火花 */
    /* 攻击效果：劈砍动画（BOLD 粗体更醒目） */
    sprintf(style_buf['/'], CYAN BOLD "/" RESET);             /* 劈砍帧1 */
    sprintf(style_buf['='], CYAN BOLD "=" RESET);             /* 劈砍帧2 */
    sprintf(style_buf['\\'], CYAN BOLD "\\" RESET);           /* 劈砍帧3 */
    sprintf(style_buf['!'], CYAN BOLD "!" RESET);             /* 劈砍帧4 */
    /* 拖尾粒子：暗色中间点 */
    sprintf(style_buf['`'], DIM "\xc2\xb7" RESET);
    /* 敌人（Latin-1 窄字符，1 终端列宽，比 ASCII 更美观） */
    sprintf(style_buf['m'], GREEN "\xc2\xb5" RESET);          /* Froggit: µ 生物感 */
    sprintf(style_buf[','], CYAN "\xc2\xb7" RESET);           /* Whimsun: · 轻盈 */
    sprintf(style_buf['d'], BLUE "\xc2\xb0" RESET);           /* Moldsmal: ° 圆形 */
    sprintf(style_buf['<'], MAGENTA "\xc2\xab" RESET);        /* Loox: « 兔耳 */
    sprintf(style_buf['p'], GREEN "\xc2\xb6" RESET);          /* Vegetoid: ¶ 草叶 */
    sprintf(style_buf['c'], YELLOW "\xc2\xa4" RESET);         /* Migosp: ¤ 奇异 */
    sprintf(style_buf['X'], RED BOLD "\xc3\x97" RESET);       /* Greater Dog: × 威压 */
}

/* ============ 初始化 ============ */

void render_init(void) {
    clear_screen();
    printf(HIDE_CUR);
    style_init();
}

void clear_screen(void) {
    printf(CLEAR HOME);
}

/* ============ 帧缓冲操作 ============ */

static void fb_clear(void) {
    int x, y;
    for (y = 0; y < ARENA_H; y++) {
        for (x = 0; x < ARENA_W; x++) {
            frame_buffer[y][x] = ' ';
        }
        frame_buffer[y][ARENA_W] = '\0';
    }
}

static void fb_set(int x, int y, char ch) {
    if (x >= 0 && x < ARENA_W && y >= 0 && y < ARENA_H) {
        frame_buffer[y][x] = ch;
    }
}

/* ============ 绘图辅助 ============ */

void draw_box(int x, int y, int w, int h, const char *color) {
    int i, j;
    printf("%s\033[%d;%dH+", color, y, x);
    for (i = 0; i < w - 2; i++) printf("-");
    printf("+");
    for (j = 1; j < h - 1; j++) {
        printf("\033[%d;%dH|", y + j, x);
        printf("\033[%d;%dH|", y + j, x + w - 1);
    }
    printf("\033[%d;%dH+", y + h - 1, x);
    for (i = 0; i < w - 2; i++) printf("-");
    printf("+");
    printf(RESET);
    fflush(stdout);
}

void draw_text(int x, int y, const char *text, const char *color) {
    printf("%s\033[%d;%dH%s" RESET, color, y, x, text);
    fflush(stdout);
}

/* ============ 终端尺寸查询 ============ */

static void get_term_size(int *w, int *h) {
    *w = 80; *h = 24;  /* 默认值 */
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        *w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
#endif
}

/* ============ 标题画面 ============ */

void render_title_screen(void) {
    clear_screen();

    const char *title[] = {
        RED"                   ♥  SOUL SURVIVOR  ♥" RESET,
        YELLOW"                       地下生存" RESET,
        "",
        CYAN"          +--------------------------------------------+",
        "          |                                            |",
        "          |     吸血鬼幸存者  ×  UNDERTALE           |",
        "          |              命令行小游戏                 |",
        "          |                                            |",
        "          +--------------------------------------------+" RESET,
        "",
        DIM"          * 在这地下世界，你的灵魂仍闪耀着光芒。" RESET,
        DIM"          * 保持决心，存活下去..." RESET,
        "",
        GREEN"                    [1]  开始游戏" RESET,
        YELLOW"                    [2]  查看记录" RESET,
        RED"                    [3]  退出" RESET,
        "",
        DIM"          =============================================" RESET,
        DIM"          操控: WASD 移动 | 空格 闪避 | P 暂停 | Q 返回" RESET,
    };

    int num_lines = sizeof(title) / sizeof(title[0]);

    /* 获取终端尺寸并居中 */
    int term_w, term_h;
    get_term_size(&term_w, &term_h);

    /* ASCII 美术大字：只在终端足够高时显示（需要额外7行） */
    int show_ascii_art = (term_h >= num_lines + 7);

    int content_h = num_lines + (show_ascii_art ? 7 : 0);
    int content_w = 52;
    int pad_y = (term_h > content_h) ? (term_h - content_h) / 2 : 0;
    int pad_x = (term_w > content_w) ? (term_w - content_w) / 2 : 0;

    int i;
    for (i = 0; i < num_lines; i++) {
        printf("\033[%d;%dH%s", pad_y + i, pad_x, title[i]);
    }

    if (show_ascii_art) {
        int ay = pad_y + num_lines;
        printf("\033[%d;%dH", ay, pad_x);
        printf(WHITE"          ██╗   ██╗███╗   ██╗██████╗ ███████╗██████╗ " RESET"\n");
        printf("\033[%d;%dH", ay + 1, pad_x);
        printf(WHITE"          ██║   ██║████╗  ██║██╔══██╗██╔════╝██╔══██╗" RESET"\n");
        printf("\033[%d;%dH", ay + 2, pad_x);
        printf(WHITE"          ██║   ██║██╔██╗ ██║██║  ██║█████╗  ██████╔╝" RESET"\n");
        printf("\033[%d;%dH", ay + 3, pad_x);
        printf(WHITE"          ██║   ██║██║╚██╗██║██║  ██║██╔══╝  ██╔══██╗" RESET"\n");
        printf("\033[%d;%dH", ay + 4, pad_x);
        printf(WHITE"          ╚██████╔╝██║ ╚████║██████╔╝███████╗██║  ██║" RESET"\n");
        printf("\033[%d;%dH", ay + 5, pad_x);
        printf(WHITE"           ╚═════╝ ╚═╝  ╚═══╝╚═════╝ ╚══════╝╚═╝  ╚═╝" RESET"\n");
    }

    fflush(stdout);
}

/* ============ 游戏画面渲染（优化版：单次 fwrite） ============ */

/**
 * 将敌人 Unicode 符号（模板中的 ♟ ♤ ◉ ♢ ♧ ♠ ♛）以正确颜色写入缓冲
 * 每格只输出一个符号，不跳过格子。宽字符由终端自然处理。
 */
static void write_enemy_colored(char **buf, const char *end, Enemy *e) {
    const char *color = WHITE;
    int bold = 0;

    if (e->flash_timer > 0) {
        /* 受伤闪烁：亮白色 */
        *buf = buf_append_str(*buf, end, BOLD WHITE);
        *buf = buf_append_str(*buf, end, e->symbol);
        *buf = buf_append_str(*buf, end, RESET);
        return;
    }

    switch (e->type) {
        case ENEMY_FROGGIT:      color = GREEN;   break;
        case ENEMY_WHIMSUN:      color = CYAN;    break;
        case ENEMY_MOLDSMAL:     color = BLUE;    break;
        case ENEMY_LOOX:         color = MAGENTA; break;
        case ENEMY_VEGETOID:     color = GREEN;   break;
        case ENEMY_MIGOSP:       color = YELLOW;  break;
        case ENEMY_GREATER_DOG:  color = RED; bold = 1; break;
        default:                 color = WHITE;   break;
    }

    if (bold) *buf = buf_append_str(*buf, end, BOLD);
    *buf = buf_append_str(*buf, end, color);
    *buf = buf_append_str(*buf, end, e->symbol);
    *buf = buf_append_str(*buf, end, RESET);
}

void render_frame(Player *p, Enemy enemies[], WaveManager *wm, VisualEffect effects[], int fps) {
    int x, y, i;
    char *buf = render_buf;
    const char *end = render_buf + RENDER_BUF_SIZE - 64;

    style_init();
    fb_clear();

    /* --- 帧缓冲：绘制边框 --- */
    fb_set(0, 0, '+');
    fb_set(ARENA_W - 1, 0, '+');
    for (x = 1; x < ARENA_W - 1; x++) fb_set(x, 0, '-');
    fb_set(0, ARENA_H - 1, '+');
    fb_set(ARENA_W - 1, ARENA_H - 1, '+');
    for (x = 1; x < ARENA_W - 1; x++) fb_set(x, ARENA_H - 1, '-');
    for (y = 1; y < ARENA_H - 1; y++) {
        fb_set(0, y, '|');
        fb_set(ARENA_W - 1, y, '|');
    }

    /* --- 帧缓冲：绘制敌人（占位，实际渲染走 enemy_grid + Unicode 符号） --- */
    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->active) continue;
        fb_set(e->x, e->y, ' ');  /* 清空该格，由 enemy_grid 覆盖 */
    }

    /* --- 帧缓冲：绘制玩家（闪烁无敌/攻击高亮） --- */
    if (!(p->invincible > 0 && p->invincible % 4 < 2)) {
        fb_set(p->x, p->y, (p->attack_flash > 0) ? '#' : '@');
    }

    /* --- 帧缓冲：绘制攻击效果（覆盖在玩家和敌人之上） --- */
    for (i = 0; i < MAX_EFFECTS; i++) {
        if (effects[i].active) {
            fb_set(effects[i].x, effects[i].y, effects[i].symbol);
        }
    }

    /* ================================================================ */
    /* 计算居中偏移（所有行使用相同的左填充，保证对齐）                   */
    /* ================================================================ */
    int term_w, term_h;
    get_term_size(&term_w, &term_h);
    int content_w = 72;  /* 最宽行约68-70可见列，取72留余量 */
    int content_h = HUD_LINES + ARENA_H + 1; /* 5 + 17 + 1 = 23 */
    int pad_x = (term_w > content_w) ? (term_w - content_w) / 2 : 0;
    int pad_y = (term_h > content_h) ? (term_h - content_h) / 2 : 0;

    /* 构建左填充字符串 */
    char pad_str[32];
    int pi;
    for (pi = 0; pi < pad_x && pi < 30; pi++) pad_str[pi] = ' ';
    pad_str[pi] = '\0';

    /* ================================================================ */
    /* 构建整屏输出到 render_buf（纯内存操作，零系统调用）               */
    /* ================================================================ */

    /* 清屏 + HOME + 隐藏光标（防止上帧残留） */
    buf = buf_append_str(buf, end, CLEAR HOME HIDE_CUR);

    /* 顶部填充（垂直居中） */
    for (i = 0; i < pad_y; i++) *buf++ = '\n';

    /* ---- HUD 第1行：标题 + 波次 + 击杀 + FPS ---- */
    buf = buf_append_str(buf, end, pad_str);
    buf += snprintf(buf, end - buf,
        BOLD WHITE "  Soul Survivor" RESET
        "  |  " YELLOW "Wave %-3d" RESET
        "  |  " GREEN "Kills: %-4d" RESET
        "  |  " DIM "FPS:" RESET " %d\n",
        wm->wave, wm->total_killed, fps);

    /* ---- HUD 第2行：HP条 + EXP条 + 等级 ---- */
    buf = buf_append_str(buf, end, pad_str);
    buf = buf_append_str(buf, end, "  " RED "HP " RESET);
    {
        int filled = (p->max_hp > 0) ? (p->hp * 12) / p->max_hp : 0;
        if (filled < 0) filled = 0;
        if (filled > 12) filled = 12;
        buf = buf_append_str(buf, end, RED);
        for (i = 0; i < filled; i++)  buf = buf_append_str(buf, end, "█");
        buf = buf_append_str(buf, end, DIM);
        for (i = filled; i < 12; i++) buf = buf_append_str(buf, end, "░");
    }
    buf += snprintf(buf, end - buf, RESET " %d/%d", p->hp, p->max_hp);
    buf = buf_append_str(buf, end, "  |  " CYAN "EXP " RESET);
    {
        int filled = (p->exp_to_next > 0) ? (p->exp * 12) / p->exp_to_next : 0;
        if (filled < 0) filled = 0;
        if (filled > 12) filled = 12;
        buf = buf_append_str(buf, end, CYAN);
        for (i = 0; i < filled; i++)  buf = buf_append_str(buf, end, "█");
        buf = buf_append_str(buf, end, DIM);
        for (i = filled; i < 12; i++) buf = buf_append_str(buf, end, "░");
    }
    buf += snprintf(buf, end - buf, RESET " Lv.%d\n", p->level);

    /* ---- HUD 第3行：属性 + 武器 + 闪避 ---- */
    buf = buf_append_str(buf, end, pad_str);
    buf += snprintf(buf, end - buf,
        "  ATK:%-2d DEF:%-2d SPD:%-2d",
        p->atk, p->def, p->spd);
    if (p->determination > 0) {
        buf += snprintf(buf, end - buf, "  " MAGENTA "DT:%d" RESET, p->determination);
    }
    buf = buf_append_str(buf, end, "  |  " DIM "WPN:" RESET);
    for (i = 0; i < p->weapon_count; i++) {
        int wid = p->weapons[i];
        if (wid >= 0 && wid < WEAPON_COUNT) {
            if (p->weapon_cooldowns[i] > 0) {
                buf += snprintf(buf, end - buf, "[" DIM "%s" RESET "]", weapon_library[wid].name);
            } else {
                buf += snprintf(buf, end - buf, "[" GREEN "%s" RESET "]", weapon_library[wid].name);
            }
        }
    }
    for (; i < MAX_PLAYER_WEAPONS; i++) {
        buf = buf_append_str(buf, end, "[---]");
    }
    if (p->special_cooldown > 0) {
        buf += snprintf(buf, end - buf, "  " BLUE "闪避:%.2fs" RESET, p->special_cooldown / 60.0);
    } else {
        buf = buf_append_str(buf, end, "  " GREEN "闪避就绪" RESET);
    }
    *buf++ = '\n';

    /* ---- HUD 第4行：敌人信息 ---- */
    buf = buf_append_str(buf, end, pad_str);
    buf += snprintf(buf, end - buf,
        "  " DIM "敌人:" RESET " %-2d存活 | 待生成:%-2d | 剩余:%-2d\n",
        wm->enemies_alive, wm->spawn_queue,
        wm->spawn_queue + wm->enemies_alive);

    /* ---- HUD 第5行：分隔线 ---- */
    buf = buf_append_str(buf, end, pad_str);
    buf = buf_append_str(buf, end, "  " DIM "-----------------------------------------------" RESET "\n");

    /* ======== 竞技场（固定宽度，敌人使用单列 ASCII 符号）======== */
    {
        /* 构建敌人位置索引（O(1) 查找每格是否有敌人） */
        int enemy_grid[ARENA_H][ARENA_W];
        memset(enemy_grid, -1, sizeof(enemy_grid));
        for (i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                int ey = enemies[i].y, ex = enemies[i].x;
                if (ey >= 0 && ey < ARENA_H && ex >= 0 && ex < ARENA_W) {
                    enemy_grid[ey][ex] = i;
                }
            }
        }

        for (y = 0; y < ARENA_H; y++) {
            buf = buf_append_str(buf, end, pad_str);
            buf = buf_append_str(buf, end, "  ");  /* 与HUD缩进对齐 */
            for (x = 0; x < ARENA_W; x++) {
                if (enemy_grid[y][x] >= 0) {
                    /* 敌人 Unicode 符号（♟ ♤ 等），每格 1 个 */
                    write_enemy_colored(&buf, end, &enemies[enemy_grid[y][x]]);
                } else {
                    char ch = frame_buffer[y][x];
                    if (style_buf[(unsigned char)ch][0] != '\0') {
                        buf = buf_append_str(buf, end, style_buf[(unsigned char)ch]);
                    } else {
                        *buf++ = ch;
                    }
                }
            }
            *buf++ = '\n';
        }
    }

    /* ---- 底部操作提示 ---- */
    buf = buf_append_str(buf, end, pad_str);
    buf = buf_append_str(buf, end, DIM "  WASD:移动 | 空格:闪避 | P:暂停 | Q:返回菜单" RESET "\n  ");

    /* 清除下方残留 */
    buf = buf_append_str(buf, end, "\033[0J");

    /* ======== 单次输出（唯一的系统调用！）======== */
    fwrite(render_buf, 1, buf - render_buf, stdout);
    fflush(stdout);
}

/* ============ 单独绘制血条/经验条（外部可用） ============ */

void draw_hp_bar(int x, int y, int current, int max, int width) {
    int filled = (max > 0) ? (current * width) / max : 0;
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    printf("\033[%d;%dH" RED, y, x);
    int i;
    for (i = 0; i < filled; i++) printf("█");
    printf(DIM);
    for (i = filled; i < width; i++) printf("░");
    printf(RESET);
}

void draw_exp_bar(int x, int y, int current, int max, int width) {
    int filled = (max > 0) ? (current * width) / max : 0;
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    printf("\033[%d;%dH" CYAN, y, x);
    int i;
    for (i = 0; i < filled; i++) printf("█");
    printf(DIM);
    for (i = filled; i < width; i++) printf("░");
    printf(RESET);
}

/* ============ 升级菜单 ============ */

void render_levelup_menu(Player *p, UpgradeOption opts[], int count) {
    (void)p;
    int i;

    int term_w, term_h;
    get_term_size(&term_w, &term_h);
    int content_w = 72; /* 与主界面统一宽度 */
    int content_h = HUD_LINES + ARENA_H + 1;
    int pad_x = (term_w > content_w) ? (term_w - content_w) / 2 : 0;
    int pad_y = (term_h > content_h) ? (term_h - content_h) / 2 : 0;

    printf(HOME);
    printf("\033[%d;%dH", pad_y + HUD_LINES + ARENA_H + 1, pad_x);
    printf(YELLOW "  +----------------------------------------------------------+" RESET "\n");
    printf("\033[%d;%dH", pad_y + HUD_LINES + ARENA_H + 2, pad_x);
    printf(YELLOW "  |" RESET CYAN BOLD "                        LEVEL UP!                         " RESET YELLOW "|" RESET "\n");
    printf("\033[%d;%dH", pad_y + HUD_LINES + ARENA_H + 3, pad_x);
    printf(YELLOW "  +----------------------------------------------------------+" RESET "\n");
    printf("\033[%d;%dH", pad_y + HUD_LINES + ARENA_H + 4, pad_x);
    printf(YELLOW "  |" RESET "  选择一项升级:                                            " YELLOW "|" RESET "\n");

    for (i = 0; i < count; i++) {
        const char *icon;
        const char *opt_color = WHITE;

        switch (opts[i].type) {
            case UPGRADE_ATK:           icon = RED "ATK" RESET;   opt_color = RED;   break;
            case UPGRADE_SPD:           icon = GREEN "SPD" RESET;  opt_color = GREEN; break;
            case UPGRADE_MAXHP:         icon = RED "HP+" RESET;    opt_color = RED;   break;
            case UPGRADE_DEF:           icon = BLUE "DEF" RESET;   opt_color = BLUE;  break;
            case UPGRADE_HEAL:          icon = GREEN "HEAL" RESET; opt_color = GREEN; break;
            case UPGRADE_WEAPON:        icon = YELLOW "WPN" RESET; opt_color = YELLOW;break;
            case UPGRADE_RANGE:         icon = CYAN "RNG" RESET;   opt_color = CYAN;  break;
            case UPGRADE_EXPBOOST:      icon = CYAN "EXP" RESET;   opt_color = CYAN;  break;
            case UPGRADE_DETERMINATION: icon = MAGENTA "DT!" RESET;opt_color = MAGENTA;break;
            case UPGRADE_MAGNET:        icon = MAGENTA "MAG" RESET;opt_color = MAGENTA;break;
            default:                    icon = "?";                opt_color = WHITE; break;
        }
        printf("\033[%d;%dH", pad_y + HUD_LINES + ARENA_H + 5 + i, pad_x);
        printf(YELLOW "  |" RESET "  [%d] %-4s | %s%-22s" RESET " | %-24s" YELLOW "|" RESET "\n",
               i + 1, icon, opt_color, opts[i].name, opts[i].desc);
    }

    printf("\033[%d;%dH", pad_y + HUD_LINES + ARENA_H + 5 + count, pad_x);
    printf(YELLOW "  +----------------------------------------------------------+" RESET "\n");
    printf("\033[%d;%dH", pad_y + HUD_LINES + ARENA_H + 6 + count, pad_x);
    printf("  " DIM "按 1/2/3 选择升级..." RESET);

    fflush(stdout);
}

/* ============ 波次通关 ============ */

void render_wave_clear(WaveManager *wm) {
    int term_w, term_h;
    get_term_size(&term_w, &term_h);
    int content_w = 72;
    int content_h = HUD_LINES + ARENA_H + 1;
    int pad_x = (term_w > content_w) ? (term_w - content_w) / 2 : 0;
    int pad_y = (term_h > content_h) ? (term_h - content_h) / 2 : 0;

    int cy = pad_y + HUD_LINES + ARENA_H / 2 - 2;
    printf(HOME);
    printf("\033[%d;%dH", cy, pad_x);
    printf(CYAN "         +------------------------------------------+" RESET "\n");
    printf("\033[%d;%dH", cy + 1, pad_x);
    printf(CYAN "         |" RESET YELLOW "           Wave %d Clear!              " RESET CYAN "|" RESET "\n", wm->wave);
    printf("\033[%d;%dH", cy + 2, pad_x);
    printf(CYAN "         |" RESET GREEN "           敌人全灭！                  " RESET CYAN "|" RESET "\n");
    printf("\033[%d;%dH", cy + 3, pad_x);
    printf(CYAN "         +------------------------------------------+" RESET "\n");
    printf("\033[%d;%dH", cy + 4, pad_x);
    printf("         " DIM "下一波即将到来..." RESET);
    fflush(stdout);
}

/* ============ 暂停画面 ============ */

void render_pause_screen(void) {
    int term_w, term_h;
    get_term_size(&term_w, &term_h);
    int content_w = 72;
    int content_h = HUD_LINES + ARENA_H + 1;
    int pad_x = (term_w > content_w) ? (term_w - content_w) / 2 : 0;
    int pad_y = (term_h > content_h) ? (term_h - content_h) / 2 : 0;

    int cy = pad_y + HUD_LINES + ARENA_H / 2 - 1;
    printf(HOME);
    printf("\033[%d;%dH", cy, pad_x);
    printf(YELLOW "  +------------------------------------------+" RESET "\n");
    printf("\033[%d;%dH", cy + 1, pad_x);
    printf(YELLOW "  |" RESET "                  PAUSED                    " YELLOW "|" RESET "\n");
    printf("\033[%d;%dH", cy + 2, pad_x);
    printf(YELLOW "  |" RESET "             按 P 继续游戏                  " YELLOW "|" RESET "\n");
    printf("\033[%d;%dH", cy + 3, pad_x);
    printf(YELLOW "  +------------------------------------------+" RESET "\n");
    fflush(stdout);
}

/* ============ 游戏结束画面 ============ */

void render_gameover_screen(Player *p, WaveManager *wm, SaveData *sd, int is_new_high) {
    clear_screen();
    (void)sd;

    int term_w, term_h;
    get_term_size(&term_w, &term_h);
    int content_w = 52;
    int content_h = 20;
    int pad_x = (term_w > content_w) ? (term_w - content_w) / 2 : 0;
    int pad_y = (term_h > content_h) ? (term_h - content_h) / 2 : 0;

    int score = wm->wave * 100 + wm->total_killed * 10;

    printf("\033[%d;%dH", pad_y + 0, pad_x);
    printf(RED"              +------------------------------------------+\n");
    printf("\033[%d;%dH", pad_y + 1, pad_x);
    printf("              |                                          |\n");
    printf("\033[%d;%dH", pad_y + 2, pad_x);
    printf("              |            G A M E   O V E R             |\n");
    printf("\033[%d;%dH", pad_y + 3, pad_x);
    printf("              |                                          |\n");
    printf("\033[%d;%dH", pad_y + 4, pad_x);
    printf("              +------------------------------------------+" RESET "\n");
    printf("\033[%d;%dH", pad_y + 5, pad_x);
    printf("\n");
    printf("\033[%d;%dH", pad_y + 6, pad_x);
    printf(DIM"              * 你的灵魂破碎了...\n" RESET);
    printf("\033[%d;%dH", pad_y + 7, pad_x);
    printf(DIM"              * 但决心永远不会真正消失。\n" RESET);
    printf("\033[%d;%dH", pad_y + 8, pad_x);
    printf("\n");
    printf("\033[%d;%dH", pad_y + 9, pad_x);
    printf(YELLOW"              ============ 战斗记录 ============\n" RESET);
    printf("\033[%d;%dH", pad_y + 10, pad_x);
    printf("              ");
    printf("波次: %-6d", wm->wave);
    printf("击杀: %-6d\n", wm->total_killed);
    printf("\033[%d;%dH", pad_y + 11, pad_x);
    printf("              ");
    printf("等级: %-6d", p->level);
    printf("得分: %-6d\n", score);
    printf("\033[%d;%dH", pad_y + 12, pad_x);
    printf("              ");
    printf("最高分: %d\n", sd->high_score);
    if (is_new_high) {
        printf("\033[%d;%dH", pad_y + 13, pad_x);
        printf(GREEN"              *** 新的最高分！***\n" RESET);
    }
    printf("\033[%d;%dH", pad_y + 14, pad_x);
    printf("\n");
    printf("\033[%d;%dH", pad_y + 15, pad_x);
    printf(GREEN"              [1]  再来一局\n" RESET);
    printf("\033[%d;%dH", pad_y + 16, pad_x);
    printf(YELLOW"              [2]  返回菜单\n" RESET);
    printf("\033[%d;%dH", pad_y + 17, pad_x);
    printf("\n");
    printf("\033[%d;%dH", pad_y + 18, pad_x);
    printf(DIM"              ============================================" RESET);

    fflush(stdout);
}
