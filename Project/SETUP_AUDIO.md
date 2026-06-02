# 🎸 Guia de Configuração — Subsistema de Áudio (UART)

Este guia explica passo a passo como configurar as portas série virtuais, a máquina virtual MINIX 3 e o script Python no Windows para que todos os membros do grupo consigam jogar com música e efeitos sonoros em tempo real.

---

## 🛠️ Passo 1: Instalação do `com0com` (Windows)

A UART do MINIX comunica com o Windows através de um par de portas série virtuais interligadas ("Null-Modem"). 

1. Faz o download do **[Null-modem emulator (com0com)](https://sourceforge.net/projects/com0com/)** (recomenda-se a versão assinada para evitar problemas de drivers no Windows 10/11).
2. Instala o programa e abre a **Setup Command Tool** do `com0com` (ou a interface gráfica).
3. Cria um par de portas virtuais ligadas entre si. Por exemplo:
   * **Porta A:** `CNCA0` (será usada pela máquina virtual)
   * **Porta B:** `COM6` (ou `CNCB0`, que será usada pelo Python no Windows)
4. Garante que as portas estão ativas e memoriza os nomes configurados.

---

## 💻 Passo 2: Configuração da Porta Série no VirtualBox

Para que o MINIX 3 consiga aceder à porta série virtual CNCA0 do Windows:

1. Desliga a Máquina Virtual do MINIX 3.
2. No VirtualBox, clica em **Definições (Settings)** da tua VM.
3. Acede ao menu **Portas Série (Serial Ports)** -> **Porta 1 (Port 1)**.
4. Configura com os seguintes parâmetros:
   * [x] **Ativar Porta Série (Enable Serial Port)**
   * **Número de Porta (Port Number):** `COM1` (isto mapeia para o endereço base `0x3F8` no MINIX)
   * **Modo de Porta (Port Mode):** `Dispositivo do Hospedeiro` (Host Device)
   * **Caminho do Dispositivo/Canal (Path/Address):** `\\.\CNCA0` (o nome da Porta A que criaste no com0com)
5. Clica em **OK** para guardar.

---

## 🐍 Passo 3: Executar o Subsistema de Áudio (Windows)

O script Python é responsável por receber os eventos de hit, miss e início de faixa da UART e reproduzir os sons via Pygame.

1. Instala as dependências necessárias no Windows (caso não as tenhas):
   ```bash
   pip install pyserial pygame
   ```
2. Acede à pasta do subsistema de áudio no Windows:
   ```bash
   cd audio_subsystem
   ```
3. Verifica se a porta configurada no topo de `som_guitar_hero.py` coincide com a tua **Porta B** do `com0com`:
   ```python
   DEFAULT_SERIAL_PORT = "COM6"  # Ajusta se a tua porta do com0com for diferente
   ```
   Também podes passar a porta pela linha de comandos, sem editar o ficheiro:
   ```bash
   python som_guitar_hero.py --port COM6
   ```
4. Corre o script de áudio no Windows:
   ```bash
   python som_guitar_hero.py --port COM6
   ```
   *(Deverá aparecer a mensagem confirmando que a porta COM foi aberta com sucesso e que está a aguardar comandos do MINIX).*

---

## 🎮 Passo 4: Compilar e Jogar no MINIX 3

Graças às nossas últimas atualizações, todo o processo de carregamento de beatmaps foi automatizado!

1. Liga a Máquina Virtual e acede à pasta do projeto:
   ```bash
   cd /shares/lcom/grupo_2leic02_2/Project   # (ou o teu caminho de montagem)
   ```
2. **Compila o projeto:**
   ```bash
   make
   ```
   > [!NOTE]
   > O `Makefile` irá copiar de forma totalmente automática todos os ficheiros de notas da pasta `beatmaps` para a memória local `/tmp/beatmaps` do MINIX 3. Isto garante que a persistência das notas funciona sempre, mesmo após a VM reiniciar!

3. **Executa o jogo:**
   ```bash
   lcom_run proj
   ```

---

## 🎵 Escolha/Troca de Músicas

A música é escolhida no próprio jogo, no ecrã de seleção depois de carregar em **Play**. Não é preciso alterar `main.c` nem recompilar só para mudar de faixa.

O MINIX envia pela UART um evento diferente para cada música:

* `0x01` -> inicia a música 1;
* `0x02` -> inicia a música 2;
* `0x03` -> termina/paragem da música;
* `0x0A` -> hit;
* `0x0E` -> miss.

---

## 🛠️ Resolução de Problemas (Troubleshooting)

> [!WARNING]
> **Erro: "Não foi possível abrir a porta COM"**
> * Garante que a Máquina Virtual está ligada, pois o VirtualBox "bloqueia" a porta CNCA0 apenas quando a VM está em execução.
> * Verifica se não tens outra instância do script Python aberta a ocupar a porta série.
> * Confirma se os nomes das portas no `com0com` coincidem exatamente com o script e com o VirtualBox.
