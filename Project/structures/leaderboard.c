#include "leaderboard.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

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

static void leaderboard_set_username(LeaderboardEntry *entry, const char *username) {
    if (entry == NULL) return;

    if (username == NULL || username[0] == '\0') {
        strncpy(entry->username, "PLAYER", LEADERBOARD_USERNAME_MAX);
    } else {
        strncpy(entry->username, username, LEADERBOARD_USERNAME_MAX);
    }

    entry->username[LEADERBOARD_USERNAME_MAX - 1] = '\0';
}

static bool parse_score_line(const char *line, LeaderboardEntry *entry) {
    char username[LEADERBOARD_USERNAME_MAX];
    int score, progress;
    unsigned int day, month, year, hours, minutes, seconds;

    if (line == NULL || entry == NULL) return false;

    /* Newest format: USERNAME SCORE PROGRESS DD MM YY HH MM SS */
    if (sscanf(line, "%15s %d %d %u %u %u %u %u %u",
               username,
               &score,
               &progress,
               &day,
               &month,
               &year,
               &hours,
               &minutes,
               &seconds) == 9) {
        leaderboard_set_username(entry, username);
        entry->score = score;
        entry->progress = progress;
        entry->date.day = (uint8_t) day;
        entry->date.month = (uint8_t) month;
        entry->date.year = (uint8_t) year;
        entry->date.hours = (uint8_t) hours;
        entry->date.minutes = (uint8_t) minutes;
        entry->date.seconds = (uint8_t) seconds;
        return true;
    }

    /* Old format: USERNAME SCORE DD MM YY HH MM SS */
    if (sscanf(line, "%15s %d %u %u %u %u %u %u",
               username,
               &score,
               &day,
               &month,
               &year,
               &hours,
               &minutes,
               &seconds) == 8) {
        leaderboard_set_username(entry, username);
        entry->score = score;
        entry->progress = 100;
        entry->date.day = (uint8_t) day;
        entry->date.month = (uint8_t) month;
        entry->date.year = (uint8_t) year;
        entry->date.hours = (uint8_t) hours;
        entry->date.minutes = (uint8_t) minutes;
        entry->date.seconds = (uint8_t) seconds;
        return true;
    }

    /* Backwards-compatible oldest format: SCORE DD MM YY HH MM SS */
    if (sscanf(line, "%d %u %u %u %u %u %u",
               &score,
               &day,
               &month,
               &year,
               &hours,
               &minutes,
               &seconds) == 7) {
        leaderboard_set_username(entry, "PLAYER");
        entry->score = score;
        entry->progress = 100;
        entry->date.day = (uint8_t) day;
        entry->date.month = (uint8_t) month;
        entry->date.year = (uint8_t) year;
        entry->date.hours = (uint8_t) hours;
        entry->date.minutes = (uint8_t) minutes;
        entry->date.seconds = (uint8_t) seconds;
        return true;
    }

    return false;
}

void leaderboard_init(void) {
    FILE *file = open_score_file("r");
    char line[96];

    if (file == NULL) {
        num_scores = 0;
        return;
    }

    num_scores = 0;
    while (num_scores < MAX_SCORES && fgets(line, sizeof(line), file) != NULL) {
        if (parse_score_line(line, &entries[num_scores])) {
            num_scores++;
        }
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
        fprintf(file, "%s %d %d %u %u %u %u %u %u\n",
                entries[i].username,
                entries[i].score,
                entries[i].progress,
                (unsigned int) entries[i].date.day,
                (unsigned int) entries[i].date.month,
                (unsigned int) entries[i].date.year,
                (unsigned int) entries[i].date.hours,
                (unsigned int) entries[i].date.minutes,
                (unsigned int) entries[i].date.seconds);
    }

    fclose(file);
}

void leaderboard_add_score(const char *username, int score, int progress, rtc_timestamp current_time) {
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

    leaderboard_set_username(&entries[pos], username);
    entries[pos].score = score;
    entries[pos].progress = progress;
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
