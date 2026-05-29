#pragma once

#include <lcom/lcf.h>
#include "rtc.h" // Precisa de estar na mesma pasta para usar a rtc_timestamp

#define MAX_SCORES 5
// AVISO: Muda este caminho para a diretoria real do vosso projeto no MINIX
#define SCORE_FILE "/home/lcom/labs/Project/structures/scores.txt"

// A estrutura que guarda um recorde
typedef struct {
    int score;
    rtc_timestamp date;
} LeaderboardEntry;

/**
 * Lê o ficheiro de texto e carrega os scores para a memória.
 * DEVE ser chamada apenas uma vez no início do jogo (ex: no proj_main_loop).
 */
void leaderboard_init();

/**
 * Tenta adicionar um score. Se for suficientemente alto, entra no Top 5, 
 * empurra os piores para baixo, e guarda automaticamente no ficheiro de texto.
 */
void leaderboard_add_score(int score, rtc_timestamp current_time);

/**
 * Retorna o array com os scores (já ordenados do maior para o menor).
 * O teu colega usa isto para desenhar o menu.
 */
LeaderboardEntry* leaderboard_get_scores();

/**
 * Retorna quantos scores existem atualmente guardados (entre 0 e MAX_SCORES).
 */
int leaderboard_get_count();