/**
 * effect.c - 视觉效果系统：弹幕、劈砍、命中火花
 * Soul Survivor: 地下生存
 */
#include "game.h"

/* 外部引用 */
extern Weapon weapon_library[MAX_WEAPONS];

/* ============ 效果池管理 ============ */

void effects_init(VisualEffect effects[]) {
    int i;
    for (i = 0; i < MAX_EFFECTS; i++) {
        effects[i].active = 0;
    }
}

/**
 * 查找空闲效果槽
 */
static int effect_find_slot(VisualEffect effects[]) {
    int i;
    for (i = 0; i < MAX_EFFECTS; i++) {
        if (!effects[i].active) return i;
    }
    return -1;
}

/* ============ 生成弹幕（远程武器） ============ */

int effects_spawn_projectile(VisualEffect effects[], int x, int y,
        int target_idx, int damage, int piercing, int aoe_radius,
        int speed, int max_range, char symbol, const char *color) {
    int slot = effect_find_slot(effects);
    if (slot < 0) return -1;

    VisualEffect *e = &effects[slot];
    e->active = 1;
    e->type = EFX_PROJECTILE;
    e->x = x;
    e->y = y;
    e->target_idx = target_idx;
    e->damage = damage;
    e->piercing = piercing;
    e->aoe_radius = aoe_radius;
    e->lifetime = max_range * speed + 10;  /* 足够飞行到目标 */
    e->max_lifetime = e->lifetime;
    e->speed = speed;
    e->move_timer = 0;
    e->max_range = max_range;
    e->traveled = 0;
    e->symbol = symbol;
    e->color = color;

    return slot;
}

/* ============ 生成劈砍（近战武器） ============ */

int effects_spawn_slash(VisualEffect effects[], int x, int y, int duration,
        const char *color) {
    int slot = effect_find_slot(effects);
    if (slot < 0) return -1;

    VisualEffect *e = &effects[slot];
    e->active = 1;
    e->type = EFX_SLASH;
    e->x = x;
    e->y = y;
    e->target_idx = -1;
    e->damage = 0;
    e->piercing = 0;
    e->aoe_radius = 0;
    e->lifetime = duration;
    e->max_lifetime = duration;
    e->speed = 0;
    e->move_timer = 0;
    e->max_range = 0;
    e->traveled = 0;
    e->symbol = '/';       /* 初始符号，每帧旋转 */
    e->color = color;

    return slot;
}

/* ============ 劈砍动画符号旋转 ============ */

static char slash_rotate_symbol(int lifetime, int max_lifetime) {
    int elapsed = max_lifetime - lifetime;
    /* 使用不与边框冲突的字符：/ = \ ! */
    switch (elapsed % 4) {
        case 0: return '/';
        case 1: return '=';
        case 2: return '\\';
        case 3: return '!';
        default: return '/';
    }
}

/* ============ 弹幕：朝目标移动并检测命中 ============ */

static void projectile_update(VisualEffect *e, Enemy enemies[]) {
    /* 移动计时 */
    e->move_timer++;
    if (e->move_timer < e->speed) return;
    e->move_timer = 0;

    /* 检查目标是否仍然存活 */
    int tidx = e->target_idx;
    if (tidx < 0 || tidx >= MAX_ENEMIES || !enemies[tidx].active) {
        /* 目标已消失，弹幕也消失 */
        e->active = 0;
        return;
    }

    Enemy *target = &enemies[tidx];

    /* 朝目标移动1格 */
    int dx = 0, dy = 0;
    if (e->x < target->x) dx = 1;
    else if (e->x > target->x) dx = -1;
    if (e->y < target->y) dy = 1;
    else if (e->y > target->y) dy = -1;

    e->x += dx;
    e->y += dy;
    e->traveled++;

    /* 超出最大射程则消失 */
    if (e->traveled > e->max_range) {
        e->active = 0;
        return;
    }

    /* 检查是否命中（到达目标位置或相邻） */
    if (e->x == target->x && e->y == target->y) {
        /* 命中！造成伤害 */
        enemy_take_damage(target, e->damage);

        /* 穿透：寻找下一个目标 */
        if (e->piercing) {
            int i;
            int next_idx = -1;
            int next_dist = 999;
            for (i = 0; i < MAX_ENEMIES; i++) {
                if (i == tidx || !enemies[i].active) continue;
                int d = distance(e->x, e->y, enemies[i].x, enemies[i].y);
                if (d < next_dist) {
                    next_dist = d;
                    next_idx = i;
                }
            }
            if (next_idx >= 0 && next_dist <= e->max_range - e->traveled) {
                e->target_idx = next_idx;
                /* 重置部分射程用于继续飞行 */
                return;  /* 继续飞行 */
            }
        }

        /* AoE范围伤害 */
        if (e->aoe_radius > 0) {
            int i;
            for (i = 0; i < MAX_ENEMIES; i++) {
                if (!enemies[i].active) continue;
                if (distance(e->x, e->y, enemies[i].x, enemies[i].y) <= e->aoe_radius) {
                    enemy_take_damage(&enemies[i], e->damage / 2 + 1);
                }
            }
        }

        e->active = 0;  /* 弹幕消失 */
    }
}

/* ============ 劈砍：倒计时消失 ============ */

static void slash_update(VisualEffect *e) {
    e->lifetime--;
    if (e->lifetime <= 0) {
        e->active = 0;
    } else {
        e->symbol = slash_rotate_symbol(e->lifetime, e->max_lifetime);
    }
}

/* ============ 主更新函数 ============ */

void effects_update(VisualEffect effects[], Enemy enemies[], Player *p) {
    int i;
    (void)p;
    for (i = 0; i < MAX_EFFECTS; i++) {
        if (!effects[i].active) continue;

        switch (effects[i].type) {
            case EFX_PROJECTILE:
                projectile_update(&effects[i], enemies);
                break;
            case EFX_SLASH:
                slash_update(&effects[i]);
                break;
            default:
                break;
        }
    }
}
