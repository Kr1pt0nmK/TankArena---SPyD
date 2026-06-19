#ifndef AUDIO_H
#define AUDIO_H

/* Capa de sonido multiplataforma (encima de miniaudio). Reproduce efectos
   cortos "dispara y olvida" que se pueden encimar. Si no hay tarjeta de sonido,
   todo se vuelve no-op sin romper el juego. */

typedef enum {
    SND_SHOT = 0,    /* disparo de tanque (cañon) */
    SND_EXPLOSION,   /* tanque explotando (metalico) */
    SND_SPLAT,       /* muerte de un soldado a pie (aguado) */
    SND_COUNT
} SoundId;

/* Carga los .wav desde 'assets_dir' y arranca la musica de fondo en bucle.
   Devuelve 0 si el audio quedo listo (si falla, todo se vuelve no-op). */
int  audio_init(const char *assets_dir);
void audio_play(SoundId id);
void audio_music(int on);   /* enciende/apaga la musica de fondo */
void audio_shutdown(void);

#endif /* AUDIO_H */
