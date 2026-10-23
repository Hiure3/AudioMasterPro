#pragma once

// Protecao contra denormais (flush-to-zero / denormals-are-zero) na audio thread.
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
 #include <xmmintrin.h>
 #define AMP_HAS_SSE_CSR 1
#endif

namespace amp
{
    class ScopedNoDenormals
    {
    public:
       #ifdef AMP_HAS_SSE_CSR
        ScopedNoDenormals() noexcept : saved_ (_mm_getcsr()) { _mm_setcsr (saved_ | 0x8040u); }
        ~ScopedNoDenormals() noexcept { _mm_setcsr (saved_); }
       #else
        ScopedNoDenormals() noexcept = default;
        ~ScopedNoDenormals() noexcept = default;
       #endif
        ScopedNoDenormals (const ScopedNoDenormals&) = delete;
        ScopedNoDenormals& operator= (const ScopedNoDenormals&) = delete;

    private:
       #ifdef AMP_HAS_SSE_CSR
        unsigned int saved_;
       #endif
    };
}
