#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "beatmap_loader.h"

static void write_log_loader(const char *format, ...) {
  FILE *fp = fopen("/tmp/log.txt", "a");
  if (fp != NULL) {
    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);
  }
}

/* -----------------------------------------------------------------------
 * beatmap_load
 *
 * Le o ficheiro linha a linha:
 *   - Ignora linhas vazias e comentarios (inicio com '#')
 *   - Parseia "MINIX_TICK LANE" com sscanf
 *   - Para quando o array esta cheio (BEATMAP_MAX_NOTES)
 * ----------------------------------------------------------------------- */
int beatmap_load(const char *path, BeatmapEntry *out_notes, int *out_count) {
  if (path == NULL || out_notes == NULL || out_count == NULL) return 1;

  *out_count = 0;

  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    write_log_loader("[DEBUG] Erro ao abrir ficheiro %s\n", path);
    return 1;
  }

  char line[64];
  int  count = 0;

  while (fgets(line, sizeof(line), fp) != NULL) {
    /* Ignorar comentarios e linhas vazias */
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

    uint32_t tick;
    int      lane;

    if (sscanf(line, "%u %d", &tick, &lane) != 2) continue;
    if (lane < 0 || lane > 4)                     continue;
    if (count >= BEATMAP_MAX_NOTES)               break;

    out_notes[count].spawn_tick = tick;
    out_notes[count].lane       = (uint8_t)lane;
    out_notes[count].spawned    = false;
    count++;
  }

  fclose(fp);
  *out_count = count;

  write_log_loader("[BEATMAP] SUCESSO: Carregadas %d notas de '%s'\n", count, path);
  return 0;
}
