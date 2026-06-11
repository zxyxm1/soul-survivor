/**
 * data.c - 存档系统：高分记录读写
 * Soul Survivor: 地下生存
 * 使用二进制文件保存游戏记录
 */
#include "game.h"

void save_load(SaveData *sd) {
    FILE *fp = fopen(SAVE_FILE, "rb");
    if (fp == NULL) {
        /* 文件不存在，使用默认值 */
        sd->high_score = 0;
        sd->total_plays = 0;
        sd->total_kills = 0;
        strcpy(sd->player_name, "DETERMINED");
        return;
    }

    /* 读取存档数据 */
    size_t read = fread(sd, sizeof(SaveData), 1, fp);
    if (read != 1) {
        /* 读取失败，使用默认值 */
        sd->high_score = 0;
        sd->total_plays = 0;
        sd->total_kills = 0;
        strcpy(sd->player_name, "DETERMINED");
    }

    fclose(fp);
}

void save_write(SaveData *sd) {
    FILE *fp = fopen(SAVE_FILE, "wb");
    if (fp == NULL) {
        /* 无法写入文件 */
        return;
    }

    fwrite(sd, sizeof(SaveData), 1, fp);
    fclose(fp);
}

void save_update_high_score(SaveData *sd, int score, int kills) {
    sd->total_plays++;
    sd->total_kills += kills;
    if (score > sd->high_score) {
        sd->high_score = score;
    }
    save_write(sd);
}
