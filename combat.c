/**
 * combat.c - 战斗系统：自动攻击、伤害计算、升级选项、特殊技能
 * Soul Survivor: 地下生存
 */
#include "game.h"

/* 外部引用 */
extern Weapon weapon_library[MAX_WEAPONS];

int distance(int x1, int y1, int x2, int y2) {
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
    /* 使用切比雪夫距离（8方向格子距离） */
    return (dx > dy) ? dx : dy;
}

int calc_damage(int atk, int def) {
    int dmg = atk - def;
    return (dmg < 1) ? 1 : dmg;
}

/* ============ 战斗主逻辑 ============ */

void combat_tick(Player *p, Enemy enemies[], VisualEffect effects[]) {
    int i;

    /* 每个武器独立攻击 */
    for (i = 0; i < p->weapon_count; i++) {
        int wid = p->weapons[i];
        if (wid < 0 || wid >= WEAPON_COUNT) continue;

        /* 检查冷却 */
        if (p->weapon_cooldowns[i] > 0) continue;

        /* 执行自动攻击 */
        auto_attack(p, enemies, i, effects);

        /* 设置冷却 */
        p->weapon_cooldowns[i] = weapon_library[wid].cooldown;
    }
}

/**
 * 判断武器是否为远程（需要弹幕）
 */
static int is_ranged_weapon(int wid) {
    return (wid == WEAPON_BONE || wid == WEAPON_LIGHTNING);
}

/**
 * 自动攻击：找到最近的目标并造成伤害
 * - 近战武器：瞬间伤害 + 劈砍动画
 * - 远程武器：生成弹幕飞行到目标后造成伤害
 */
void auto_attack(Player *p, Enemy enemies[], int weapon_slot, VisualEffect effects[]) {
    int wid = p->weapons[weapon_slot];
    if (wid < 0 || wid >= WEAPON_COUNT) return;

    Weapon *wp = &weapon_library[wid];
    int range = wp->range;
    int targets = wp->target_count;
    int dmg = calc_damage(p->atk + wp->damage, 0);
    int aoe = wp->aoe_radius;
    int is_ranged = is_ranged_weapon(wid);

    /* 收集范围内的敌人索引 */
    int targets_in_range[MAX_ENEMIES];
    int target_count = 0;
    int i;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        int dist = distance(p->x, p->y, enemies[i].x, enemies[i].y);
        if (dist <= range) {
            targets_in_range[target_count++] = i;
        }
    }

    if (target_count == 0) return;

    /* 按距离排序（冒泡排序，够用） */
    int j;
    for (i = 0; i < target_count - 1; i++) {
        for (j = i + 1; j < target_count; j++) {
            int di = distance(p->x, p->y, enemies[targets_in_range[i]].x, enemies[targets_in_range[i]].y);
            int dj = distance(p->x, p->y, enemies[targets_in_range[j]].x, enemies[targets_in_range[j]].y);
            if (dj < di) {
                int tmp = targets_in_range[i];
                targets_in_range[i] = targets_in_range[j];
                targets_in_range[j] = tmp;
            }
        }
    }

    if (is_ranged) {
        /* ====== 远程武器：生成弹幕 ====== */
        int speed, proj_symbol;
        const char *proj_color;

        switch (wid) {
            case WEAPON_BONE:
                speed = 2;
                proj_symbol = '|';
                proj_color = WHITE;
                break;
            case WEAPON_LIGHTNING:
                speed = 1;
                proj_symbol = '~';
                proj_color = YELLOW;
                break;
            default:
                speed = 2;
                proj_symbol = '~';
                proj_color = WHITE;
                break;
        }

        int fired = 0;
        for (i = 0; i < target_count && fired < targets; i++) {
            int idx = targets_in_range[i];
            effects_spawn_projectile(effects, p->x, p->y, idx,
                dmg, wp->piercing, aoe, speed, range + 2,
                proj_symbol, proj_color);
            fired++;
        }

        /* 远程攻击闪烁：出招瞬间玩家变亮 */
        p->attack_flash = 2;
    } else {
        /* ====== 近战武器：瞬间伤害 + 劈砍动画 ====== */
        int slash_duration;
        const char *slash_color;

        switch (wid) {
            case WEAPON_KNIFE:
                slash_duration = 4;
                slash_color = CYAN;
                break;
            case WEAPON_FIRE:
                slash_duration = 6;
                slash_color = YELLOW;
                break;
            default: /* STICK */
                slash_duration = 5;
                slash_color = CYAN;
                break;
        }

        /* 在第一个目标位置生成劈砍动画 + 火花爆发 */
        if (target_count > 0) {
            int slash_tidx = targets_in_range[0];
            effects_spawn_slash(effects, enemies[slash_tidx].x, enemies[slash_tidx].y,
                slash_duration, slash_color);
        }

        /* 如果火焰有AoE，在最近目标周围额外生成红色爆发 */
        if (aoe > 0 && target_count > 0) {
            int tidx = targets_in_range[0];
            effects_spawn_slash(effects, enemies[tidx].x, enemies[tidx].y, slash_duration, RED);
        }

        /* 近战攻击闪烁：持续时间稍长 */
        p->attack_flash = 3;

        /* 攻击最近的N个目标（瞬间伤害） */
        int attacked = 0;
        for (i = 0; i < target_count && attacked < targets; i++) {
            int idx = targets_in_range[i];
            Enemy *e = &enemies[idx];

            enemy_take_damage(e, dmg);

            /* AoE范围伤害 */
            if (aoe > 0) {
                int k;
                for (k = 0; k < MAX_ENEMIES; k++) {
                    if (!enemies[k].active || k == idx) continue;
                    if (distance(e->x, e->y, enemies[k].x, enemies[k].y) <= aoe) {
                        enemy_take_damage(&enemies[k], dmg / 2 + 1);
                    }
                }
            }

            attacked++;

            /* 检查击杀 */
            if (enemy_is_dead(e)) {
                e->active = 0;
                p->kills++;
                player_gain_exp(p, e->exp_reward);
            }
        }
    }

    /* 清理已死亡敌人 */
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemy_is_dead(&enemies[i])) {
            enemies[i].active = 0;
            p->kills++;
            player_gain_exp(p, enemies[i].exp_reward);
        }
    }
}

