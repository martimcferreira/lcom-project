import serial
import pygame
import sys

# ----------------------------
# CONFIGURAÇÃO DA PORTA (Ajustada para a tua CNCB2 -> COM6)
# ----------------------------
SERIAL_PORT = 'COM6' 
BAUD_RATE = 115200

# ----------------------------
# INIT AUDIO & SERIAL
# ----------------------------
pygame.mixer.init(frequency=44100, size=-16, channels=2, buffer=512)

try:
    # timeout=0.01 garante que o loop do Python corre livre e sem breaks
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.01)
    print(f"==================================================")
    print(f" PORTA {SERIAL_PORT} ABERTA COM SUCESSO!")
    print(f" A aguardar comandos do MINIX... (Prime Ctrl+C para sair)")
    print(f"==================================================")
except Exception as e:
    print(f"ERRO CRÍTICO: Não foi possível abrir a porta {SERIAL_PORT}.")
    print(f"Verifica se o com0com está ativo ou se outro programa a está a usar.")
    print(f"Detalhe do erro: {e}")
    sys.exit(1)

# ----------------------------
# LOAD SOUNDS (Efeitos curtos - WAV)
# ----------------------------
try:
    guitar = {
        0x01: pygame.mixer.Sound("guitar/green.wav"),
        0x02: pygame.mixer.Sound("guitar/red.wav"),
        0x03: pygame.mixer.Sound("guitar/yellow.wav"),
        0x04: pygame.mixer.Sound("guitar/blue.wav"),
        0x05: pygame.mixer.Sound("guitar/orange.wav")
    }

    fail_sound = pygame.mixer.Sound("fx/fail.wav")
    combo_sound = pygame.mixer.Sound("fx/combo.wav")
    crowd_sound = pygame.mixer.Sound("fx/crowd.wav")
except Exception as e:
    print(f"AVISO: Alguns ficheiros de áudio .wav não foram encontrados.")
    print(f"Garante que as pastas 'guitar' e 'fx' existem. Erro: {e}\n")

# ----------------------------
# MUSIC TRACKS (Música de fundo - MP3)
# ----------------------------
songs = {
    0x01: "songs/song1.mp3",
    0x02: "songs/song2.mp3"
}

def play_song(song_id):
    if song_id in songs:
        try:
            pygame.mixer.music.load(songs[song_id])
            pygame.mixer.music.play()
            print(f"🎵 [MÚSICA] A iniciar faixa {song_id}: {songs[song_id]}")
        except Exception as e:
            print(f"❌ Erro ao carregar música {song_id}: {e}")

def stop_song():
    pygame.mixer.music.stop()
    print("🛑 [MÚSICA] Parada imediatamente.")

# ----------------------------
# SERIAL LOOP (Otimizado e Não-Bloqueante)
# ----------------------------
try:
    while True:
        # Só lê se houver dados no buffer da porta série
        if ser.in_waiting > 0:
            byte = ser.read(1)[0]
            
            if byte != 0x01AA and byte != 0xAA: # Proteção contra variações de tipo de dados
                continue 

            # Lê os restantes 3 bytes do pacote [CMD, ARG, END]
            packet = ser.read(3)
            if len(packet) < 3:
                print(f"Recebi o byte: {hex(byte)} mas o pacote está incompleto: {[hex(b) for b in packet]}")
                continue 
                
            cmd = packet[0]
            arg = packet[1]
            end = packet[2]
            print(f"Recebi o byte: {hex(byte)} | CMD={hex(cmd)} ARG={hex(arg)} END={hex(end)}")
            
            if end != 0xFF:
                continue # Descarta se o pacote estiver desalinhado

            # --- PROCESSAMENTO DO PROTOCOLO ---
            
            # Família 0x10: MÚSICA
            if cmd == 0x10:
                play_song(arg)
            elif cmd == 0x11:
                stop_song()
            elif cmd == 0x12:
                pygame.mixer.music.set_volume(arg / 100.0)

            # Família 0x20: GUITARRA
            elif cmd == 0x20:
                print(f"🎸 [HIT] Pista {arg} acertada!")
                if 'guitar' in locals() and arg in guitar:
                    guitar[arg].play()
            elif cmd == 0x21:
                print("💥 [MISS] Jogador falhou a nota.")
                if 'fail_sound' in locals(): fail_sound.play()

            # Família 0x40: UI / EFEITOS
            elif cmd == 0x40:
                if 'combo_sound' in locals(): combo_sound.play()
            elif cmd == 0x41:
                if 'crowd_sound' in locals(): crowd_sound.play()
            elif cmd == 0x42:
                if 'fail_sound' in locals(): fail_sound.play()
                
except KeyboardInterrupt:
    print("\n[INFO] Script terminado pelo utilizador.")
finally:
    ser.close()
    pygame.mixer.quit()