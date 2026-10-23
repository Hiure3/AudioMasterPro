#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Utilities/DbUtils.h"
#include "Utilities/LockFreeFifo.h"

namespace amp::metering
{
    // FFT radix-2 (potencia de 2).
    class Fft
    {
    public:
        void setSize (int n)
        {
            n_ = n;
            int bits = 0;
            while ((1 << bits) < n) ++bits;
            rev_.assign (static_cast<size_t> (n), 0);
            for (int i = 0; i < n; ++i)
            {
                int r = 0;
                for (int b = 0; b < bits; ++b)
                    if (i & (1 << b)) r |= 1 << (bits - 1 - b);
                rev_[static_cast<size_t> (i)] = r;
            }
            cos_.assign (static_cast<size_t> (n / 2), 0.0);
            sin_.assign (static_cast<size_t> (n / 2), 0.0);
            for (int k = 0; k < n / 2; ++k)
            {
                cos_[static_cast<size_t> (k)] = std::cos (2.0 * kPi * k / n);
                sin_[static_cast<size_t> (k)] = -std::sin (2.0 * kPi * k / n);
            }
        }

        int size() const noexcept { return n_; }

        void forward (std::vector<double>& re, std::vector<double>& im) const
        {
            for (int i = 0; i < n_; ++i)
            {
                const int j = rev_[static_cast<size_t> (i)];
                if (j > i) { std::swap (re[static_cast<size_t> (i)], re[static_cast<size_t> (j)]);
                             std::swap (im[static_cast<size_t> (i)], im[static_cast<size_t> (j)]); }
            }
            for (int len = 2; len <= n_; len <<= 1)
            {
                const int half = len / 2, stride = n_ / len;
                for (int i = 0; i < n_; i += len)
                    for (int j = 0; j < half; ++j)
                    {
                        const double wr = cos_[static_cast<size_t> (j * stride)];
                        const double wi = sin_[static_cast<size_t> (j * stride)];
                        const size_t a = static_cast<size_t> (i + j), b = static_cast<size_t> (i + j + half);
                        const double tr = re[b] * wr - im[b] * wi;
                        const double ti = re[b] * wi + im[b] * wr;
                        re[b] = re[a] - tr;  im[b] = im[a] - ti;
                        re[a] += tr;         im[a] += ti;
                    }
            }
        }

    private:
        int n_ = 0;
        std::vector<int> rev_;
        std::vector<double> cos_, sin_;
    };

    // Analisador de espectro (thread de UI): consome amostras mono de um FIFO lock-free,
    // aplica janela de Hann + FFT e entrega magnitudes em dBFS (0 dB = seno de pico 1.0).
    // Tamanhos suportados: 1024 .. 32768 (padrao 4096).
    class SpectrumAnalyzer
    {
    public:
        explicit SpectrumAnalyzer (LockFreeFifo<float>& source) : fifo_ (source) { setFftSize (4096); }

        void setSampleRate (double sr) noexcept { sr_ = sr; }
        double sampleRate() const noexcept { return sr_; }
        int fftSize() const noexcept { return size_; }

        void setFftSize (int n)
        {
            n = std::clamp (n, 256, 32768);
            int p = 256;
            while (p < n) p <<= 1;
            size_ = p;
            fft_.setSize (size_);
            history_.assign (static_cast<size_t> (size_), 0.0f);
            window_.assign (static_cast<size_t> (size_), 0.0);
            double sum = 0.0;
            for (int i = 0; i < size_; ++i)
            {
                window_[static_cast<size_t> (i)] = 0.5 - 0.5 * std::cos (2.0 * kPi * i / size_);
                sum += window_[static_cast<size_t> (i)];
            }
            scale_ = 2.0 / sum;
            re_.assign (static_cast<size_t> (size_), 0.0);
            im_.assign (static_cast<size_t> (size_), 0.0);
            mags_.assign (static_cast<size_t> (size_ / 2 + 1), -120.0f);
            writePos_ = 0;
            first_ = true;
        }

        // Chamar periodicamente na thread de UI.
        void update()
        {
            float tmp[1024];
            std::size_t got;
            while ((got = fifo_.pop (tmp, 1024)) > 0)
                for (std::size_t i = 0; i < got; ++i)
                {
                    history_[static_cast<size_t> (writePos_)] = tmp[i];
                    if (++writePos_ >= size_) writePos_ = 0;
                }

            for (int i = 0; i < size_; ++i)
            {
                re_[static_cast<size_t> (i)] = history_[static_cast<size_t> ((writePos_ + i) % size_)] * window_[static_cast<size_t> (i)];
                im_[static_cast<size_t> (i)] = 0.0;
            }
            fft_.forward (re_, im_);

            for (int k = 0; k <= size_ / 2; ++k)
            {
                const double mag = std::sqrt (re_[static_cast<size_t> (k)] * re_[static_cast<size_t> (k)]
                                              + im_[static_cast<size_t> (k)] * im_[static_cast<size_t> (k)]) * scale_;
                const float db = static_cast<float> (20.0 * std::log10 (std::max (mag, 1e-9)));
                float& m = mags_[static_cast<size_t> (k)];
                m = first_ ? db : m + (db - m) * (db > m ? 0.6f : 0.15f);
            }
            first_ = false;
        }

        const std::vector<float>& magnitudesDb() const noexcept { return mags_; }
        double binToHz (int bin) const noexcept { return bin * sr_ / size_; }

    private:
        LockFreeFifo<float>& fifo_;
        Fft fft_;
        double sr_ = 48000.0, scale_ = 1.0;
        int size_ = 4096, writePos_ = 0;
        bool first_ = true;
        std::vector<float> history_, mags_;
        std::vector<double> window_, re_, im_;
    };
}
