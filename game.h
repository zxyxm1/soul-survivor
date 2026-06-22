/**
 * game.h - Soul Survivor: 地下生存
 * 吸血鬼幸存者 × UNDERTALE 风格命令行游戏
 *
 * 课程设计 - 程序设计基础 A - AI Coding Assignment
 */

#ifndef GAME_H
#define GAME_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
    #define CLEAR_CMD "cls"
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
    #define CLEAR_CMD "clear"
#endif

/* ============ 常量定义 ============ */
#define ARENA_W     60      /* 竞技场宽度 */
#define ARENA_H     17      /* 竞技场高度 */
#define MAX_ENEMIES 30      /* 屏幕最大敌人数量 */
#define MAX_EFFECTS 60      /* 最大同时显示效果数（含火花/拖尾粒子） */
#define MAX_WEAPONS 8       /* 最大武器种类 */
#define MAX_PLAYER_WEAPONS 4 /* 玩家最多持有的武器数 */
#define MAX_UPGRADES 20     /* 升级选项池大小 */
#define BASE_FPS    60      /* 基础帧率 */
#define SAVE_FILE   "soul_survivor_save.dat"

/* HUD区域 */
#define HUD_LINES   5       /* HUD占用行数 */

/* ============ 颜色宏 ============ */
#define RED     "\033[91m"
#define GREEN   "\033[92m"
#define YELLOW  "\033[93m"
#define BLUE    "\033[94m"
#define MAGENTA "\033[95m"
#define CYAN    "\033[96m"
#define WHITE   "\033[97m"
#define DIM     "\033[2m"
#define BOLD    "\033[1m"
#define RESET   "\033[0m"

/* 光标控制 */
#define HOME    "\033[H"
#define CLEAR   "\033[2J"
#define HIDE_CUR "\033[?25l"
#define SHOW_CUR "\033[?25h"

/* ============ 枚举 ============ */
typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_LEVELUP,
    STATE_PAUSED,
    STATE_GAMEOVER,
    STATE_WAVE_CLEAR,
    STATE_QUIT
} GameState;

typedef enum {
    ENEMY_FROGGIT,      /* 青蛙型 */
    ENEMY_WHIMSUN,      /* 蝴蝶型（胆小） */
    ENEMY_MOLDSMAL,     /* 史莱姆型 */
    ENEMY_LOOX,         /* 兔子型 */
    ENEMY_VEGETOID,     /* 植物型 */
    ENEMY_MIGOSP,       /* 小Boss */
    ENEMY_GREATER_DOG,  /* Boss */
    ENEMY_TYPE_COUNT
} EnemyType;

typedef enum {
    WEAPON_STICK,       /* 树枝 - 基础(近战) */
    WEAPON_BONE,        /* 骨头 - 穿透(远程) */
    WEAPON_FIRE,        /* 火焰 - 范围(近战) */
    WEAPON_KNIFE,       /* 小刀 - 快速(近战) */
    WEAPON_LIGHTNING,   /* 闪电 - 连锁(远程) */
    WEAPON_COUNT
} WeaponType;

typedef enum {
    EFX_PROJECTILE,     /* 弹幕（远程） */
    EFX_SLASH,          /* 劈砍（近战） */
    EFX_SPARK,          /* 命中火花粒子 */
    EFX_TRAIL,          /* 弹幕拖尾粒子 */
} EffectType;

typedef enum {
    UPGRADE_ATK,        /* 攻击力+ */
    UPGRADE_SPD,        /* 速度+ */
    UPGRADE_MAXHP,      /* 最大生命+ */
    UPGRADE_DEF,        /* 防御+ */
    UPGRADE_HEAL,       /* 回复生命 */
    UPGRADE_WEAPON,     /* 获得新武器 */
    UPGRADE_RANGE,      /* 攻击范围+ */
    UPGRADE_EXPBOOST,   /* 经验加成 */
    UPGRADE_DETERMINATION, /* 决心（复活） */
    UPGRADE_MAGNET,     /* 磁铁（吸经验） */
    UPGRADE_COUNT
} UpgradeType;

/* ============ 结构体 ============ */
typedef struct {
    int id;
    char name[20];
    char desc[48];
    int damage;
    int cooldown;       /* 攻击冷却帧数 */
    int range;           /* 攻击范围（格子） */
    int target_count;   /* 同时攻击目标数 */
    int piercing;        /* 是否穿透 */
    int aoe_radius;      /* 范围伤害半径（0=无） */
    char symbol[8];      /* 显示的符号 */
} Weapon;

typedef struct {
    int x, y;            /* 玩家位置 */
    int hp, max_hp;
    int atk;              /* 基础攻击力 */
    int def;              /* 防御力 */
    int spd;              /* 每N帧移动一次 (越小越快) */
    int level;
    int exp;
    int exp_to_next;
    int exp_boost;        /* 经验倍率 (100 = 1x) */
    int weapon_count;
    int weapons[MAX_PLAYER_WEAPONS];  /* 持有的武器ID */
    int weapon_cooldowns[MAX_PLAYER_WEAPONS]; /* 各武器冷却剩余 */
    int special_cooldown;  /* 特殊技能冷却 */
    int special_max_cd;
    int determination;    /* 决心：死亡时复活次数 */
    int magnet_range;     /* 吸经验范围 */
    int invincible;       /* 无敌帧数 */
    int attack_flash;     /* 攻击闪烁倒计时（帧数）*/
    int move_timer;       /* 移动计时器：毫秒累积，帧率无关 */
    int frame_ms;         /* 上一帧实际耗时（毫秒）*/
    int kills;            /* 击杀计数 */
} Player;

