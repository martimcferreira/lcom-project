/**
 * @file game.h
 * @brief Lógica de jogo e notas.
 *
 * Atualiza e processa as notas a cair no ecrã.
 * 
 * @defgroup Game Game Logic
 * @ingroup Core
 * @brief Core do jogo.
 * @{
 */

#pragma once
#include <stdbool.h>

/**
 * @brief Estrutura duma nota no ecrã.
 */
typedef struct {
    int x;         /**< @brief Posição X da nota. */
    int y;         /**< @brief Posição Y da nota. */
    int speed;     /**< @brief Velocidade. */
    bool active;   /**< @brief True se a nota estiver a ser mostrada. */
    bool missed;   /**< @brief True se o jogador falhou. */
} Note;

/** @brief Número máximo de notas que podem estar ativas em simultâneo. */
#define MAX_NOTES 100

/** @brief Coordenada X inicial da primeira pista (lane). */
#define LANE_BASE_X 200

/** @brief Largura de cada pista (distância entre notas de pistas adjacentes). */
#define LANE_WIDTH 80

/** @brief Número total de pistas no jogo (A, S, D, F, G). */
#define NUM_LANES 5

/** @brief Limite superior da zona de hit (onde as notas devem ser tocadas). */
#define HIT_ZONE_TOP 490

/** @brief Limite inferior da zona de hit. */
#define HIT_ZONE_BOTTOM 530

/** @brief Altura efetiva do sprite da nota (usada nas colisões). */
#define NOTE_HIT_HEIGHT 60

/**
 * @brief Inicializa (limpa) o array de notas.
 */
void init_notes();

/**
 * @brief Dá update à posição de todas as notas a cair.
 * 
 * Move as notas para baixo e verifica se o jogador deixou passar alguma (miss).
 * 
 * @return int Número de misses neste frame.
 */
int update_notes();

/** @} */
