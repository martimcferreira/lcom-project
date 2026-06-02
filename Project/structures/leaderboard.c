#include "leaderboard.h"
#include <stdio.h>

static LeaderboardEntry entries[MAX_SCORES];
static int num_scores = 0;
static const char *active_score_file = SCORE_FILE;

static FILE *open_score_file(const char *mode) {
    FILE *file = fopen(active_score_file, mode);
    if (file != NULL) return file;

    file = fopen(SCORE_FILE, mode);
    if (file != NULL) {
        active_score_file = SCORE_FILE;
        return file;
    }

    file = fopen(SCORE_FILE_FALLBACK, mode);
    if (file != NULL) {
        active_score_file = SCORE_FILE_FALLBACK;
    }
    return file;
}

void leaderboard_init(void) {
    FILE *file = open_score_file("r");
    if (file == NULL) {
        num_scores = 0;
        return;
    }

    num_scores = 0;
    while (num_scores < MAX_SCORES &&
           fscanf(file, "%d %hhu %hhu %hhu %hhu %hhu %hhu",
                  &entries[num_scores].score,
                  &entries[num_scores].date.day,
                  &entries[num_scores].date.month,
                  &entries[num_scores].date.year,
                  &entries[num_scores].date.hours,
                  &entries[num_scores].date.minutes,
                  &entries[num_scores].date.seconds) == 7) {
        num_scores++;
    }

    fclose(file);
}

static void leaderboard_save(void) {
    FILE *file = open_score_file("w");
    if (file == NULL) {
        printf("[LEADERBOARD] Erro: nao foi possivel escrever os scores.\n");
        return;
    }

    for (int i = 0; i < num_scores; i++) {
        fprintf(file, "%d %hhu %hhu %hhu %hhu %hhu %hhu\n",
                entries[i].score,
                entries[i].date.day,
                entries[i].date.month,
                entries[i].date.year,
                entries[i].date.hours,
                entries[i].date.minutes,
                entries[i].date.seconds);
    }

    fclose(file);
}

void leaderboard_add_score(int score, rtc_timestamp current_time) {
    if (score <= 0) return;

    int pos = 0;
    while (pos < num_scores && entries[pos].score >= score) {
        pos++;
    }

    if (pos >= MAX_SCORES) return;

    int shift_end = (num_scores < MAX_SCORES) ? num_scores : MAX_SCORES - 1;
    for (int i = shift_end; i > pos; i--) {
        entries[i] = entries[i - 1];
    }

    entries[pos].score = score;
    entries[pos].date = current_time;

    if (num_scores < MAX_SCORES) {
        num_scores++;
    }

    leaderboard_save();
}

LeaderboardEntry* leaderboard_get_scores(void) {
    return entries;
}

int leaderboard_get_count(void) {
    return num_scores;
}
