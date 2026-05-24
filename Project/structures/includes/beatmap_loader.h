#pragma once

#include <stdint.h>
#include <stdbool.h>

/* -----------------------------------------------------------------------
 * Beatmap Loader
 *
 * Carrega um ficheiro de beatmap (.txt) gerado pelo convert_charts.py
 * e preenche um array de BeatmapNote alocado dinamicamente.
 *
 * Formato do ficheiro:
 *   - Linhas que comecam por '#' sao comentarios (ignoradas)
 *   - Restantes linhas: "MINIX_TICK LANE"
 *
 * Exemplo:
 *   # Song: song2.txt
 *   101 1
 *   127 2
 *   ...
 * ----------------------------------------------------------------------- */

/** Tamanho maximo de notas que o loader suporta (ajustar se necessario). */
#define BEATMAP_MAX_NOTES 2048

/** Estrutura de uma nota do beatmap (identica a BeatmapNote em main.c). */
typedef struct {
  uint32_t spawn_tick; /**< Tick MINIX (60 Hz) em que a nota deve surgir. */
  uint8_t  lane;       /**< Pista: 0=Verde 1=Vermelho 2=Amarelo 3=Azul 4=Laranja. */
  bool     spawned;    /**< Controlo interno: true quando ja foi accionada. */
} BeatmapEntry;

/**
 * @brief Carrega um beatmap a partir de um ficheiro de texto.
 *
 * @param path      Caminho absoluto para o ficheiro .txt (no MINIX usa
 *                  /shares/lcom/grupo_2leic02_2/Project/beatmaps/songN.txt).
 * @param out_notes Array de destino (deve ter pelo menos BEATMAP_MAX_NOTES entradas).
 * @param out_count Numero de notas carregadas (saida).
 * @return 0 em sucesso, 1 em erro (ficheiro nao encontrado, formato invalido).
 */
int beatmap_load(const char *path, BeatmapEntry *out_notes, int *out_count);
