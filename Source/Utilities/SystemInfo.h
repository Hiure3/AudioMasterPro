#pragma once

#include <cstddef>

namespace amp
{
    // Memoria residente (working set) do processo, em bytes. 0 se indisponivel.
    std::size_t processMemoryBytes() noexcept;
}
