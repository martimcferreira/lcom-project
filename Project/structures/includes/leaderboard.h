#pragma once

#include <lcom/lcf.h>
#include "rtc.h"

#define MAX_SCORES 10
#define LEADERBOARD_USERNAME_MAX 16

/*
 * Ficheiro principal usado quando o jogo é executado a partir da pasta Project.
 * Se esse caminho não estiver disponível, o módulo usa /tmp/guitar_hero_scores.txt
 * como fallback para não rebentar só porque o diretório mudou. Tecnologia: incrível.
 */
#define SCORE_FILE "scores.txt"
#define SCORE_FILE_FALLBACK "/tmp/guitar_hero_scores.txt"

typedef struct {
    char username[LEADERBOARD_USERNAME_MAX];
    int score;
    rtc_timestamp date;
} LeaderboardEntry;

void leaderboard_init(void);
void leaderboard_add_score(const char *username, int score, rtc_timestamp current_time);
LeaderboardEntry* leaderboard_get_scores(void);
int leaderboard_get_count(void);
