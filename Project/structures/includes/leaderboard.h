/**
 * @file leaderboard.h
 * @brief Funções e estruturas para gestão da leaderboard.
 *
 * Este ficheiro contém as definições para guardar, carregar e apresentar
 * as pontuações máximas dos jogadores.
 * 
 * @defgroup Leaderboard Leaderboard
 * @ingroup Core
 * @brief Módulo responsável pela gestão das pontuações dos jogadores.
 * @{
 */

#pragma once

#include <lcom/lcf.h>
#include "rtc.h"

/** @brief Número máximo de pontuações guardadas na leaderboard. */
#define MAX_SCORES 10

/** @brief Tamanho máximo do nome de utilizador. */
#define LEADERBOARD_USERNAME_MAX 16

/** @brief Caminho principal para guardar o ficheiro de pontuações. */
#define SCORE_FILE "scores.txt"

/** @brief Caminho de fallback caso não seja possível escrever no diretório atual. */
#define SCORE_FILE_FALLBACK "/tmp/guitar_hero_scores.txt"

/**
 * @brief Estrutura que representa uma entrada na leaderboard.
 */
typedef struct {
    char username[LEADERBOARD_USERNAME_MAX]; /**< @brief Nome do jogador. */
    int score;                               /**< @brief Pontuação obtida. */
    int progress;                            /**< @brief Progresso na música (0 a 100%). */
    rtc_timestamp date;                      /**< @brief Data e hora em que a pontuação foi atingida. */
    int song_id;                             /**< @brief ID da música tocada. */
} LeaderboardEntry;

/**
 * @brief Inicializa a leaderboard.
 * 
 * Tenta ler o ficheiro de pontuações (principal ou fallback) e preenche a
 * lista de scores em memória.
 */
void leaderboard_init(void);

/**
 * @brief Adiciona uma nova pontuação à leaderboard.
 * 
 * Caso a pontuação seja suficientemente alta, insere a entrada na posição correta,
 * ordenando a leaderboard e descartando pontuações mais baixas se exceder o MAX_SCORES.
 * 
 * @param username Nome do jogador.
 * @param score Pontuação final obtida.
 * @param progress Progresso concluído da música.
 * @param song_id Identificador da música.
 * @param current_time Timestamp do RTC do momento em que o jogo acabou.
 */
void leaderboard_add_score(const char *username, int score, int progress, int song_id, rtc_timestamp current_time);

/**
 * @brief Obtém o array com as pontuações atuais.
 * 
 * @return LeaderboardEntry* Apontador para o início do array de pontuações.
 */
LeaderboardEntry* leaderboard_get_scores(void);

/**
 * @brief Obtém o número total de pontuações atualmente registadas.
 * 
 * @return int Número de scores (entre 0 e MAX_SCORES).
 */
int leaderboard_get_count(void);

/** @} */
