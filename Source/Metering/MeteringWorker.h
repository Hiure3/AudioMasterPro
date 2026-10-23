#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "LoudnessMeter.h"
#include "Utilities/LockFreeFifo.h"
#include "Utilities/TripleBuffer.h"

namespace amp::metering
{
    // Thread de metering: a audio thread so empurra amostras (lock-free); aqui roda o
    // calculo pesado (LUFS, gating, LRA, True Peak) e publica resultados para a UI.
    class MeteringWorker
    {
    public:
        MeteringWorker() : fifo_ (1u << 18) {}
        ~MeteringWorker() { stop(); }

        // Nao-realtime. Para a thread, reconfigura e reinicia.
        void start (double sampleRate)
        {
            stop();
            meter_.prepare (sampleRate);
            fifo_.clear();
            results_.publish (LoudnessResults{});
            resetRequested_.store (false);
            running_.store (true);
            thread_ = std::thread ([this] { run(); });
        }

        void stop()
        {
            running_.store (false);
            if (thread_.joinable())
                thread_.join();
        }

        // Audio thread: nunca bloqueia. Entrelaca L/R no FIFO; se cheio, descarta e conta.
        void push (const float* l, const float* r, int n) noexcept
        {
            float tmp[512];
            int done = 0;
            while (done < n)
            {
                const int chunk = std::min (256, n - done);
                for (int i = 0; i < chunk; ++i) { tmp[2 * i] = l[done + i]; tmp[2 * i + 1] = r[done + i]; }
                const std::size_t want = static_cast<std::size_t> (chunk) * 2;
                if (fifo_.push (tmp, want) < want)
                    dropped_.fetch_add (1, std::memory_order_relaxed);
                done += chunk;
            }
        }

        void requestReset() noexcept { resetRequested_.store (true); }

        // Thread de UI.
        LoudnessResults latest() noexcept
        {
            results_.update();
            return results_.read();
        }

        std::uint64_t droppedChunks() const noexcept { return dropped_.load(); }

    private:
        void run()
        {
            std::vector<float> buf (4096), l (2048), r (2048);
            while (running_.load())
            {
                if (resetRequested_.exchange (false))
                {
                    meter_.reset();
                    results_.publish (meter_.results());
                }

                std::size_t got = fifo_.pop (buf.data(), buf.size());
                got &= ~static_cast<std::size_t> (1);
                if (got == 0)
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (4));
                    continue;
                }
                const int frames = static_cast<int> (got / 2);
                for (int i = 0; i < frames; ++i) { l[static_cast<size_t> (i)] = buf[2 * static_cast<size_t> (i)];
                                                   r[static_cast<size_t> (i)] = buf[2 * static_cast<size_t> (i) + 1]; }
                meter_.process (l.data(), r.data(), frames);
                results_.publish (meter_.results());
            }
        }

        LockFreeFifo<float> fifo_;
        LoudnessMeter meter_;
        TripleBuffer<LoudnessResults> results_;
        std::thread thread_;
        std::atomic<bool> running_ { false };
        std::atomic<bool> resetRequested_ { false };
        std::atomic<std::uint64_t> dropped_ { 0 };
    };
}
