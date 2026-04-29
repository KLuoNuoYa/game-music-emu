#include "gme_inno.h"

#include "../gme/gme.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <climits>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kChannels = 2;
constexpr int kBitsPerSample = 16;
constexpr int kBufferCount = 4;
constexpr int kSamplesPerBuffer = 4096;

struct AudioBuffer {
    WAVEHDR header {};
    std::vector<short> samples;
    bool prepared = false;
    bool queued = false;
};

class InnoPlayer {
public:
    ~InnoPlayer() {
        close();
    }

    int open_file(const wchar_t* path, int sample_rate) {
        return open_file_track(path, sample_rate, 0);
    }

    int open_file_track(const wchar_t* path, int sample_rate, int track_index) {
        if (!path || !*path) {
            set_last_error(L"Music path is empty.");
            return 0;
        }
        if (sample_rate <= 0) {
            set_last_error(L"Sample rate must be greater than zero.");
            return 0;
        }

        std::vector<unsigned char> file_data;
        if (!read_file(path, file_data)) {
            return 0;
        }

        stop_thread();

        std::lock_guard<std::recursive_mutex> lock(mutex_);
        close_locked();

        Music_Emu* emu = nullptr;
        gme_err_t err = gme_open_data(file_data.data(), static_cast<long>(file_data.size()), &emu, sample_rate);
        if (err) {
            set_last_error_ascii(err);
            return 0;
        }

        emu_ = emu;
        file_data_ = std::move(file_data);
        sample_rate_ = sample_rate;
        current_track_ = 0;
        last_error_.clear();

        gme_ignore_silence(emu_, 1);
        gme_set_autoload_playback_limit(emu_, 0);

        if (track_index < 0) {
            track_index = 0;
        }

        if (!start_track_locked(track_index)) {
            close_locked();
            return 0;
        }

        if (!open_wave_out_locked()) {
            close_locked();
            return 0;
        }

        return 1;
    }

    void close() {
        stop_thread();
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        close_locked();
    }

    int start_track(int track_index) {
        stop_thread();

        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!emu_) {
            set_last_error(L"No music file is loaded.");
            return 0;
        }
        if (!start_track_locked(track_index)) {
            return 0;
        }
        return 1;
    }

    int play() {
        cleanup_finished_worker();

        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!emu_) {
            set_last_error(L"No music file is ready for playback.");
            return 0;
        }

        if (worker_.joinable()) {
            if (paused_) {
                paused_ = false;
                waveOutRestart(wave_out_);
            }
            return 1;
        }

        reset_worker_state_locked();
        stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        buffer_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!stop_event_ || !buffer_event_) {
            set_last_error(L"Unable to create playback events.");
            close_worker_handles_locked();
            return 0;
        }

        MMRESULT mmr = waveOutOpen(&wave_out_, WAVE_MAPPER, &wave_format_,
                                   reinterpret_cast<DWORD_PTR>(buffer_event_), 0, CALLBACK_EVENT);
        if (mmr != MMSYSERR_NOERROR) {
            wave_out_ = nullptr;
            set_mm_error(mmr, L"Unable to open audio output device.");
            close_worker_handles_locked();
            return 0;
        }

        try {
            worker_ = std::thread(&InnoPlayer::worker_loop, this);
        } catch (...) {
            waveOutClose(wave_out_);
            wave_out_ = nullptr;
            close_worker_handles_locked();
            set_last_error(L"Unable to create playback thread.");
            return 0;
        }

        paused_ = false;
        worker_running_ = true;
        return 1;
    }

    void pause() {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (wave_out_ && worker_.joinable() && !paused_) {
            waveOutPause(wave_out_);
            paused_ = true;
        }
    }

    void stop() {
        stop_thread();

        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (emu_) {
            gme_start_track(emu_, current_track_);
        }
    }

    int is_playing() {
        cleanup_finished_worker();

        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return worker_running_ && !paused_ ? 1 : 0;
    }

    void set_loop(int enabled) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        loop_enabled_ = enabled != 0;
    }

    void set_volume(int volume_percent) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        volume_percent_ = std::max(0, std::min(100, volume_percent));
    }

    int get_track_count() {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return emu_ ? gme_track_count(emu_) : 0;
    }

    int get_current_track() {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return emu_ ? current_track_ : -1;
    }

    int get_last_error(wchar_t* buffer, int capacity) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        int needed = static_cast<int>(last_error_.size()) + 1;
        if (!buffer || capacity <= 0) {
            return needed;
        }

        int to_copy = std::min(capacity - 1, static_cast<int>(last_error_.size()));
        if (to_copy > 0) {
            memcpy(buffer, last_error_.data(), static_cast<size_t>(to_copy) * sizeof(wchar_t));
        }
        buffer[to_copy] = L'\0';
        return needed;
    }

    int get_last_error_length() {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return static_cast<int>(last_error_.size()) + 1;
    }