/* ============ 经验收集 ============ */

void collect_exp(Player *p, Enemy enemies[]) {
    int i;
    /* 磁铁效果：在磁铁范围内的经验自动收集 */
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) continue;
        if (enemies[i].exp_reward <= 0) continue;

        /* 如果玩家在磁铁范围内 */
        int dist = abs(p->x - enemies[i].x) + abs(p->y - enemies[i].y);
        if (dist <= p->magnet_range) {
            player_gain_exp(p, enemies[i].exp_reward);
            enemies[i].exp_reward = 0;
        }
    }
}

/* ============ 升级系统 ============ */

/**
 * 生成3个随机升级选项
 */
UpgradeOption* generate_upgrades(Player *p, int *count) {
    static UpgradeOption pool[10];
    static UpgradeOption results[3];
    int pool_size = 0;
    int i;

    /* 构建可用升级池 */
    /* 1. 属性升级（总是可用） */
    pool[pool_size++] = (UpgradeOption){UPGRADE_ATK,    "攻击力+3",    "提升攻击力3点",     3, 0};
    pool[pool_size++] = (UpgradeOption){UPGRADE_SPD,    "速度+1",      "移动速度提升",      1, 0};
    pool[pool_size++] = (UpgradeOption){UPGRADE_MAXHP,  "最大HP+8",   "最大生命值提升8点",  8, 0};
    pool[pool_size++] = (UpgradeOption){UPGRADE_DEF,    "防御力+2",   "减少受到的伤害",     2, 0};
    if (p->hp < p->max_hp) {
        pool[pool_size++] = (UpgradeOption){UPGRADE_HEAL,   "治疗",       "恢复10点HP",       10, 0};
    }

    /* 2. 武器升级（只提供未拥有的） */
    if (p->weapon_count < MAX_PLAYER_WEAPONS) {
        int has_bone = 0, has_fire = 0, has_knife = 0, has_lightning = 0;
        for (i = 0; i < p->weapon_count; i++) {
            switch (p->weapons[i]) {
                case WEAPON_BONE:      has_bone = 1;      break;
                case WEAPON_FIRE:      has_fire = 1;       break;
                case WEAPON_KNIFE:     has_knife = 1;      break;
                case WEAPON_LIGHTNING: has_lightning = 1;  break;
                default: break;
            }
        }
        if (!has_bone)      pool[pool_size++] = (UpgradeOption){UPGRADE_WEAPON, "骨头攻击", "穿透直线伤害", 0, WEAPON_BONE};
        if (!has_fire)      pool[pool_size++] = (UpgradeOption){UPGRADE_WEAPON, "火焰魔法", "范围灼烧伤害", 0, WEAPON_FIRE};
        if (!has_knife)     pool[pool_size++] = (UpgradeOption){UPGRADE_WEAPON, "小刀",     "快速近战攻击", 0, WEAPON_KNIFE};
        if (!has_lightning) pool[pool_size++] = (UpgradeOption){UPGRADE_WEAPON, "闪电",     "连锁多目标",   0, WEAPON_LIGHTNING};
    }

    /* 3. 特殊升级 */
    pool[pool_size++] = (UpgradeOption){UPGRADE_RANGE,  "攻击范围+1", "扩大武器攻击范围", 1, 0};
    pool[pool_size++] = (UpgradeOption){UPGRADE_EXPBOOST, "经验加成", "经验获取+25%",     25, 0};
    pool[pool_size++] = (UpgradeOption){UPGRADE_MAGNET, "磁铁效果",   "扩大经验拾取范围",  2, 0};

    /* 决心升级（限制次数） */
    if (p->determination < 2) {
        pool[pool_size++] = (UpgradeOption){UPGRADE_DETERMINATION, "决心+1", "死亡时自动复活一次", 1, 0};
    }

    /* 随机抽取3个不重复的选项 */
    *count = 3;
    if (pool_size <= 3) {
        memcpy(results, pool, sizeof(UpgradeOption) * pool_size);
        *count = pool_size;
    } else {
        /* Fisher-Yates 洗牌前3个 */
        for (i = 0; i < 3; i++) {
            int r = i + rand() % (pool_size - i);
            UpgradeOption tmp = pool[i];
            pool[i] = pool[r];
            pool[r] = tmp;
        }
        memcpy(results, pool, sizeof(UpgradeOption) * 3);
    }

    return results;
}

