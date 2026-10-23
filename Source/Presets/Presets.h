#pragma once

#include <string>

#include "Audio/Parameters.h"

namespace amp::presets
{
    // Presets de fabrica: Default, Vocal, Speech, Bass, Kick, Snare, Drums, Master, Live, Church Live.
    // Sao pontos de partida; ajuste ao gosto/sala.
    int numFactoryPresets() noexcept;
    const char* factoryPresetName (int index) noexcept;
    ParamSnapshot makeFactoryPreset (int index);

    // Presets so do compressor: Vocal, Bass, Drums, Speech, Master.
    int numCompressorPresets() noexcept;
    const char* compressorPresetName (int index) noexcept;
    void applyCompressorPreset (int index, ParamSnapshot& p) noexcept;

    // Formato proprio em texto (chave=valor, uma por linha, cabecalho "AMP_PRESET 1").
    // Seguro: chaves desconhecidas sao ignoradas e todos os valores passam por sanitize().
    std::string serialize (const ParamSnapshot& p);
    bool parse (const std::string& text, ParamSnapshot& out);
}
