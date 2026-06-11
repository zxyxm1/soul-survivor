/**
 * player.c - 玩家逻辑：初始化、移动、伤害、升级
 * Soul Survivor: 地下生存
 */
#include "game.h"

/* 外部引用 */
extern Weapon weapon_library[MAX_WEAPONS];

void player_init(Player *p) {
    p->x = ARENA_W / 2;
    p->y = ARENA_H / 2;
    p->max_hp = 20;
    p->hp = 20;
    p->atk = 3;
    p->def = 0;
    p->spd = 3;            /* 每3帧移动一次 */
    p->level = 1;
    p->exp = 0;
    p->exp_to_next = 15;
    p->exp_boost = 100;    /* 100% = 1x */
    p->weapon_count = 1;
    p->weapons[0] = WEAPON_STICK;   /* 初始武器：树枝 */
    p->weapons[1] = -1;
    p->weapons[2] = -1;
    p->weapons[3] = -1;
    p->weapon_cooldowns[0] = 0;
    p->weapon_cooldowns[1] = 0;
    p->weapon_cooldowns[2] = 0;
    p->weapon_cooldowns[3] = 0;
    p->special_cooldown = 0;
    p->special_max_cd = 440; /* 特殊技能冷却：~7秒 */
    p->determination = 0;    /* 初始无决心复活 */
    p->magnet_range = 3;
    p->move_timer = 50;
    p->frame_ms = 17;         /* 默认 ~60FPS */
    p->invincible = 220;     /* 开局短暂无敌 ~3.7秒 */
    p->kills = 0;
}

void player_move(Player *p, int dx, int dy) {
    int new_x = p->x + dx;
    int new_y = p->y + dy;

    /* 边界检查（竞技场内部） */
    if (new_x >= 1 && new_x < ARENA_W - 1 &&
        new_y >= 1 && new_y < ARENA_H - 1) {
        p->x = new_x;
        p->y = new_y;
    }
}

int player_take_damage(Player *p, int damage) {
    if (p->invincible > 0) return 0;  /* 无敌 */

    int actual = damage - p->def;
    if (actual < 1) actual = 1;       /* 最低1点伤害 */

    p->hp -= actual;
    p->invincible = 60;               /* 受伤后短暂无敌 ~1秒 */

    return actual;
}

void player_gain_exp(Player *p, int amount) {
    /* 应用经验加成 */
    int gained = (amount * p->exp_boost) / 100;
    if (gained < 1) gained = 1;
    p->exp += gained;
}

int player_check_levelup(Player *p) {
    return (p->exp >= p->exp_to_next);
}

void player_levelup(Player *p, UpgradeOption *choice) {
    p->level++;
    apply_upgrade(p, choice);
}

void player_update_cooldowns(Player *p) {
    int i;
    /* 武器冷却 */
    for (i = 0; i < p->weapon_count; i++) {
        if (p->weapon_cooldowns[i] > 0) {
            p->weapon_cooldowns[i]--;
        }
    }
    /* 特殊技能冷却 */
    if (p->special_cooldown > 0) {
        p->special_cooldown--;
    }
    /* 无敌帧 */
    if (p->invincible > 0) {
        p->invincible--;
    }
}

void player_set_weapon_cooldown(Player *p, int slot) {
    if (slot < 0 || slot >= p->weapon_count) return;
    int wid = p->weapons[slot];
    if (wid < 0 || wid >= WEAPON_COUNT) return;
    p->weapon_cooldowns[slot] = weapon_library[wid].cooldown;
}

/**
 * 给玩家添加新武器
 * 返回 1 成功，0 失败（已有或已满）
 */
int player_add_weapon(Player *p, int weapon_id) {
    int i;
    /* 检查是否已有 */
    for (i = 0; i < p->weapon_count; i++) {
        if (p->weapons[i] == weapon_id) return 0;
    }
    /* 检查是否已满 */
    if (p->weapon_count >= MAX_PLAYER_WEAPONS) return 0;
    /* 添加 */
    p->weapons[p->weapon_count] = weapon_id;
    p->weapon_cooldowns[p->weapon_count] = 0;
    p->weapon_count++;
    return 1;
}
