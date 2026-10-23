#pragma once

#include <atomic>

namespace amp
{
    // Buffer triplo lock-free (1 produtor, 1 consumidor). O consumidor sempre le o
    // ultimo valor completo publicado, sem nunca esperar o produtor.
    template <typename T>
    class TripleBuffer
    {
    public:
        TripleBuffer() = default;
        explicit TripleBuffer (const T& init) { buf_[0] = buf_[1] = buf_[2] = init; }

        // Produtor.
        void publish (const T& v) noexcept
        {
            buf_[write_] = v;
            const int old = middle_.exchange (write_ | kDirty, std::memory_order_acq_rel);
            write_ = old & 3;
        }

        // Consumidor: devolve true se ha um valor novo (que passa a ser lido por read()).
        bool update() noexcept
        {
            if (middle_.load (std::memory_order_acquire) & kDirty)
            {
                const int old = middle_.exchange (read_, std::memory_order_acq_rel);
                read_ = old & 3;
                return true;
            }
            return false;
        }

        const T& read() const noexcept { return buf_[read_]; }

    private:
        static constexpr int kDirty = 4;
        T buf_[3] {};
        int write_ = 0;
        int read_ = 1;
        std::atomic<int> middle_ { 2 };
    };
}
