import argparse
import math
import os
import serial
import pygame
import sys
from array import array
import time

# -----------------------------------------------------------------------------
# Protocolo simples recebido do MINIX pela UART:
#   0x01 -> início do jogo: tocar música
#   0x0A -> acerto: só regista/printa o evento
#   0x0E -> erro/miss: toca efeito de falha/desafinação
# -----------------------------------------------------------------------------
DEFAULT_SERIAL_PORT = "COM6"
BAUD_RATE = 115200
DEFAULT_SONG = os.path.join("songs", "song2.mp3")  # main.c está com song_id = 2
DEFAULT_FAIL_WAV = os.path.join("fx", "fail.wav")

EVENT_GAME_START_SONG1 = 0x01
EVENT_GAME_START_SONG2 = 0x02
EVENT_GAME_END = 0x03
EVENT_HIT = 0x0A
EVENT_MISS = 0x0E

SONGS = {
    1: os.path.join("songs", "song1.mp3"),
    2: os.path.join("songs", "song2.mp3")
}




def make_detune_sound(duration=0.35, sample_rate=44100):
    """Cria um efeito curto de 'desafinação' caso não exista um .wav."""
    samples = array("h")
    total = int(duration * sample_rate)

    for i in range(total):
        t = i / sample_rate
        # Frequência a descer, para soar como uma nota a morrer/desafinar.
        freq = 420 - 260 * (i / total)
        envelope = max(0.0, 1.0 - (i / total))
        value = int(22000 * envelope * math.sin(2 * math.pi * freq * t))
        # Mixer inicializado em estéreo: duplicar sample L/R.
        samples.append(value)
        samples.append(value)

    return pygame.mixer.Sound(buffer=samples.tobytes())


def load_fail_sound(path):
    if os.path.exists(path):
        try:
            return pygame.mixer.Sound(path)
        except Exception as e:
            print(f"[AUDIO] Não consegui carregar {path}: {e}")

    print(f"[AUDIO] {path} não existe. Vou usar efeito gerado em Python.")
    return make_detune_sound()


def play_song(song_path):
    try:
        pygame.mixer.music.load(song_path)
        pygame.mixer.music.play()
        print(f"🎵 [MÚSICA] A tocar: {song_path}")
    except Exception as e:
        print(f"❌ Erro ao carregar música '{song_path}': {e}")


def main():
    parser = argparse.ArgumentParser(description="Listener de áudio UART para o projeto Guitar Hero.")
    parser.add_argument("--port", default=DEFAULT_SERIAL_PORT, help="Porta COM ligada ao MINIX, ex: COM6")
    parser.add_argument("--song", default=DEFAULT_SONG, help="MP3 a tocar quando chegar 0x01")
    parser.add_argument("--fail", default=DEFAULT_FAIL_WAV, help="WAV a tocar quando chegar 0x0E")
    args = parser.parse_args()

    pygame.mixer.init(frequency=44100, size=-16, channels=2, buffer=512)
    fail_sound = load_fail_sound(args.fail)

    try:
        ser = serial.Serial(args.port, BAUD_RATE, timeout=0.01)
    except Exception as e:
        print(f"ERRO: Não consegui abrir a porta {args.port}.")
        print("Confirma o com0com, o VirtualBox e se não tens outro Python a usar a porta.")
        print(f"Detalhe: {e}")
        pygame.mixer.quit()
        sys.exit(1)

    print("==================================================")
    print(f" Porta {args.port} aberta a {BAUD_RATE} bps")
    print(" Protocolo: 0x01=start, 0x0A=hit, 0x0E=miss")
    print(f" Música: {args.song}")
    print(" A aguardar bytes do MINIX... Ctrl+C para sair")
    print("==================================================")

    try:
        while True:
            data = ser.read(1)
            if not data:
                time.sleep(0.002)
                continue

            event = data[0]
            print(f"[UART] Recebi {event:#04x}")

            if event == EVENT_GAME_START_SONG1:
                play_song(SONGS[1])
            elif event == EVENT_GAME_START_SONG2:
                play_song(SONGS[2])
            elif event == EVENT_GAME_END:
                pygame.mixer.music.stop()
                print("🛑 [MÚSICA] Parada imediatamente (Fim de Jogo).")
            elif event == EVENT_HIT:
                print("✅ [HIT] Nota acertada.")
            elif event == EVENT_MISS:
                print("💥 [MISS] Nota falhada.")
                fail_sound.play()
            else:
                print(f"[UART] Byte ignorado: {event:#04x}")



    except KeyboardInterrupt:
        print("\n[INFO] Script terminado pelo utilizador.")
    finally:
        ser.close()
        pygame.mixer.quit()


if __name__ == "__main__":
    main()
