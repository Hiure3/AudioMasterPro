# AUDIO MASTER PRO
**Professional Audio. Precision Loudness. One Platform.**

Processador de áudio profissional para Windows (C++20 + JUCE), com baixa latência, medição de loudness
conforme ITU-R BS.1770 / EBU R128 e arquitetura preparada para VST3.

## O que a Fase 1 faz

```
INPUT 1 ┐                                                              ┌─> LUFS / True Peak (thread própria)
        ├─ GAIN → PAN/MUTE/SOLO → soma → HPF → EQ 8 bandas → COMPRESSOR → LIMITER → MASTER ─┤
INPUT 2 ┘                                                              └─> MONITOR → saída
                                    (Spectrum antes e depois do EQ)         BYPASS ALL / A-B
```

| Recurso | Detalhes |
|---|---|
| Audio Settings | Driver (WASAPI / DirectSound; ASIO opcional), dispositivos, canais, sample rate (44.1/48/88.2/96 kHz), buffer (32…1024, conforme o driver), latências |
| Mixer | INPUT 1, INPUT 2, MASTER: gain (-60…+24 dB), pan (lei de -3 dB), mute, solo, medidor |
| HPF | 20 Hz–2 kHz, 12/18/24/36/48 dB/oct (Butterworth) |
| EQ paramétrico | 8 bandas: Bell, Low/High Shelf, Low/High Cut, Notch. Gráfico com arrastar (freq/ganho), roda do mouse (Q), duplo clique (reset) |
| Spectrum | FFT 1024…32768 (padrão 4096), 20 Hz–20 kHz log, entrada e pós-EQ |
| Compressor | Threshold, ratio, attack, release, knee, makeup, mix; medidor de GR; presets Vocal/Bass/Drums/Speech/Master |
| Limiter | Brickwall com lookahead (0,5–10 ms), threshold, ceiling, release; medidor de GR |
| Loudness | LUFS-M / S / I, LRA, True Peak (4x), TP Max, histórico, alvo (target), diferença e status LOW/GOOD/WARNING/TOO LOUD/CLIPPING |
| Bypass / A-B | Bypass instantâneo sem click (alinhado pela latência do limiter); A/B com snapshots |
| Presets | 10 de fábrica + salvar/carregar `.ampreset` (texto seguro) |
| Performance | CPU, Audio CPU, memória, SR, buffer, latência, dropouts, XRuns, erros; aviso de CPU alta |
| Tema | Dark (padrão) e Light; HiDPI |

## Como compilar (Windows)

Pré-requisitos: **Windows 10/11 x64**, **Visual Studio 2022** (workload "Desenvolvimento para desktop com C++"), **Git** e internet no primeiro build.

Abra o **Developer PowerShell for VS 2022** e rode:

```powershell
git clone https://github.com/SEU-USUARIO/AudioMasterPro.git   # baixa o projeto
cd AudioMasterPro                                              # entra na pasta
cmake --preset windows-release                                 # configura e baixa JUCE e Catch2
cmake --build --preset windows-release                         # compila (Release)
ctest --preset windows-release                                 # roda os testes automatizados
```

O programa fica em `build\windows-release\AudioMasterPro_artefacts\Release\Audio Master Pro.exe`.

Sem instalar nada: use o GitHub Actions (aba **Actions** → execução → **Artifacts** → `AudioMasterPro-Windows-Release`).

### ASIO (opcional)
O SDK da Steinberg não pode ser redistribuído. Baixe-o em steinberg.net/developers e configure com:
```powershell
cmake --preset windows-release -DAMP_ASIO_SDK_DIR="C:/caminho/ASIOSDK"
```
Sem isso o programa usa WASAPI/DirectSound normalmente.

## Como usar (primeiros passos)
1. Abra o programa. **Use fones de ouvido**: o monitor vem **desligado** para evitar microfonia.
2. **AUDIO SETTINGS** → escolha driver, entrada, saída, sample rate (comece com 48 kHz) e buffer.
3. Fale/toque no microfone e veja o medidor do INPUT 1. Clique em **MONITOR** para ouvir.
4. Escolha um preset (Vocal, Speech, Church Live...) ou ajuste HPF/EQ/Compressor/Limiter.
5. Veja LUFS/True Peak no painel da direita e defina o TARGET (ex.: -14 LUFS, -1 dBTP).

Windows: se o microfone não aparecer/silenciar, ative *Configurações → Privacidade → Microfone → Permitir que aplicativos da área de trabalho acessem o microfone*.

## Testes
- `AMP_Tests` (Catch2): dB, biquads, HPF (resposta Butterworth analítica), EQ, compressor, limiter, **K-weighting vs. coeficientes da norma**, **LUFS** (seno -23 dBFS = -23,0 LUFS, gating absoluto/relativo, LRA, ruído rosa, 44.1/88.2/96 kHz), **True Peak**, FFT, FIFO/TripleBuffer, engine (roteamento, mute/solo, bypass sem click, NaN/Inf, fuzz de parâmetros), presets.
- `stability_quick`: 20 s de áudio simulado (roda no `ctest`).
- `stability_30min`: 30 min simulados (`ctest --preset windows-release-long`, ou o workflow manual **stability-30min**). Opção `--realtime` roda em tempo real.

## Arquitetura (resumo)
- `AMP_Core` (biblioteca estática, **sem JUCE**): DSP, Metering, AudioEngine, Presets. É o núcleo que o VST3 vai reutilizar.
- `AudioMasterPro` (Standalone): JUCE + UI + dispositivo. Ver `Docs/ARCHITECTURE.md`.
- Threads: Audio (sem alocação/mutex/disco), UI (30 Hz), Metering (LUFS/TP), com FIFOs lock-free e TripleBuffer de parâmetros.

## Licença
Pendente (ver `LICENSE`). O JUCE tem licença própria (AGPLv3 / comercial).
