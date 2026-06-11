/**
 * enemy.c - 敌人系统：类型定义、生成、AI移动
 * Soul Survivor: 地下生存
 * 敌人设计参考 UNDERTALE 怪物
 */
#include "game.h"

/* ============ 敌人类型数据 ============ */
typedef struct {
    EnemyType type;
    char name[24];
    char symbol[8];      /* ASCII显示符号 */
    int base_hp;
    int base_atk;
    int base_def;
    int base_spd;        /* 每N帧移动 */
    int exp_reward;
    int is_boss;
} EnemyTemplate;

static const EnemyTemplate enemy_templates[ENEMY_TYPE_COUNT] = {
    /*  type               name       symbol  HP  ATK DEF SPD EXP boss */
    {ENEMY_FROGGIT,      "Froggit",   "♟",   8,  2,  0, 36,  5,  0},
    {ENEMY_WHIMSUN,      "Whimsun",   "♤",   5,  1,  0, 28,  3,  0},
    {ENEMY_MOLDSMAL,     "Moldsmal",  "◉",  12,  1,  2, 60,  6,  0},
    {ENEMY_LOOX,         "Loox",      "♢",  10,  3,  0, 36,  7,  0},
    {ENEMY_VEGETOID,     "Vegetoid",  "♧",  15,  2,  1, 44,  8,  0},
    {ENEMY_MIGOSP,       "Migosp",    "♠",  25,  4,  1, 28, 15,  0},
    {ENEMY_GREATER_DOG,  "GreaterDog","♛",  60,  6,  3, 44, 50,  1},
};

/**
 * 根据波次调整敌人属性
 */
static void scale_enemy(Enemy *e, int wave) {
    float scale = 1.0f + (wave - 1) * 0.25f;  /* 每波+25% */
    e->max_hp = (int)(e->max_hp * scale);
    e->hp = e->max_hp;
    e->atk = (int)(e->atk * scale);
    if (wave > 3) e->def += wave / 3;
    /* 后期敌人移动加快 */
    if (wave > 5) e->spd = e->spd > 8 ? e->spd - 4 : 8;
}

/* ============ 敌人池管理 ============ */

void enemy_init_pool(Enemy enemies[]) {
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }
}

/**
 * 在竞技场边缘生成一个敌人
 * 返回: 成功生成的位置索引，-1表示失败
 */
int enemy_spawn(Enemy enemies[], WaveManager *wm, int arena_w, int arena_h) {
    int i;
    EnemyType type;
    int edge;

    /* 找空位 */
    int slot = -1;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;  /* 屏幕已满 */

    /* 选择敌人类型 */
    if (wm->wave >= 5 && !wm->boss_spawned && wm->wave % 5 == 0) {
        /* Boss波 */
        type = ENEMY_GREATER_DOG;
        wm->boss_spawned = 1;
    } else if (wm->wave >= 6) {
        /* 后期混合生成 */
        int r = rand() % ENEMY_TYPE_COUNT;
        if (r == ENEMY_GREATER_DOG) r = ENEMY_MIGOSP;  /* Boss不随机出 */
        type = (EnemyType)r;
    } else if (wm->wave >= 3) {
        type = (EnemyType)(rand() % (ENEMY_MIGOSP + 1));
    } else {
        /* 前期简单敌人 */
        type = (EnemyType)(rand() % 3); /* Froggit, Whimsun, Moldsmal */
    }

    /* 从模板初始化 */
    const EnemyTemplate *tmpl = &enemy_templates[type];
    Enemy *e = &enemies[slot];
    e->type = type;
    strncpy(e->symbol, tmpl->symbol, sizeof(e->symbol) - 1);
    e->symbol[sizeof(e->symbol) - 1] = '\0';
    e->max_hp = tmpl->base_hp;
    e->hp = tmpl->base_hp;
    e->atk = tmpl->base_atk;
    e->def = tmpl->base_def;
    e->spd = tmpl->base_spd;
    e->exp_reward = tmpl->exp_reward;
    e->active = 1;
    e->move_timer = 0;
    e->flash_timer = 0;

    /* 出生在边缘 */
    edge = rand() % 4;
    switch (edge) {
        case 0: /* 上 */  e->x = 1 + rand() % (arena_w - 2); e->y = 1;           break;
        case 1: /* 下 */  e->x = 1 + rand() % (arena_w - 2); e->y = arena_h - 2; break;
        case 2: /* 左 */  e->x = 1;           e->y = 1 + rand() % (arena_h - 2); break;
        case 3: /* 右 */  e->x = arena_w - 2; e->y = 1 + rand() % (arena_h - 2); break;
    }

    /* 根据波次调整 */
    scale_enemy(e, wm->wave);

    /* Boss额外属性 */
    if (tmpl->is_boss) {
        e->max_hp += wm->wave * 10;
        e->hp = e->max_hp;
    }

    if (wm->spawn_queue > 0) wm->spawn_queue--;
    wm->enemies_alive++;

    return slot;
}

