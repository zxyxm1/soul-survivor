/**
 * main.c - 游戏入口、主菜单、主循环
 * Soul Survivor: 地下生存
 */
#include "game.h"

/* 全局武器定义 */
Weapon weapon_library[MAX_WEAPONS] = {
    {WEAPON_STICK,    "树枝",    "基础武器，范围攻击",     3, 48, 3, 1, 0, 0, "~"},
    {WEAPON_BONE,     "骨头攻击","穿透型，直线伤害",       5, 72, 5, 3, 1, 0, "|"},
    {WEAPON_FIRE,     "火焰魔法","范围灼烧",              4, 88, 2, 1, 0, 2, "*"},
    {WEAPON_KNIFE,    "小刀",    "快速近战",             2, 24, 2, 1, 0, 0, "+"},
    {WEAPON_LIGHTNING,"闪电",   "连锁攻击多个敌人",       3, 60, 3, 4, 0, 0, "~"},
};

/* 全局变量 */
static GameState game_state;
static Player player;
static Enemy enemies[MAX_ENEMIES];
static VisualEffect effects[MAX_EFFECTS];
static WaveManager wave_mgr;
static SaveData save_data;
static UpgradeOption upgrade_opts[3];
static int frame_count = 0;
static int current_fps = 240;
static int is_new_high_score = 0;
static int fps_calibrated = 0;  /* 本局是否已校准帧率 */

/* ============ 平台相关函数 ============ */

void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void enable_vt_mode(void) {
#ifdef _WIN32
    /* 设置控制台编码为UTF-8，避免中文乱码 */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    /* 启用Windows 10+的ANSI转义序列支持 */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    /* 设置系统定时器精度为1ms，让Sleep()真正按毫秒休眠 */
    /* Windows默认15.6ms精度会导致60FPS帧率严重下降 */
    {
        HMODULE hWinmm = LoadLibraryA("winmm.dll");
        if (hWinmm) {
            typedef UINT (WINAPI *FnTimeBeginPeriod)(UINT);
            FnTimeBeginPeriod pTimeBeginPeriod =
                (FnTimeBeginPeriod)(void*)GetProcAddress(hWinmm, "timeBeginPeriod");
            if (pTimeBeginPeriod) {
                pTimeBeginPeriod(1);
            }
        }
    }
#endif
}