private:
    void worker_loop() {
        bool thread_started = false;

        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (!emu_ || !wave_out_) {
                return;
            }
            allocate_buffers_locked();
            for (AudioBuffer& buffer : buffers_) {
                if (!queue_buffer_locked(buffer)) {
                    break;
                }
                thread_started = true;
            }
        }

        if (!thread_started) {
            finish_worker();
            return;
        }

        HANDLE wait_handles[2];
        for (;;) {
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                wait_handles[0] = stop_event_;
                wait_handles[1] = buffer_event_;
            }

            DWORD wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
            if (wait_result == WAIT_OBJECT_0) {
                break;
            }
            if (wait_result != WAIT_OBJECT_0 + 1) {
                set_last_error(L"Playback thread wait failed.");
                break;
            }

            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (!wave_out_) {
                break;
            }

            bool any_queued = false;
            for (AudioBuffer& buffer : buffers_) {
                if (buffer.queued && (buffer.header.dwFlags & WHDR_DONE) != 0) {
                    buffer.queued = false;
                    if (fill_buffer_locked(buffer)) {
                        MMRESULT mmr = waveOutWrite(wave_out_, &buffer.header, sizeof(buffer.header));
                        if (mmr == MMSYSERR_NOERROR) {
                            buffer.queued = true;
                            any_queued = true;
                        } else {
                            set_mm_error(mmr, L"Unable to submit audio buffer.");
                        }
                    }
                } else if (buffer.queued) {
                    any_queued = true;
                }
            }

            if (!any_queued) {
                break;
            }
        }

        finish_worker();
    }

    void finish_worker() {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (wave_out_) {
            waveOutReset(wave_out_);
            cleanup_buffers_locked();
            waveOutClose(wave_out_);
            wave_out_ = nullptr;
        }
        paused_ = false;
        worker_running_ = false;
        close_worker_handles_locked();
    }

    void stop_thread() {
        cleanup_finished_worker();

        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (!worker_.joinable()) {
                return;
            }
            if (stop_event_) {
                SetEvent(stop_event_);
            }
            if (wave_out_) {
                waveOutReset(wave_out_);
            }
        }

        worker_.join();
    }

    void cleanup_finished_worker() {
        std::thread finished_worker;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (!worker_.joinable() || worker_running_) {
                return;
            }
            finished_worker = std::move(worker_);
        }

        if (finished_worker.joinable()) {
            finished_worker.join();
        }
    }

    bool read_file(const wchar_t* path, std::vector<unsigned char>& file_data) {
        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            set_last_error(L"Unable to open music file.");
            return false;
        }

        LARGE_INTEGER size {};
        if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > LONG_MAX) {
            CloseHandle(file);
            set_last_error(L"Music file size is unsupported.");
            return false;
        }

        file_data.resize(static_cast<size_t>(size.QuadPart));
        DWORD bytes_read = 0;
        BOOL ok = file_data.empty() ? TRUE :
            ReadFile(file, file_data.data(), static_cast<DWORD>(file_data.size()), &bytes_read, nullptr);
        CloseHandle(file);

        if (!ok || bytes_read != file_data.size()) {
            file_data.clear();
            set_last_error(L"Unable to read music file.");
            return false;
        }

        return true;
    }

    bool start_track_locked(int track_index) {
        int track_count = gme_track_count(emu_);
        if (track_index < 0 || track_index >= track_count) {
            set_last_error(L"Track index is out of range.");
            return false;
        }

        gme_err_t err = gme_start_track(emu_, track_index);
        if (err) {
            set_last_error_ascii(err);
            return false;
        }

        current_track_ = track_index;
        last_error_.clear();
        return true;
    }

    bool open_wave_out_locked() {
        wave_format_.wFormatTag = WAVE_FORMAT_PCM;
        wave_format_.nChannels = kChannels;
        wave_format_.nSamplesPerSec = sample_rate_;
        wave_format_.wBitsPerSample = kBitsPerSample;
        wave_format_.nBlockAlign = static_cast<WORD>((wave_format_.nChannels * wave_format_.wBitsPerSample) / 8);
        wave_format_.nAvgBytesPerSec = wave_format_.nSamplesPerSec * wave_format_.nBlockAlign;
        wave_format_.cbSize = 0;
        return true;
    }

    void allocate_buffers_locked() {
        buffers_.clear();
        buffers_.resize(kBufferCount);
        for (AudioBuffer& buffer : buffers_) {
            buffer.samples.resize(kSamplesPerBuffer);
            buffer.header.lpData = reinterpret_cast<LPSTR>(buffer.samples.data());
            buffer.header.dwBufferLength = static_cast<DWORD>(buffer.samples.size() * sizeof(short));
            buffer.header.dwFlags = 0;
            buffer.header.dwLoops = 0;
            buffer.header.dwUser = 0;
            buffer.header.lpNext = nullptr;
            buffer.header.reserved = 0;
        }
    }

    bool queue_buffer_locked(AudioBuffer& buffer) {
        if (!fill_buffer_locked(buffer)) {
            return false;
        }

        if (!buffer.prepared) {
            MMRESULT mmr = waveOutPrepareHeader(wave_out_, &buffer.header, sizeof(buffer.header));
            if (mmr != MMSYSERR_NOERROR) {
                set_mm_error(mmr, L"Unable to prepare audio buffer.");
                return false;
            }
            buffer.prepared = true;
        }

        MMRESULT mmr = waveOutWrite(wave_out_, &buffer.header, sizeof(buffer.header));
        if (mmr != MMSYSERR_NOERROR) {
            set_mm_error(mmr, L"Unable to queue audio buffer.");
            return false;
        }

        buffer.queued = true;
        return true;
    }

    bool fill_buffer_locked(AudioBuffer& buffer) {
        if (!emu_) {
            return false;
        }

        if (!loop_enabled_ && gme_track_ended(emu_)) {
            return false;
        }

        gme_err_t err = gme_play(emu_, static_cast<int>(buffer.samples.size()), buffer.samples.data());
        if (err) {
            set_last_error_ascii(err);
            zero_buffer(buffer);
            return false;
        }

        if (gme_track_ended(emu_) && loop_enabled_) {
            err = gme_start_track(emu_, current_track_);
            if (err) {
                set_last_error_ascii(err);
                return false;
            }
        }

        apply_volume_locked(buffer.samples);
        return true;
    }

    void apply_volume_locked(std::vector<short>& samples) const {
        if (volume_percent_ >= 100) {
            return;
        }

        for (short& sample : samples) {
            int scaled = static_cast<int>(sample) * volume_percent_ / 100;
            scaled = std::max(-32768, std::min(32767, scaled));
            sample = static_cast<short>(scaled);
        }
    }

    void cleanup_buffers_locked() {
        for (AudioBuffer& buffer : buffers_) {
            if (buffer.prepared) {
                waveOutUnprepareHeader(wave_out_, &buffer.header, sizeof(buffer.header));
                buffer.prepared = false;
            }
            buffer.queued = false;
            buffer.header.dwFlags = 0;
        }
        buffers_.clear();
    }

    void close_locked() {
        cleanup_buffers_locked();
        if (wave_out_) {
            waveOutClose(wave_out_);
            wave_out_ = nullptr;
        }
        if (emu_) {
            gme_delete(emu_);
            emu_ = nullptr;
        }
        file_data_.clear();
        current_track_ = 0;
        paused_ = false;
        close_worker_handles_locked();
    }

    void reset_worker_state_locked() {
        cleanup_buffers_locked();
        close_worker_handles_locked();
        paused_ = false;
    }

    void close_worker_handles_locked() {
        if (buffer_event_) {
            CloseHandle(buffer_event_);
            buffer_event_ = nullptr;
        }
        if (stop_event_) {
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
        }
    }

    static void zero_buffer(AudioBuffer& buffer) {
        std::fill(buffer.samples.begin(), buffer.samples.end(), 0);
    }

    void set_last_error(const wchar_t* message) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        last_error_ = message ? message : L"Unknown error.";
    }

    void set_last_error_ascii(const char* message) {
        set_last_error(from_utf8(message).c_str());
    }

    void set_mm_error(MMRESULT mmr, const wchar_t* prefix) {
        wchar_t mm_text[MAXERRORLENGTH] {};
        waveOutGetErrorTextW(mmr, mm_text, MAXERRORLENGTH);
        std::wstring message = prefix ? prefix : L"Audio output error.";
        message += L" ";
        message += mm_text;
        set_last_error(message.c_str());
    }

    static std::wstring from_utf8(const char* text) {
        if (!text || !*text) {
            return L"";
        }

        int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (needed <= 0) {
            needed = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
            if (needed <= 0) {
                return L"";
            }
            std::wstring wide(static_cast<size_t>(needed), L'\0');
            MultiByteToWideChar(CP_ACP, 0, text, -1, &wide[0], needed);
            wide.resize(static_cast<size_t>(needed - 1));
            return wide;
        }

        std::wstring wide(static_cast<size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text, -1, &wide[0], needed);
        wide.resize(static_cast<size_t>(needed - 1));
        return wide;
    }

    std::recursive_mutex mutex_;
    Music_Emu* emu_ = nullptr;
    HWAVEOUT wave_out_ = nullptr;
    WAVEFORMATEX wave_format_ {};
    std::vector<unsigned char> file_data_;
    std::vector<AudioBuffer> buffers_;
    std::thread worker_;
    HANDLE stop_event_ = nullptr;
    HANDLE buffer_event_ = nullptr;
    std::wstring last_error_;
    int sample_rate_ = 44100;
    int current_track_ = 0;
    int volume_percent_ = 100;
    bool loop_enabled_ = true;
    bool paused_ = false;
    bool worker_running_ = false;
};