/**
 * 更新所有敌人的移动AI
 * 朝玩家方向移动
 */
void enemy_update(Enemy enemies[], Player *p, int arena_w, int arena_h) {
    int i;

    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->active) continue;

        /* 受伤闪烁计时 */
        if (e->flash_timer > 0) e->flash_timer--;

        /* 移动计时器 */
        e->move_timer++;
        if (e->move_timer < e->spd) continue;
        e->move_timer = 0;

        /* 计算移动方向（朝玩家） */
        int dx = 0, dy = 0;
        if (e->x < p->x) dx = 1;
        else if (e->x > p->x) dx = -1;
        if (e->y < p->y) dy = 1;
        else if (e->y > p->y) dy = -1;

        /* 80%概率沿主轴移动，20%概率随机偏移（减少聚集） */
        if (rand() % 100 < 20) {
            if (dx != 0 && dy != 0) {
                if (rand() % 2) dx = 0; else dy = 0;
            }
        }

        int nx = e->x + dx;
        int ny = e->y + dy;

        /* 边界检查 */
        if (nx >= 1 && nx < arena_w - 1 && ny >= 1 && ny < arena_h - 1) {
            /* 不与其他敌人重叠 */
            int blocked = 0;
            int j;
            for (j = 0; j < MAX_ENEMIES; j++) {
                if (i != j && enemies[j].active &&
                    enemies[j].x == nx && enemies[j].y == ny) {
                    blocked = 1;
                    break;
                }
            }
            if (!blocked) {
                e->x = nx;
                e->y = ny;
            }
        }

        /* 碰撞检测：碰到玩家 */
        if (e->x == p->x && e->y == p->y) {
            player_take_damage(p, e->atk);
            /* 碰撞后将敌人弹开 */
            e->x += (e->x > p->x) ? 2 : -2;
            e->y += (e->y > p->y) ? 2 : -2;
            /* 边界钳制 */
            if (e->x < 1) e->x = 1;
            if (e->x >= arena_w - 1) e->x = arena_w - 2;
            if (e->y < 1) e->y = 1;
            if (e->y >= arena_h - 1) e->y = arena_h - 2;
        }
    }
}

void enemy_take_damage(Enemy *e, int damage) {
    int actual = damage - e->def;
    if (actual < 1) actual = 1;
    e->hp -= actual;
    e->flash_timer = 2;  /* 闪烁2帧 */
}

int enemy_is_dead(Enemy *e) {
    return e->hp <= 0;
}

int count_active_enemies(Enemy enemies[]) {
    int i, count = 0;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) count++;
    }
    return count;
}

/* ============ 波次管理 ============ */

void wave_init(WaveManager *wm) {
    wm->wave = 1;
    wm->enemies_alive = 0;
    wm->spawn_queue = 4;
    wm->spawn_timer = 0;
    wm->total_killed = 0;
    wm->wave_duration = 0;
    wm->boss_spawned = 0;
}

void wave_update(WaveManager *wm, Enemy enemies[], int arena_w, int arena_h) {
    wm->wave_duration++;

    /* 更新存活计数 */
    wm->enemies_alive = count_active_enemies(enemies);

    /* 生成计时器：有剩余生成名额时持续生成 */
    if (wm->spawn_queue > 0) {
        wm->spawn_timer++;
        /* 每60-188帧生成一个，根据波次加速 */
        int spawn_interval = 188 - wm->wave;
        if (spawn_interval < 60) spawn_interval = 60;
        if (wm->spawn_timer >= spawn_interval) {
            wm->spawn_timer = 0;
            enemy_spawn(enemies, wm, arena_w, arena_h);
        }
    }

    /* 所有敌人死亡且无待生成 → 波次结束 */
}

int wave_is_complete(WaveManager *wm) {
    return (wm->spawn_queue <= 0 && wm->enemies_alive <= 0);
}
