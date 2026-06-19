/* Implementacion de la capa de sonido sobre miniaudio.
   miniaudio se compila aqui (una sola vez) con MINIAUDIO_IMPLEMENTATION. */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio.h"
#include <stdio.h>

static ma_engine g_engine;
static int       g_ready = 0;

static char      g_path[SND_COUNT][512];          /* rutas de los efectos */
static ma_sound  g_music;
static int       g_music_ok = 0;

/* nombre de archivo de cada efecto (mismo orden que SoundId) */
static const char *FILES[SND_COUNT] = {
    "shot.wav",       /* SND_SHOT */
    "explosion.wav",  /* SND_EXPLOSION */
    "splat.wav",      /* SND_SPLAT */
};

int audio_init(const char *assets_dir)
{
    if (ma_engine_init(NULL, &g_engine) != MA_SUCCESS)
        return -1;                 /* sin tarjeta de sonido: queda no-op */
    g_ready = 1;

    for (int i = 0; i < SND_COUNT; i++)
        snprintf(g_path[i], sizeof(g_path[i]), "%s/%s", assets_dir, FILES[i]);

    /* musica de fondo en bucle (streaming, volumen moderado) */
    char mpath[512];
    snprintf(mpath, sizeof(mpath), "%s/music.wav", assets_dir);
    if (ma_sound_init_from_file(&g_engine, mpath, MA_SOUND_FLAG_STREAM,
                                NULL, NULL, &g_music) == MA_SUCCESS) {
        g_music_ok = 1;
        ma_sound_set_looping(&g_music, MA_TRUE);
        ma_sound_set_volume(&g_music, 0.45f);
        ma_sound_start(&g_music);
    }
    return 0;
}

void audio_play(SoundId id)
{
    if (!g_ready || id < 0 || id >= SND_COUNT) return;
    /* dispara y olvida: cada llamada crea una voz nueva, asi se enciman */
    ma_engine_play_sound(&g_engine, g_path[id], NULL);
}

void audio_music(int on)
{
    if (!g_music_ok) return;
    if (on) ma_sound_start(&g_music);
    else    ma_sound_stop(&g_music);
}

void audio_shutdown(void)
{
    if (!g_ready) return;
    if (g_music_ok) ma_sound_uninit(&g_music);
    ma_engine_uninit(&g_engine);
    g_ready = 0;
}
