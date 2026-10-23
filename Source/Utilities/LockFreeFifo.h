#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <vector>

namespace amp
{
    // FIFO lock-free de UM produtor e UM consumidor (SPSC). Nunca bloqueia nem aloca
    // depois de construida. Se estiver cheia, push() grava so o que cabe.
    template <typename T>
    class LockFreeFifo
    {
    public:
        explicit LockFreeFifo (std::size_t minCapacity = 1u << 16)
        {
            std::size_t c = 2;
            while (c < minCapacity) c <<= 1;
            buf_.assign (c, T{});
            mask_ = c - 1;
        }

        std::size_t capacity() const noexcept { return buf_.size(); }

        // Produtor.
        std::size_t push (const T* src, std::size_t n) noexcept
        {
            const std::size_t w = head_.load (std::memory_order_relaxed);
            const std::size_t r = tail_.load (std::memory_order_acquire);
            const std::size_t freeSpace = buf_.size() - (w - r);
            const std::size_t cnt = std::min (n, freeSpace);
            for (std::size_t i = 0; i < cnt; ++i)
                buf_[(w + i) & mask_] = src[i];
            head_.store (w + cnt, std::memory_order_release);
            return cnt;
        }

        // Consumidor.
        std::size_t pop (T* dst, std::size_t n) noexcept
        {
            const std::size_t r = tail_.load (std::memory_order_relaxed);
            const std::size_t w = head_.load (std::memory_order_acquire);
            const std::size_t cnt = std::min (n, w - r);
            for (std::size_t i = 0; i < cnt; ++i)
                dst[i] = buf_[(r + i) & mask_];
            tail_.store (r + cnt, std::memory_order_release);
            return cnt;
        }

        std::size_t available() const noexcept
        {
            return head_.load (std::memory_order_acquire) - tail_.load (std::memory_order_acquire);
        }

        // Somente o consumidor.
        void clear() noexcept { tail_.store (head_.load (std::memory_order_acquire), std::memory_order_release); }

    private:
        std::vector<T> buf_;
        std::size_t mask_ = 0;
        std::atomic<std::size_t> head_ { 0 };
        std::atomic<std::size_t> tail_ { 0 };
    };
}