int get_input(void) {
#ifdef _WIN32
    if (_kbhit()) {
        int ch = _getch();
        /* 处理方向键等特殊键 */
        if (ch == 0 || ch == 224) {
            ch = _getch();
            switch (ch) {
                case 72: return 'w';  /* 上 */
                case 80: return 's';  /* 下 */
                case 75: return 'a';  /* 左 */
                case 77: return 'd';  /* 右 */
                default: return 0;
            }
        }
        return ch;
    }
    return 0;
#else
    /* Unix: 非阻塞读取 */
    struct termios oldt, newt;
    int oldf;
    int ch = 0;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    /* 处理转义序列 */
    if (ch == 27) {
        int c2 = getchar();
        if (c2 == 91) {
            int c3 = getchar();
            switch (c3) {
                case 65: ch = 'w'; break;
                case 66: ch = 's'; break;
                case 68: ch = 'a'; break;
                case 67: ch = 'd'; break;
                default: ch = 0;
            }
        } else {
            ch = 27; /* ESC键 */
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch == EOF) ch = 0;
    return ch;
#endif
}

/* ============ 游戏初始化 ============ */

void init_game(Player *p, WaveManager *wm, Enemy enemies[], SaveData *sd) {
    player_init(p);
    wave_init(wm);
    enemy_init_pool(enemies);
    effects_init(effects);
    save_load(sd);

    /* 初始生成几个敌人 */
    enemies[0].active = 0;
    wm->spawn_queue = 4;  /* 第一波预生成 */
    wm->spawn_timer = 0;

    frame_count = 0;
    is_new_high_score = 0;
}

void reset_game(void) {
    init_game(&player, &wave_mgr, enemies, &save_data);
    fps_calibrated = 0;   /* 新一局重新校准 */
    game_state = STATE_PLAYING;
}

/* ============ 主循环 ============ */

void game_loop(void) {
    int input;

#ifdef _WIN32
    /* 高精度帧计时 */
    LARGE_INTEGER freq, frame_start, frame_end;
    QueryPerformanceFrequency(&freq);
    double target_frame_s = 1.0 / BASE_FPS;  /* 默认值，校准后覆盖 */
    const double MIN_FRAME_S = 1.0 / 240.0;
    const double MAX_FRAME_S = 1.0 / 15.0;
#else
    int frame_delay = 1000 / BASE_FPS;  /* 基础帧间隔ms: 16ms @ 60FPS */
#endif

    while (game_state != STATE_QUIT) {

#ifdef _WIN32
        QueryPerformanceCounter(&frame_start);
#endif

        switch (game_state) {

            case STATE_TITLE: {
                render_title_screen();
                input = 0;
                while (input == 0) {
                    input = get_input();
                    sleep_ms(50);
                }
                if (input == '1' || input == '\r' || input == '\n') {
                    reset_game();
                } else if (input == '2') {
                    save_load(&save_data);
                    printf("\n" YELLOW "  最高分: %d | 总击杀: %d | 游戏次数: %d" RESET "\n",
                           save_data.high_score, save_data.total_kills, save_data.total_plays);
                    printf("\n  按任意键返回...\n");
                    sleep_ms(1000);
                    while (!get_input()) sleep_ms(50);
                } else if (input == '3' || input == 'q' || input == 'Q') {
                    game_state = STATE_QUIT;
                }
                break;
            }

            case STATE_PLAYING: {
                /* --- 帧率校准（每局一次）--- */
#ifdef _WIN32
                if (!fps_calibrated) {
                    int cal_frames = 40;
                    LARGE_INTEGER cal_start, cal_end;
                    double cal_total = 0.0;
                    int cal_i;

                    /* 预生成敌人模拟真实负载 */
                    for (cal_i = 0; cal_i < 10; cal_i++) {
                        enemy_spawn(enemies, &wave_mgr, ARENA_W, ARENA_H);
                    }

                    clear_screen();
                    for (cal_i = 0; cal_i < cal_frames; cal_i++) {
                        QueryPerformanceCounter(&cal_start);
                        render_frame(&player, enemies, &wave_mgr, effects, 60);
                        QueryPerformanceCounter(&cal_end);
                        cal_total += (double)(cal_end.QuadPart - cal_start.QuadPart) / freq.QuadPart;
                    }

                    /* 取平均值，留 8% 余量 */
                    double avg = cal_total / cal_frames;
                    target_frame_s = avg * 1.08;
                    if (target_frame_s < MIN_FRAME_S) target_frame_s = MIN_FRAME_S;
                    if (target_frame_s > MAX_FRAME_S) target_frame_s = MAX_FRAME_S;
                    current_fps = (int)(1.0 / target_frame_s + 0.5);

                    /* 清除校准产生的敌人 */
                    enemy_init_pool(enemies);
                    wave_mgr.spawn_queue = 4;
                    wave_mgr.spawn_timer = 0;
                    wave_mgr.enemies_alive = 0;

                    fps_calibrated = 1;
                }
#endif
                /* --- 输入处理 --- */
                {
                    int dx = 0, dy = 0;

#ifdef _WIN32
                    /* WASD 移动：GetAsyncKeyState 实时读取物理按键状态 */
                    /* 高位置1表示按键当前被按下，支持同时检测多个键（斜向移动）*/
                    if (GetAsyncKeyState('W') & 0x8000) dy = -1;
                    if (GetAsyncKeyState('S') & 0x8000) dy = 1;
                    if (GetAsyncKeyState('A') & 0x8000) dx = -1;
                    if (GetAsyncKeyState('D') & 0x8000) dx = 1;

                    /* 清空字符缓冲区，处理动作按键（空格/暂停/退出）*/
                    while (_kbhit()) {
                        int ch = _getch();
                        if (ch == 0 || ch == 224) {
                            _getch();  /* 跳过方向键扩展码 */
                            continue;
                        }
                        switch (ch) {
                            case ' ': player_use_special(&player, enemies); break;
                            case 'p': case 'P': game_state = STATE_PAUSED; break;
                            case 'q': case 'Q':
                            case 27: /* ESC */
                                game_state = STATE_TITLE;
                                clear_screen();
                                break;
                        }
                    }
#else
                    /* Unix: 循环读取所有字符输入 */
                    while ((input = get_input()) != 0) {
                        switch (input) {
                            case 'w': case 'W': dy = -1; break;
                            case 's': case 'S': dy = 1;  break;
                            case 'a': case 'A': dx = -1; break;
                            case 'd': case 'D': dx = 1;  break;
                            case ' ': player_use_special(&player, enemies); break;
                            case 'p': case 'P': game_state = STATE_PAUSED; break;
                            case 'q': case 'Q':
                            case 27:
                                game_state = STATE_TITLE;
                                clear_screen();
                                break;
                        }
                    }
#endif

                    /* 移动速度限制：基于时间（毫秒），帧率无关 */
                    {
                        int move_interval = player.spd * 1000 / 60; /* spd=3 → 50ms */
                        if (dx != 0 || dy != 0) {
                            player.move_timer += player.frame_ms;
                            while (player.move_timer >= move_interval) {
                                player.move_timer -= move_interval;
                                player_move(&player, dx, dy);
                            }
                        } else {
                            player.move_timer = move_interval;  /* 就绪：下次按键立即移动 */
                        }
                    }
                }

                /* --- 游戏逻辑更新 --- */
                wave_update(&wave_mgr, enemies, ARENA_W, ARENA_H);
                enemy_update(enemies, &player, ARENA_W, ARENA_H);
                combat_tick(&player, enemies, effects);
                effects_update(effects, enemies, &player);
                collect_exp(&player, enemies);
                player_update_cooldowns(&player);

                /* 同步击杀计数到波次管理器 */
                wave_mgr.total_killed = player.kills;

                /* --- 检查升级 --- */
                if (player_check_levelup(&player)) {
                    int count;
                    UpgradeOption *opts = generate_upgrades(&player, &count);
                    memcpy(upgrade_opts, opts, sizeof(UpgradeOption) * (count < 3 ? count : 3));
                    if (count < 3) count = 3; /* 保证有3个选项 */
                    game_state = STATE_LEVELUP;
                }

                /* --- 检查波次完成 --- */
                if (wave_is_complete(&wave_mgr)) {
                    wave_mgr.wave++;
                    wave_mgr.spawn_queue = 3 + wave_mgr.wave * 2;
                    wave_mgr.spawn_timer = 0;
                    wave_mgr.boss_spawned = 0;
                    wave_mgr.wave_duration = 0;
                    game_state = STATE_WAVE_CLEAR;
                }

                /* --- 死亡检查 --- */
                if (player.hp <= 0) {
                    /* 决心复活 */
                    if (player.determination > 0) {
                        player.determination--;
                        player.hp = player.max_hp / 2;
                        player.invincible = 30; /* 2秒无敌 */
                    } else {
                        game_state = STATE_GAMEOVER;
                        /* 更新最高分 */
                        int score = wave_mgr.wave * 100 + wave_mgr.total_killed * 10;
                        if (score > save_data.high_score) {
                            is_new_high_score = 1;
                            save_data.high_score = score;
                        }
                        save_data.total_kills += wave_mgr.total_killed;
                        save_data.total_plays++;
                        save_write(&save_data);
                    }
                }

                frame_count++;
                break;
            }

            case STATE_LEVELUP: {
                render_levelup_menu(&player, upgrade_opts, 3);
                input = 0;
                while (input != '1' && input != '2' && input != '3') {
                    input = get_input();
                    sleep_ms(50);
                }
                player_levelup(&player, &upgrade_opts[input - '1']);
                /* 消耗一些升级经验 */
                player.exp -= player.exp_to_next;
                player.exp_to_next = (player.level + 1) * 15;
                game_state = STATE_PLAYING;
                break;
            }

            case STATE_WAVE_CLEAR: {
                render_wave_clear(&wave_mgr);
                sleep_ms(1500);
                /* 快速生成下一波的敌人 */
                while (wave_mgr.spawn_queue > 0 &&
                       count_active_enemies(enemies) < MAX_ENEMIES) {
                    enemy_spawn(enemies, &wave_mgr, ARENA_W, ARENA_H);
                }
                game_state = STATE_PLAYING;
                break;
            }

            case STATE_PAUSED: {
                render_pause_screen();
                input = 0;
                while (input != 'p' && input != 'P' && input != 27) {
                    input = get_input();
                    sleep_ms(50);
                }
                game_state = STATE_PLAYING;
                break;
            }

            case STATE_GAMEOVER: {
                render_gameover_screen(&player, &wave_mgr, &save_data, is_new_high_score);
                input = 0;
                while (input != '1' && input != '2') {
                    input = get_input();
                    sleep_ms(50);
                }
                if (input == '1') {
                    reset_game();
                } else {
                    game_state = STATE_TITLE;
                    clear_screen();
                }
                break;
            }

            default:
                break;
        }

        /* --- 渲染 --- */
        if (game_state == STATE_PLAYING) {
            render_frame(&player, enemies, &wave_mgr, effects, current_fps);

#ifdef _WIN32
            /* 固定帧率控制（目标值在游戏开始时校准）*/
            QueryPerformanceCounter(&frame_end);
            double elapsed = (double)(frame_end.QuadPart - frame_start.QuadPart) / freq.QuadPart;

            /* 显示用 FPS：EMA 平滑（α=0.1，较快响应）*/
            {
                static double display_ema = 0.0;
                if (display_ema == 0.0) display_ema = target_frame_s;
                display_ema = display_ema * 0.9 + elapsed * 0.1;
                current_fps = (int)(1.0 / display_ema + 0.5);
                if (current_fps < 15) current_fps = 15;
                if (current_fps > 240) current_fps = 240;
            }

            double remaining = target_frame_s - elapsed;
            if (remaining > 0.002) {
                Sleep((DWORD)((remaining - 0.001) * 1000));
            }
            /* 忙等微调 */
            do {
                QueryPerformanceCounter(&frame_end);
                elapsed = (double)(frame_end.QuadPart - frame_start.QuadPart) / freq.QuadPart;
            } while (elapsed < target_frame_s);

            /* 存储本帧实际耗时（毫秒），供下帧移动系统使用 */
            player.frame_ms = (int)(elapsed * 1000.0 + 0.5);
#else
            sleep_ms(frame_delay);
            player.frame_ms = frame_delay;
#endif
        }
    }
}

/* ============ 程序入口 ============ */

int main(void) {
    /* 初始化 */
    enable_vt_mode();
    srand((unsigned int)time(NULL));
    render_init();

    /* 设置初始状态 */
    game_state = STATE_TITLE;
    memset(enemies, 0, sizeof(enemies));
    memset(&player, 0, sizeof(player));
    memset(&wave_mgr, 0, sizeof(wave_mgr));
    memset(&save_data, 0, sizeof(save_data));

    /* 运行游戏 */
    game_loop();

    /* 清理 */
    clear_screen();
    printf(SHOW_CUR);
    printf(WHITE "  感谢游玩！Soul Survivor —— 地下生存\n" RESET);
    printf(DIM "  程序设计基础 A - 课程设计作品\n" RESET);

    return 0;
}
