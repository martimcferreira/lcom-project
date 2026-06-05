/**
 * @file beatmap_loader.h
 * @brief Parser dos ficheiros de beatmap (.txt).
 *
 * Lê a sequência de notas de uma música a partir do txt.
 * 
 * @defgroup BeatmapLoader Beatmap Loader
 * @ingroup Core
 * @brief Loader das notas.
 * @{
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/** @brief Tamanho máximo de notas que o loader suporta por música. */
#define BEATMAP_MAX_NOTES 2048

/**
 * @brief Estrutura duma nota no ficheiro (momento em que spawna).
 */
typedef struct {
  uint32_t spawn_tick; /**< @brief Tick (a 60Hz) em que a nota aparece. */
  uint8_t  lane;       /**< @brief Pista (0 a 4). */
  bool     spawned;    /**< @brief True se já deu spawn. */
} BeatmapEntry;

/**
 * @brief Carrega um beatmap a partir de um ficheiro de texto.
 *
 * Lê um ficheiro de texto (onde cada linha válida contém o tick e a pista) e preenche
 * o array passado como argumento, para ser depois processado ao longo do jogo.
 *
 * @param path Caminho absoluto para o ficheiro .txt com o mapa.
 * @param out_notes Array destino que armazenará a sequência de notas (deve suportar BEATMAP_MAX_NOTES).
 * @param out_count Apontador onde será guardado o número total de notas lidas com sucesso.
 * @return int Retorna 0 em caso de sucesso, ou não-zero caso o ficheiro não exista ou tenha formato inválido.
 */
int beatmap_load(const char *path, BeatmapEntry *out_notes, int *out_count);

/** @} */
