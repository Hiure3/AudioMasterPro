# Changelog

## [0.2.0] - Fase 1 completa
- feat: audio engine, mixer (2 inputs + master), gain, HPF, EQ 8 bandas, compressor, limiter com lookahead
- feat: LUFS (M/S/I), LRA e True Peak (BS.1770 / R128) em thread de metering
- feat: UI (mixer, gráfico de EQ com spectrum, dinâmica, loudness, audio settings, A/B, bypass, presets, tema dark/light)
- feat: presets de fábrica e arquivos .ampreset
- test: 53 testes automatizados + estabilidade (20 s no ctest, 30 min sob demanda)
- ci: windows-2022, workflow de estabilidade manual

## [0.1.0] - Etapa 1.0
- feat: esqueleto do projeto (CMake, presets, janela JUCE vazia, teste de dB, CI)
