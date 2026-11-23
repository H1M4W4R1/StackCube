#include "Audio.h"
#include "WavFile.h"

#include <M5Unified.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <esp_heap_caps.h>
#include <memory>

namespace stackcube::audio {
namespace {
constexpr size_t NameCapacity = 121;
constexpr size_t MusicFrames = 2048;
constexpr size_t MusicBufferBytes = MusicFrames * 2 * sizeof(int16_t);
constexpr size_t ReadBytes = 4096;
constexpr size_t MaxEffectBytes = WavFile::SampleRate * 2 * sizeof(int16_t);
constexpr int MusicChannel = 0;
constexpr unsigned EffectVoices = 4;
constexpr unsigned CachedEffects = 8;
constexpr int MusicLevel = 255;
constexpr int DuckedMusicLevel = 128;
constexpr int EffectLevel = 190;
constexpr uint32_t EffectTailMs = 30;

using Samples = std::unique_ptr<int16_t, decltype(&heap_caps_free)>;

int16_t* allocateSamples(size_t bytes) {
    return static_cast<int16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

struct MusicBuffer {
    Samples pcm{nullptr, heap_caps_free};
    std::atomic<bool> playing{false};
    size_t samples = 0;
};

struct Effect {
    char name[NameCapacity] = {};
    Samples pcm{nullptr, heap_caps_free};
    size_t bytes = 0;
    unsigned pending = 0;
    bool stereo = false;
    bool ready() const { return bytes != 0; }
};

String folder;
bool enabled;
WavFile music;
char requestedMusic[NameCapacity];
char currentMusic[NameCapacity];
bool stopMusic;
// M5Unified owns up to two blocks. Two more can be filled ahead of playback.
std::array<MusicBuffer, 4> musicBuffers;
unsigned nextMusicBuffer;
unsigned nextMusicPlayback;
bool musicPrefilled;
size_t musicFill;
int musicLevel;
int normalMusicLevel;
int duckedMusicLevel;
bool effectsEnabled;
uint32_t lastMusicLevelUpdate;
uint32_t lastEffect;

std::array<Effect, CachedEffects> effects;
std::array<std::atomic<const int16_t*>, EffectVoices> playingEffects{};
unsigned nextEviction;
WavFile effectFile;
Effect* loadingEffect;

bool validName(const char* name) {
    if (!name || *name == '.') return false;
    const size_t length = strnlen(name, NameCapacity);
    return length > 4 && length < NameCapacity && !strpbrk(name, "/\\:") &&
           !strcasecmp(name + length - 4, ".wav");
}

bool openWave(WavFile& wave, const char* name) {
    const String path = folder + '/' + name;
    if (wave.open(path.c_str())) return true;
    Serial.printf("Audio: missing or invalid 48 kHz PCM WAV: %s\n", path.c_str());
    return false;
}

void releaseBuffer(void*, const void* data, uint8_t channel) {
    if (channel == MusicChannel) {
        for (auto& buffer : musicBuffers) {
            if (buffer.pcm.get() == data) buffer.playing.store(false, std::memory_order_release);
        }
    } else if (channel <= EffectVoices) {
        playingEffects[channel - 1].store(nullptr, std::memory_order_release);
    }
}

void closeMusic() {
    music.close();
    currentMusic[0] = '\0';
    musicFill = 0;
    nextMusicBuffer = nextMusicPlayback = 0;
    musicPrefilled = false;
    for (auto& buffer : musicBuffers) buffer.samples = 0;
}

void queueMusic() {
    if (!musicPrefilled) {
        // Start with two complete blocks, including for stereo tracks and slow SD cards.
        const auto& second = musicBuffers[(nextMusicPlayback + 1) % musicBuffers.size()];
        if (!second.samples) return;
        musicPrefilled = true;
    }
    while (M5.Speaker.isPlaying(MusicChannel) < 2) {
        auto& buffer = musicBuffers[nextMusicPlayback];
        if (!buffer.samples) return;
        buffer.playing.store(true, std::memory_order_relaxed);
        if (!M5.Speaker.playRaw(buffer.pcm.get(), buffer.samples, WavFile::SampleRate,
                                music.stereo(), 1, MusicChannel)) {
            buffer.playing.store(false, std::memory_order_release);
            Serial.println("Audio: could not queue background track samples");
            closeMusic();
            return;
        }
        buffer.samples = 0;
        nextMusicPlayback = (nextMusicPlayback + 1) % musicBuffers.size();
    }
}

// Report whether music used this poll's SD budget.
bool serviceMusic() {
    if (stopMusic) {
        closeMusic();
        stopMusic = false;
    }
    if (requestedMusic[0]) {
        WavFile next;
        if (openWave(next, requestedMusic)) {
            closeMusic();
            music = std::move(next);
            strcpy(currentMusic, requestedMusic);
            Serial.printf("Audio: looping %s\n", currentMusic);
        }
        requestedMusic[0] = '\0';
        return true;
    }
    if (!music) return false;
    queueMusic();
    if (!music) return false;

    auto& buffer = musicBuffers[nextMusicBuffer];
    if (buffer.samples || buffer.playing.load(std::memory_order_acquire)) return false;
    auto* bytes = reinterpret_cast<uint8_t*>(buffer.pcm.get());
    const size_t blockBytes = MusicFrames * sizeof(int16_t) * (music.stereo() ? 2 : 1);
    // Expand very short loops in RAM, retaining an integer number of repetitions.
    const size_t target = music.size() < blockBytes ? blockBytes / music.size() * music.size() : blockBytes;
    if (!music.remaining() && !music.rewind()) {
        Serial.println("Audio: could not rewind background track");
        closeMusic();
        return true;
    }
    const size_t amount = std::min({target - musicFill, music.remaining(), ReadBytes});
    if (!music.read(bytes + musicFill, amount)) {
        Serial.println("Audio: background track read failed");
        closeMusic();
        return true;
    }
    musicFill += amount;
    if (music.size() < blockBytes && musicFill == music.size()) {
        while (musicFill < target) {
            const size_t copy = std::min(musicFill, target - musicFill);
            memcpy(bytes + musicFill, bytes, copy);
            musicFill += copy;
        }
    }
    // A partial block stays staged until the next reads fill it.
    if (musicFill == target) {
        buffer.samples = target / sizeof(int16_t);
        musicFill = 0;
        nextMusicBuffer = (nextMusicBuffer + 1) % musicBuffers.size();
        queueMusic();
    }
    return true;
}

bool inUse(const Effect& effect) {
    if (!effect.ready()) return false;
    for (auto& voice : playingEffects) {
        if (voice.load(std::memory_order_acquire) == effect.pcm.get()) return true;
    }
    return false;
}

Effect* reserveEffect(const char* name) {
    for (auto& effect : effects) {
        if (!strcasecmp(effect.name, name)) return &effect;
    }
    for (unsigned offset = 0; offset < effects.size(); ++offset) {
        const unsigned index = (nextEviction + offset) % effects.size();
        auto& effect = effects[index];
        if (&effect == loadingEffect || effect.pending || inUse(effect)) continue;
        effect = {};
        strcpy(effect.name, name);
        nextEviction = (index + 1) % effects.size();
        return &effect;
    }
    return nullptr;
}

bool openEffect(Effect& effect, WavFile& wave) {
    if (!openWave(wave, effect.name)) return false;
    if (wave.size() > MaxEffectBytes) {
        Serial.printf("Audio: effect exceeds %u bytes: %s\n", unsigned(MaxEffectBytes), effect.name);
        wave.close();
        return false;
    }
    effect.pcm.reset(allocateSamples(wave.size()));
    if (!effect.pcm) {
        Serial.println("Audio: could not allocate effect buffer");
        wave.close();
        return false;
    }
    return true;
}

bool readEffect(Effect& effect, WavFile& wave, size_t budget) {
    auto* write = reinterpret_cast<uint8_t*>(effect.pcm.get()) + wave.size() - wave.remaining();
    if (!wave.read(write, std::min(budget, wave.remaining()))) {
        Serial.printf("Audio: effect read failed: %s\n", effect.name);
        wave.close();
        effect = {};
        return false;
    }
    if (!wave.remaining()) {
        effect.bytes = wave.size();
        effect.stereo = wave.stereo();
        wave.close();
    }
    return true;
}

void loadPendingEffect() {
    if (!loadingEffect) {
        for (auto& effect : effects) {
            if (effect.pending && !effect.ready()) {
                if (openEffect(effect, effectFile)) loadingEffect = &effect;
                else effect = {};
                return;
            }
        }
        return;
    }
    readEffect(*loadingEffect, effectFile, ReadBytes);
    if (!effectFile) loadingEffect = nullptr;
}

void playPendingEffects(uint32_t now) {
    for (auto& effect : effects) {
        if (!effect.ready() || !effect.pending) continue;
        for (unsigned voice = 0; voice < EffectVoices && effect.pending; ++voice) {
            if (playingEffects[voice].load(std::memory_order_acquire)) continue;
            const auto* pcm = effect.pcm.get();
            playingEffects[voice].store(pcm, std::memory_order_relaxed);
            if (M5.Speaker.playRaw(pcm, effect.bytes / sizeof(int16_t), WavFile::SampleRate,
                                   effect.stereo, 1, voice + 1)) {
                lastEffect = now;
            } else {
                playingEffects[voice].store(nullptr, std::memory_order_release);
            }
            --effect.pending;
        }
        effect.pending = 0;
    }
}

void updateMusicLevel(uint32_t now) {
    for (const auto& voice : playingEffects) {
        if (voice.load(std::memory_order_acquire)) {
            lastEffect = now;
            break;
        }
    }
    // The mixer can release a buffer before I2S has played the last samples.
    const bool effectAudible = now - lastEffect < EffectTailMs;
    const int elapsed = std::min(now - lastMusicLevelUpdate, uint32_t{128});
    lastMusicLevelUpdate = now;
    const int level = effectAudible ? std::max(duckedMusicLevel, musicLevel - 16 * elapsed)
                                    : std::min(normalMusicLevel, musicLevel + elapsed);
    if (level != musicLevel) {
        musicLevel = level;
        M5.Speaker.setChannelVolume(MusicChannel, musicLevel);
    }
}
}

bool begin(const GameLaunch& launch) {
    if (enabled) return true;
    const auto& levels = launch.audioLevels;
    if (launch.audioOutput == AudioOutput::Off || !levels.audible()) return false;
    if (levels.music) {
        for (auto& buffer : musicBuffers) {
            buffer.pcm.reset(allocateSamples(MusicBufferBytes));
            if (!buffer.pcm) {
                end();
                return false;
            }
        }
    }
    auto config = M5.Speaker.config();
    config.sample_rate = WavFile::SampleRate;
    config.dma_buf_count = 4;
    config.task_pinned_core = xPortGetCoreID() == 0 ? 1 : 0;
    const bool hdmi = launch.audioOutput == AudioOutput::Hdmi;
    if (hdmi) {
        // M126 takes I2S on M-Bus pins 21, 23 and 24. Pin 22 is the display CS.
        config.pin_bck = M5.getPin(m5::pin_name_t::mbus_pin21);
        config.pin_data_out = M5.getPin(m5::pin_name_t::mbus_pin23);
        config.pin_ws = M5.getPin(m5::pin_name_t::mbus_pin24);
        config.stereo = true;
        config.magnification = 16;
    }
    M5.Speaker.config(config);
    const unsigned outputGain = hdmi ? 115 : 230;
    M5.Speaker.setVolume(outputGain * levels.master / MaxAudioVolume);
    normalMusicLevel = MusicLevel * levels.music / MaxAudioVolume;
    duckedMusicLevel = DuckedMusicLevel * levels.music / MaxAudioVolume;
    musicLevel = normalMusicLevel;
    effectsEnabled = levels.effects != 0;
    lastMusicLevelUpdate = millis();
    lastEffect = lastMusicLevelUpdate - EffectTailMs;
    M5.Speaker.setAllChannelVolume(EffectLevel * levels.effects / MaxAudioVolume);
    M5.Speaker.setChannelVolume(MusicChannel, musicLevel);
    M5.Speaker.setBufferReleaseCallback(nullptr, releaseBuffer);
    folder = launch.folder;
    enabled = M5.Speaker.begin();
    if (enabled) {
        Serial.printf("Audio: %s at %u%% (music %u%%, effects %u%%)\n", audioOutputName(launch.audioOutput),
                      unsigned(levels.master), unsigned(levels.music), unsigned(levels.effects));
    } else {
        M5.Speaker.end();
        end();
    }
    return enabled;
}

void end() {
    if (enabled) M5.Speaker.end();
    enabled = false;
    closeMusic();
    requestedMusic[0] = '\0';
    stopMusic = false;
    for (auto& buffer : musicBuffers) {
        buffer.playing.store(false, std::memory_order_relaxed);
        buffer.pcm.reset();
    }
    effectFile.close();
    loadingEffect = nullptr;
    for (auto& voice : playingEffects) voice.store(nullptr, std::memory_order_relaxed);
    for (auto& effect : effects) effect = {};
    nextEviction = 0;
}

void poll() {
    if (!enabled) return;
    const uint32_t now = millis();
    playPendingEffects(now);
    updateMusicLevel(now);
    if (!serviceMusic()) loadPendingEffect();
}

bool preloadSfx(const char* name) {
    if (!enabled || !effectsEnabled || !validName(name)) return false;
    auto* effect = reserveEffect(name);
    if (!effect) return false;
    if (effect->ready()) return true;
    if (effect == loadingEffect) {
        loadingEffect = nullptr;
        return readEffect(*effect, effectFile, effectFile.remaining());
    }
    WavFile wave;
    if (openEffect(*effect, wave)) return readEffect(*effect, wave, wave.remaining());
    *effect = {};
    return false;
}

bool playSfx(const char* name) {
    if (!enabled || !effectsEnabled || !validName(name)) return false;
    auto* effect = reserveEffect(name);
    if (!effect || effect->pending == EffectVoices) return false;
    ++effect->pending;
    return true;
}

bool playBgTrack(const char* name) {
    if (!enabled || !normalMusicLevel || !validName(name)) return false;
    stopMusic = false;
    if (!strcasecmp(currentMusic, name)) requestedMusic[0] = '\0';
    else strcpy(requestedMusic, name);
    return true;
}

void stopBgTrack() {
    requestedMusic[0] = '\0';
    stopMusic = true;
}

}