InnoPlayer g_player;

} // namespace

extern "C" {

int GME_INNO_CALL GMEInnoOpenFileW(const wchar_t* path, int sample_rate) {
    return g_player.open_file(path, sample_rate);
}

int GME_INNO_CALL GMEInnoOpenFileTrackW(const wchar_t* path, int sample_rate, int track_index) {
    return g_player.open_file_track(path, sample_rate, track_index);
}

void GME_INNO_CALL GMEInnoClose(void) {
    g_player.close();
}

int GME_INNO_CALL GMEInnoStartTrack(int track_index) {
    return g_player.start_track(track_index);
}

int GME_INNO_CALL GMEInnoPlay(void) {
    return g_player.play();
}

void GME_INNO_CALL GMEInnoPause(void) {
    g_player.pause();
}

void GME_INNO_CALL GMEInnoStop(void) {
    g_player.stop();
}

int GME_INNO_CALL GMEInnoIsPlaying(void) {
    return g_player.is_playing();
}

void GME_INNO_CALL GMEInnoSetLoop(int enabled) {
    g_player.set_loop(enabled);
}

void GME_INNO_CALL GMEInnoSetVolume(int volume_percent) {
    g_player.set_volume(volume_percent);
}

int GME_INNO_CALL GMEInnoGetTrackCount(void) {
    return g_player.get_track_count();
}

int GME_INNO_CALL GMEInnoGetCurrentTrack(void) {
    return g_player.get_current_track();
}

int GME_INNO_CALL GMEInnoGetLastErrorW(wchar_t* buffer, int capacity) {
    return g_player.get_last_error(buffer, capacity);
}

int GME_INNO_CALL GMEInnoGetLastErrorLength(void) {
    return g_player.get_last_error_length();
}

}
