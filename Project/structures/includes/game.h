/**
 * @file game.h
 * @brief Funções e estruturas relacionadas com a lógica de jogo.
 *
 * Lida com o processamento das notas (criação, queda e verificação de hits/misses).
 * 
 * @defgroup Game Game Logic
 * @ingroup Core
 * @brief Módulo responsável por gerir o estado principal do jogo rítmico.
 * @{
 */

#pragma once
#include <stdbool.h>

/**
 * @brief Estrutura que representa uma nota no ecrã.
 */
typedef struct {
    int x;         /**< @brief Posição horizontal da nota no ecrã. */
    int y;         /**< @brief Posição vertical atual da nota. */
    int speed;     /**< @brief Velocidade de descida da nota. */
    bool active;   /**< @brief Indica se a nota está a cair e visível no ecrã. */
    bool missed;   /**< @brief Indica se o jogador falhou a nota (passou do hit zone sem ser atingida). */
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
 * @brief Inicializa o array de notas, desativando-as a todas no início da partida.
 */
void init_notes();

/**
 * @brief Atualiza a posição de todas as notas ativas.
 * 
 * Faz as notas descerem no ecrã de acordo com a sua velocidade.
 * Também verifica se alguma nota passou a HIT_ZONE sem ser tocada, contabilizando como um "miss".
 * 
 * @return int Número de notas que passaram o limite inferior (misses) neste frame.
 */
int update_notes();

/** @} */