/**
 * 应用升级效果
 */
void apply_upgrade(Player *p, UpgradeOption *opt) {
    switch (opt->type) {
        case UPGRADE_ATK:
            p->atk += opt->value1;
            break;

        case UPGRADE_SPD:
            p->spd -= opt->value1;
            if (p->spd < 1) p->spd = 1;  /* 最快每帧移动 */
            break;

        case UPGRADE_MAXHP:
            p->max_hp += opt->value1;
            p->hp += opt->value1;  /* 同时治疗 */
            break;

        case UPGRADE_DEF:
            p->def += opt->value1;
            break;

        case UPGRADE_HEAL:
            p->hp += opt->value1;
            if (p->hp > p->max_hp) p->hp = p->max_hp;
            break;

        case UPGRADE_WEAPON:
            player_add_weapon(p, opt->value2);
            break;

        case UPGRADE_RANGE:
            /* 所有武器范围+1 */
            {
                int i;
                for (i = 0; i < WEAPON_COUNT; i++) {
                    weapon_library[i].range += opt->value1;
                }
            }
            break;

        case UPGRADE_EXPBOOST:
            p->exp_boost += opt->value1;  /* +25 */
            break;

        case UPGRADE_DETERMINATION:
            p->determination += opt->value1;
            break;

        case UPGRADE_MAGNET:
            p->magnet_range += opt->value1;
            break;

        default:
            break;
    }
}

/* ============ 特殊技能：闪避 ============ */

void player_use_special(Player *p, Enemy enemies[]) {
    if (p->special_cooldown > 0) return;

    /* 闪避：向面朝方向冲刺3格，并对路径上的敌人造成伤害 */
    int dx = 0, dy = 0;
    /* 寻找最近敌人作为冲刺方向 */
    int closest_dist = 999;
    int closest_idx = -1;
    int i;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        int dist = distance(p->x, p->y, enemies[i].x, enemies[i].y);
        if (dist < closest_dist) {
            closest_dist = dist;
            closest_idx = i;
        }
    }

    if (closest_idx >= 0) {
        /* 远离最近敌人的方向 */
        dx = (p->x > enemies[closest_idx].x) ? 1 : -1;
        dy = (p->y > enemies[closest_idx].y) ? 1 : -1;
    } else {
        dx = 1; /* 默认向右 */
    }

    /* 冲刺3格 */
    int steps;
    for (steps = 0; steps < 3; steps++) {
        int nx = p->x + dx;
        int ny = p->y + dy;
        if (nx >= 1 && nx < ARENA_W - 1 && ny >= 1 && ny < ARENA_H - 1) {
            p->x = nx;
            p->y = ny;
        }

        /* 对路径上的敌人造成伤害 */
        for (i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) continue;
            if (enemies[i].x == p->x && enemies[i].y == p->y) {
                enemy_take_damage(&enemies[i], p->atk * 2);
                if (enemy_is_dead(&enemies[i])) {
                    enemies[i].active = 0;
                    p->kills++;
                    player_gain_exp(p, enemies[i].exp_reward);
                }
            }
        }
    }

    p->special_cooldown = p->special_max_cd;
    p->invincible = 6;  /* 闪避过程中短暂无敌 */
}