typedef struct {
    int x, y;
    int hp, max_hp;
    int atk;
    int def;
    int spd;              /* 每N帧移动一次 */
    int exp_reward;
    EnemyType type;
    int move_timer;       /* 移动计时器 */
    int active;           /* 是否活跃 */
    char symbol[8];       /* 显示符号（UTF-8） */
    int flash_timer;      /* 受伤闪烁 */
} Enemy;

typedef struct {
    int wave;
    int enemies_alive;
    int spawn_queue;      /* 待生成数量 */
    int spawn_timer;
    int total_killed;
    int wave_duration;    /* 当前波持续时间 */
    int boss_spawned;
} WaveManager;

typedef struct {
    int high_score;
    int total_plays;
    int total_kills;
    char player_name[32];
} SaveData;

typedef struct {
    UpgradeType type;
    char name[24];
    char desc[48];
    int value1;           /* 主数值 */
    int value2;           /* 副数值（如武器ID） */
} UpgradeOption;

typedef struct {
    int active;
    EffectType type;
    int x, y;            /* 当前位置（格子） */
    int target_idx;       /* 目标敌人索引（弹幕用） */
    int damage;           /* 伤害值 */
    int piercing;         /* 穿透 */
    int aoe_radius;       /* 范围伤害半径 */
    int lifetime;         /* 剩余帧数 */
    int max_lifetime;     /* 总帧数（用于动画） */
    int speed;            /* 每N帧移动1格 */
    int move_timer;       /* 移动计时器 */
    int max_range;        /* 最大射程 */
    int traveled;         /* 已移动格数 */
    char symbol;          /* 显示字符 */
    const char *color;    /* ANSI颜色 */
} VisualEffect;

/* ============ 函数声明 ============ */

/* main.c */
void init_game(Player *p, WaveManager *wm, Enemy enemies[], SaveData *sd);
void game_loop(void);
int  get_input(void);
void sleep_ms(int ms);
void enable_vt_mode(void);

/* player.c */
void player_init(Player *p);
void player_move(Player *p, int dx, int dy);
int  player_take_damage(Player *p, int damage);
void player_gain_exp(Player *p, int amount);
int  player_check_levelup(Player *p);
void player_levelup(Player *p, UpgradeOption *choice);
void player_update_cooldowns(Player *p);
void player_set_weapon_cooldown(Player *p, int slot);
int  player_add_weapon(Player *p, int weapon_id);

/* enemy.c */
void enemy_init_pool(Enemy enemies[]);
int  enemy_spawn(Enemy enemies[], WaveManager *wm, int arena_w, int arena_h);
void enemy_update(Enemy enemies[], Player *p, int arena_w, int arena_h);
void enemy_take_damage(Enemy *e, int damage);
int  enemy_is_dead(Enemy *e);
int  count_active_enemies(Enemy enemies[]);
void wave_init(WaveManager *wm);
void wave_update(WaveManager *wm, Enemy enemies[], int arena_w, int arena_h);
int  wave_is_complete(WaveManager *wm);

/* render.c */
void render_init(void);
void render_frame(Player *p, Enemy enemies[], WaveManager *wm, VisualEffect effects[], int fps);
void render_title_screen(void);
void render_gameover_screen(Player *p, WaveManager *wm, SaveData *sd, int is_new_high);
void render_levelup_menu(Player *p, UpgradeOption opts[], int count);
void render_wave_clear(WaveManager *wm);
void render_pause_screen(void);
void draw_box(int x, int y, int w, int h, const char *color);
void draw_text(int x, int y, const char *text, const char *color);
void draw_hp_bar(int x, int y, int current, int max, int width);
void draw_exp_bar(int x, int y, int current, int max, int width);
void clear_screen(void);

/* combat.c */
void combat_tick(Player *p, Enemy enemies[], VisualEffect effects[]);
void auto_attack(Player *p, Enemy enemies[], int weapon_slot, VisualEffect effects[]);
int  calc_damage(int atk, int def);
int  distance(int x1, int y1, int x2, int y2);
void collect_exp(Player *p, Enemy enemies[]);
UpgradeOption* generate_upgrades(Player *p, int *count);
void apply_upgrade(Player *p, UpgradeOption *opt);
void player_use_special(Player *p, Enemy enemies[]);

/* effect.c */
void effects_init(VisualEffect effects[]);
int  effects_spawn_projectile(VisualEffect effects[], int x, int y,
        int target_idx, int damage, int piercing, int aoe_radius,
        int speed, int max_range, char symbol, const char *color);
int  effects_spawn_slash(VisualEffect effects[], int x, int y, int duration,
        const char *color);
int  effects_spawn_spark(VisualEffect effects[], int x, int y,
        int lifetime, const char *color);
void effects_spawn_hit_burst(VisualEffect effects[], int x, int y,
        int count, const char *color);
void effects_update(VisualEffect effects[], Enemy enemies[], Player *p);

/* data.c */
void save_load(SaveData *sd);
void save_write(SaveData *sd);
void save_update_high_score(SaveData *sd, int score, int kills);

#endif /* GAME_H */
