/**
 * @file beatmap_loader.h
 * @brief Lida com a leitura e processamento de ficheiros de beatmap.
 *
 * Carrega a sequência de notas musicais a partir de ficheiros de texto para
 * sincronizar as notas geradas com a música reproduzida.
 * 
 * @defgroup BeatmapLoader Beatmap Loader
 * @ingroup Core
 * @brief Módulo responsável por carregar os mapas rítmicos das músicas.
 * @{
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/** @brief Tamanho máximo de notas que o loader suporta por música. */
#define BEATMAP_MAX_NOTES 2048

/**
 * @brief Estrutura que representa o momento em que uma nota deve ser criada ("spawn").
 */
typedef struct {
  uint32_t spawn_tick; /**< @brief Tick MINIX (a 60 Hz) em que a nota deve começar a cair. */
  uint8_t  lane;       /**< @brief Pista (0=Verde, 1=Vermelha, 2=Azul, 3=Roxa, 4=Amarela). */
  bool     spawned;    /**< @brief Controlo interno: true quando a nota já foi criada no ecrã. */
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
