# Arquitetura - Fase 1

## Camadas
| Pasta | Conteúdo | Depende de JUCE? |
|---|---|---|
| `Source/Utilities` | dB, SmoothedValue, LockFreeFifo, TripleBuffer, DenormalGuard, TestSignals, SystemInfo | não |
| `Source/DSP` | Biquad, HighPassFilter, ParametricEQ, Compressor, Limiter, StereoDelay | não |
| `Source/Metering` | LoudnessMeter (BS.1770/R128), TruePeak, SpectrumAnalyzer (FFT), MeteringWorker | não |
| `Source/Audio` | Parameters (snapshot + sanitize), AudioEngine | não |
| `Source/Presets` | presets de fábrica e formato `.ampreset` | não |
| `Source/Application` | Main, AppController (dispositivo + A/B) | sim |
| `Source/UI` | MainComponent, Mixer, EQ, Dinâmica, Loudness, Audio Settings, Tema | sim |
| `Source/Plugin` | reservado para o wrapper VST3 (Fase 2) | - |

## Threads
- **Audio thread**: `AudioEngine::process`. Sem alocação, mutex, disco, rede ou chamadas de UI. Proteção contra denormais (FTZ/DAZ). Entradas/saídas não finitas são neutralizadas e contadas.
- **UI thread**: edita `ParamSnapshot` e publica via `TripleBuffer` (lock-free, o áudio nunca espera). Lê medidores por atomics e FIFOs.
- **Metering thread**: recebe L/R por FIFO lock-free; calcula K-weighting, gating, LRA e True Peak; publica via TripleBuffer.

## Decisões
- **Cadeia no barramento de mix**: HPF/EQ/Compressor/Limiter processam a soma dos inputs (como no diagrama da especificação). Inserts por canal ficam para a Fase 3 (Mixer/Routing).
- **Master fader após o limiter** (somente atenuação, -60…0 dB) para nunca violar o ceiling; o LUFS/True Peak mede a saída final.
- **Limiter**: latência de lookahead constante (mesmo desligado), compensada no caminho "original" do bypass.
- **Loudness** usa histogramas (sem alocação): Integrated com bins de 0,01 LU e LRA com 0,1 LU.
- **True Peak**: interpolador FIR polifásico 4x (sinc + Kaiser, 96 taps); a norma admite tolerância, validado por testes.
- **Pan**: lei de -3 dB (centro = -3 dB em cada lado).
- **Parâmetros**: coeficientes de filtros recalculados quando mudam; mixes (enable/bypass) com rampa de 10–20 ms.

## Fase 2 (VST3)
O wrapper VST3 será um `juce::AudioProcessor` fino que chama `AudioEngine::process` e mapeia `ParamSnapshot` para parâmetros automatizáveis. Nenhum DSP será duplicado.
