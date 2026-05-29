#include "includes/leaderboard.h"
#include <stdio.h>


static LeaderboardEntry entries[MAX_SCORES];
static int num_scores = 0;

void leaderboard_init() {
    FILE *file = fopen(SCORE_FILE, "r");
    if (file == NULL) {
        // Ficheiro não existe ainda, começa vazio.
        num_scores = 0;
        return; 
    }

    num_scores = 0;
    // Lemos linha a linha (Pontuação, Dia, Mês, Ano, Horas, Minutos, Segundos)
    while (num_scores < MAX_SCORES && fscanf(file, "%d %hhu %hhu %hhu %hhu %hhu %hhu",
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

// Função privada: O teu colega não precisa de saber que isto existe.
// Guarda o estado atual do array de volta no ficheiro de texto.
static void leaderboard_save() {
    FILE *file = fopen(SCORE_FILE, "w");
    if (file == NULL) {
        printf("Erro fatal: Nao foi possivel escrever os scores!\n");
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
    // 1. Procurar em que posição este novo score deve entrar
    int pos = 0;
    while (pos < num_scores && entries[pos].score >= score) {
        pos++;
    }

    // 2. Se a posição for fora do Top (ex: 6º lugar) e a lista já está cheia, ignoramos
    if (pos >= MAX_SCORES) return;

    // 3. Deslocar os scores piores uma casa para baixo para abrir espaço
    int shift_end = (num_scores < MAX_SCORES) ? num_scores : MAX_SCORES - 1;
    for (int i = shift_end; i > pos; i--) {
        entries[i] = entries[i - 1];
    }

    // 4. Inserir o novo campeão
    entries[pos].score = score;
    entries[pos].date = current_time;

    if (num_scores < MAX_SCORES) {
        num_scores++;
    }

    // 5. Guardar as alterações permanentemente
    leaderboard_save();
}

LeaderboardEntry* leaderboard_get_scores() {
    return entries;
}

int leaderboard_get_count() {
    return num_scores;
}