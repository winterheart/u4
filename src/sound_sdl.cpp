/*
 * sound_sdl.cpp
 */

#include <SDL.h>
#include <SDL_mixer.h>

#include "sound.h"
#include "config.h"
#include "debug.h"
#include "error.h"
#include "music.h"
#include "settings.h"
#include "u4file.h"

extern int u4_SDL_InitSubSystem(Uint32 flags);
extern void u4_SDL_QuitSubSystem(Uint32 flags);


Music* musicMgr = NULL;
std::vector<Mix_Chunk *> soundChunk;

int soundInit(void)
{
    ASSERT(! musicMgr, "soundInit called more than once");
    musicMgr = new Music;
    soundChunk.resize(SOUND_MAX, NULL);
    return 1;
}

void soundDelete(void)
{
    delete musicMgr;
}

static bool sound_load(Sound sound) {
    if (soundChunk[sound] == NULL) {
        const char* pathname = configService->soundFile(sound);
        if (pathname) {
            soundChunk[sound] = Mix_LoadWAV(pathname);
            if (!soundChunk[sound]) {
                errorWarning("Unable to load sound file %s: %s",
                             pathname, Mix_GetError());
                return false;
            }
        }
    }
    return true;
}

void soundPlay(Sound sound, bool onlyOnce, int specificDurationInTicks) {
    ASSERT(sound < SOUND_MAX, "Attempted to play an invalid sound in soundPlay()");

    // If music didn't initialize correctly, then we can't play it anyway
    if (!Music::functional || !settings.soundVol)
        return;

    if (soundChunk[sound] == NULL)
    {
        if (!sound_load(sound))
            return;
    }

    /**
     * Use Channel 1 for sound effects
     */
    if (!onlyOnce || !Mix_Playing(1)) {
        if (Mix_PlayChannelTimed(1, soundChunk[sound],
                    specificDurationInTicks == -1 ? 0 : -1,
                    specificDurationInTicks) == -1)
            fprintf(stderr, "Error playing sound %d: %s\n",
                    sound, Mix_GetError());
    }
}

void soundStop(int channel) {
    // If music didn't initialize correctly, then we shouldn't try to stop it
    if (!musicMgr->functional || !settings.soundVol)
        return;

    if (Mix_Playing(channel))
        Mix_HaltChannel(channel);
}


void Music::create_sys() {
    /*
     * initialize sound subsystem
     */
    TRACE_LOCAL(*logger, "Initializing SDL sound subsystem");

    int audio_rate = 22050;
    Uint16 audio_format = AUDIO_S16LSB; /* 16-bit stereo */
    int audio_channels = 2;
    int audio_buffers = 1024;

    if (u4_SDL_InitSubSystem(SDL_INIT_AUDIO) == -1) {
        errorWarning("unable to init SDL audio subsystem: %s",
                SDL_GetError());
        this->functional = false;
        return;
    }

    TRACE_LOCAL(*logger, "Opening audio");

    if (Mix_OpenAudio(audio_rate, audio_format, audio_channels,
            audio_buffers)) {
        fprintf(stderr, "Unable to open audio!\n");
        this->functional = false;
        return;
    }
    this->functional = true;
    TRACE_LOCAL(*logger, "Allocating channels");

    Mix_AllocateChannels(16);
}

void Music::destroy_sys() {
    if (playing) {
        TRACE_LOCAL(*logger, "Stopping currently playing music");
        Mix_FreeMusic(playing);
        playing = NULL;
    }

    TRACE_LOCAL(*logger, "Closing audio");
    Mix_CloseAudio();

    TRACE_LOCAL(*logger, "Quitting SDL audio subsystem");
    u4_SDL_QuitSubSystem(SDL_INIT_AUDIO);

}

bool Music::load_sys(const char* pathname) {
    if (playing) {
        Mix_FreeMusic(playing);
        playing = NULL;
    }

    playing = Mix_LoadMUS(pathname);
    if (!playing) {
        errorWarning("unable to load music file %s: %s", pathname,
                Mix_GetError());
        return false;
    }
    return true;
}

/**
 * Play a midi file
 */
void Music::playMid(int music) {
    if (!functional || !musicEnabled)
        return;

    /* loaded a new piece of music */
    if (load(music)) {
        Mix_PlayMusic(playing, NLOOPS);
        //Mix_SetMusicPosition(0.0);  //Could be useful if music was stored on different 'it/mod' patterns
    }
}

/**
 * Stop playing a MIDI file.
 */
void Music::stopMid()
{
    Mix_HaltMusic();
}

/**
 * Set, increase, and decrease sound volume
 */
void Music::setSoundVolume(int volume) {
    // Use Channel 1 for sound effects
    Mix_Volume(1, int((float)MIX_MAX_VOLUME / MAX_VOLUME * volume));
}

/**
 * Returns true if music is playing.
 */
bool Music::isPlaying()
{
    return Mix_PlayingMusic();
}

/**
 * Set, increase, and decrease music volume
 */
void Music::setMusicVolume(int volume) {
    Mix_VolumeMusic(int((float)MIX_MAX_VOLUME / MAX_VOLUME * volume));
}

void Music::fadeIn_sys(int msecs, bool loadFromMap) {
    if (Mix_FadeInMusic(playing, NLOOPS, msecs) == -1)
        errorWarning("Mix_FadeInMusic: %s\n", Mix_GetError());
}

void Music::fadeOut_sys(int msecs) {
    if (Mix_FadeOutMusic(msecs) == -1)
        errorWarning("Mix_FadeOutMusic: %s\n", Mix_GetError());
}
