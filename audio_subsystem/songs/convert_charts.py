#!/usr/bin/env python3
"""
convert_charts.py  -  Conversor de .chart -> beatmap .txt para MINIX Guitar Hero
================================================================================
Uso:
    python convert_charts.py

Gera os ficheiros em:
    ../../Project/beatmaps/song1.txt
    ../../Project/beatmaps/song2.txt

Formato de saida (cada linha):
    MINIX_TICK LANE
    (linhas com '#' sao comentarios, ignoradas pelo beatmap_loader.c)

Formula de conversao:
    MINIX_tick = round(chart_tick / ((BPM * resolution) / (60 * 60)))
"""

import re
import os

# ---------------------------------------------------------------------------
# Configuracao das musicas
# Adiciona entradas aqui para converter musicas novas.
# ---------------------------------------------------------------------------
SONGS = [
    {
        "chart":      "notes_song1.chart",
        "out":        "song1.txt",
        "bpm":        142,           # BPM principal (da seccao [SyncTrack])
        "resolution": 192,           # Resolution do cabecalho [Song]
        "section":    "ExpertSingle",
        "min_tick":   768,           # Ignorar introducao (ticks < este valor)
        "min_dist":   96,            # Filtro de densidade: distancia minima entre notas
                                     # (96 = 1 colcheia a 142 BPM, evita notas coladas do Expert)
    },
    {
        "chart":      "notes_song2.chart",
        "out":        "song2.txt",
        "bpm":        165,
        "resolution": 192,
        "section":    "MediumSingle",
        "min_tick":   768,
        "min_dist":   96,
    },
]

# ---------------------------------------------------------------------------
# Caminhos (relativos a este ficheiro)
# ---------------------------------------------------------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR    = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "Project", "beatmaps"))

NOTE_RE = re.compile(r"^\s*(\d+)\s*=\s*N\s*([0-4])\s+0", re.MULTILINE)

# ---------------------------------------------------------------------------
def convert(song: dict) -> None:
    bpm  = song["bpm"]
    res  = song["resolution"]
    cf   = (bpm * res) / (60.0 * 60.0)   # chart ticks per MINIX tick
    mtk  = song["min_tick"]
    mds  = song["min_dist"]

    chart_path = os.path.join(SCRIPT_DIR, song["chart"])
    print(f"  Lendo  : {song['chart']}")

    with open(chart_path, "r", encoding="utf-8-sig") as f:
        content = f.read()

    # Extrair seccao de notas
    pat = re.compile(r"\[" + re.escape(song["section"]) + r"\]\s*\{([^}]*)\}", re.DOTALL)
    m   = pat.search(content)
    if not m:
        print(f"  ERRO   : seccao [{song['section']}] nao encontrada!")
        return

    # Parsear notas lane 0-4 (lane 5/6 sao flags HOPO/open, ignorar)
    raws = [(int(x.group(1)), int(x.group(2)))
            for x in NOTE_RE.finditer(m.group(1))
            if int(x.group(1)) >= mtk]
    raws.sort()

    # Filtro de densidade: garante separacao minima entre notas consecutivas
    filtered, last = [], -mds
    for ct, lane in raws:
        if ct - last >= mds:
            filtered.append((ct, lane))
            last = ct

    if not filtered:
        print(f"  AVISO  : nenhuma nota encontrada apos filtros!")
        return

    # Escrever ficheiro de saida
    os.makedirs(OUT_DIR, exist_ok=True)
    out_path   = os.path.join(OUT_DIR, song["out"])
    start_mt   = int(round(filtered[0][0]  / cf))
    end_mt     = int(round(filtered[-1][0] / cf))

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(f"# Song:    {song['chart']}\n")
        f.write(f"# Section: [{song['section']}] | BPM: {bpm} | Resolution: {res}\n")
        f.write(f"# Factor:  (BPM*res)/(60*60) = ({bpm}*{res})/(3600) = {cf:.6f}\n")
        f.write(f"# Notes:   {len(filtered)} (raw: {len(raws)}, density filter: min_dist={mds})\n")
        f.write(f"# Range:   MINIX tick {start_mt} -> {end_mt} (~{end_mt/60:.1f}s at 60Hz)\n")
        f.write("# Format:  MINIX_TICK LANE  (lanes 0=Verde 1=Vermelho 2=Amarelo 3=Azul 4=Laranja)\n")
        f.write("#\n")
        for ct, lane in filtered:
            mt = int(round(ct / cf))
            f.write(f"{mt} {lane}\n")

    print(f"  Saida  : {out_path}")
    print(f"  Notas  : {len(filtered)}  (raw: {len(raws)})  |  factor: {cf:.6f}")
    print(f"  Duracao: ~{end_mt/60:.1f}s  (MINIX ticks {start_mt} -> {end_mt})")

# ---------------------------------------------------------------------------
if __name__ == "__main__":
    print("=" * 60)
    print("  Guitar Hero MINIX - Conversor de Beatmaps")
    print(f"  Destino: {OUT_DIR}")
    print("=" * 60)
    for song in SONGS:
        print()
        convert(song)
    print()
    print("Conversao concluida.")
