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
        "offset":     -1.0,          # Compensar o tempo de viagem da nota (travel time ~1s)
    },
    {
        "chart":      "notes_song2.chart",
        "out":        "song2.txt",
        "bpm":        165,
        "resolution": 192,
        "section":    "MediumSingle",
        "min_tick":   768,
        "min_dist":   96,
        "offset":     -1.0,          # Compensar o tempo de viagem da nota (travel time ~1s)
    },
    {
        "chart":      "notes_song3.chart",
        "out":        "song3.txt",
        "bpm":        111,           # Highway To Hell - AC/DC (SyncTrack: B 111072 = ~111 BPM)
        "resolution": 192,
        "section":    "ExpertSingle",
        "min_tick":   480,           # Primeira nota em 480
        "min_dist":   96,
        "offset":     -1.0,          # Compensar o tempo de viagem da nota (travel time ~1s)
    },
    {
        "chart":      "notes_song4.chart",
        "out":        "song4.txt",
        "bpm":        128,           # Summer - Calvin Harris (~128 BPM)
        "resolution": 192,
        "section":    "MediumSingle",
        "min_tick":   768,
        "min_dist":   96,
        "offset":     0.0,           # O MP3 já tem silêncio de lead-in integrado, não necessita de antecipação
    },
]

# ---------------------------------------------------------------------------
# Caminhos (relativos a este ficheiro)
# ---------------------------------------------------------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR    = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "Project", "beatmaps"))
NOTE_RE = re.compile(r"^\s*(\d+)\s*=\s*N\s*([0-4])\s+0", re.MULTILINE)

# ---------------------------------------------------------------------------
def parse_chart_sync(content, resolution):
    sync_pat = re.compile(r"\[SyncTrack\]\s*\{([^}]*)\}", re.DOTALL)
    m = sync_pat.search(content)
    if not m:
        return [(0, 120000)]
    
    b_events = []
    b_pat = re.compile(r"^\s*(\d+)\s*=\s*B\s+(\d+)", re.MULTILINE)
    for match in b_pat.finditer(m.group(1)):
        tick = int(match.group(1))
        mibpm = int(match.group(2))
        b_events.append((tick, mibpm))
        
    b_events.sort()
    if not b_events or b_events[0][0] != 0:
        first_bpm = b_events[0][1] if b_events else 120000
        b_events.insert(0, (0, first_bpm))
        
    return b_events

def tick_to_seconds(tick, resolution, bpm_events):
    current_time = 0.0
    current_tick = 0
    current_milliBPM = bpm_events[0][1]
    
    for event_tick, event_milliBPM in bpm_events[1:]:
        if event_tick > tick:
            break
        interval_ticks = event_tick - current_tick
        dt = (interval_ticks * 60000.0) / (resolution * current_milliBPM)
        current_time += dt
        current_tick = event_tick
        current_milliBPM = event_milliBPM
        
    if tick > current_tick:
        interval_ticks = tick - current_tick
        dt = (interval_ticks * 60000.0) / (resolution * current_milliBPM)
        current_time += dt
        
    return current_time

# ---------------------------------------------------------------------------
def convert(song: dict) -> None:
    res  = song["resolution"]
    mtk  = song["min_tick"]
    mds  = song["min_dist"]

    chart_path = os.path.join(SCRIPT_DIR, song["chart"])
    print(f"  Lendo  : {song['chart']}")

    with open(chart_path, "r", encoding="utf-8-sig") as f:
        content = f.read()

    # Parse Offset (em segundos)
    offset_match = re.search(r"^\s*Offset\s*=\s*(-?\d+(?:\.\d+)?)", content, re.IGNORECASE | re.MULTILINE)
    file_offset = float(offset_match.group(1)) if offset_match else 0.0
    custom_offset = song.get("offset", 0.0)
    offset = file_offset + custom_offset

    # Parse SyncTrack
    bpm_events = parse_chart_sync(content, res)

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
    
    # Calcular ticks do MINIX (a 60 Hz)
    start_mt = int(round((tick_to_seconds(filtered[0][0], res, bpm_events) + offset) * 60.0))
    end_mt   = int(round((tick_to_seconds(filtered[-1][0], res, bpm_events) + offset) * 60.0))

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(f"# Song:    {song['chart']}\n")
        f.write(f"# Section: [{song['section']}] | Resolution: {res} | Offset: {offset}\n")
        f.write(f"# Notes:   {len(filtered)} (raw: {len(raws)}, density filter: min_dist={mds})\n")
        f.write(f"# Range:   MINIX tick {start_mt} -> {end_mt} (~{end_mt/60:.1f}s at 60Hz)\n")
        f.write("# Format:  MINIX_TICK LANE  (lanes 0=Verde 1=Vermelho 2=Amarelo 3=Azul 4=Laranja)\n")
        f.write("#\n")
        for ct, lane in filtered:
            sec = tick_to_seconds(ct, res, bpm_events) + offset
            mt = int(round(sec * 60.0))
            f.write(f"{mt} {lane}\n")

    print(f"  Saida  : {out_path}")
    print(f"  Notas  : {len(filtered)}  (raw: {len(raws)})")
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
