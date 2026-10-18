// MegaRace 2 CD / video / audio support
#pragma once
#include <stdint.h>
#include <stddef.h>

// ---- ISO9660 (MEGARACE2.gog is a plain 2048-byte-sector ISO image)
int iso_open(const char *path);                 // 1 = ok
int iso_is_open(void);
// load a whole file from the CD ("TRACK1/TIBET.UBB"), case-insensitive. caller frees with sys_free.
uint8_t *iso_load(const char *path, size_t *size);
// reading a big file in steps (loading bar): begin, then step until it returns 1 (or -1 on error)
struct IsoStream { uint64_t off; uint32_t size, done; uint8_t *buf; };
int iso_stream_begin(const char *path, IsoStream *s);
int iso_stream_step(IsoStream *s, uint32_t bytes);
// enumerate files in a CD directory ("BOYLE"). cb gets the bare file name.
void iso_list(const char *dir, void (*cb)(void *user, const char *name, uint32_t size), void *user);

void iso_list_all(const char *dir,void (*cb)(void *,const char *,int),void *user);

// ---- M.EXE: the original game executable is loaded (LE, DOS4GW) to drive the original video decoder
int mexe_load(const char *path);                // 1 = ok
int mexe_engine_profile(int car,int *level);    // Original car -> sample profile, original 0..127 gain
int mexe_engine_frequency(int raw_speed);       // Original 1400-entry playback/gear curve (Hz)

// ---- UBB video (Cryo "UBB2" / track videos)
struct Video {
  unsigned generation;
  uint8_t *data; size_t size;
  int w, h, nframes, fps; double fps_exact;
  uint32_t *frame_ofs; uint8_t *keyframe; uint16_t *pal_id;
  uint8_t (*pals)[768]; int npals;
  int16_t *pcm; int pcm_frames;                // interleaved stereo, 22050 Hz
  int decoded_frame;                            // last frame decoded into the decoder buffers (-1 none)
  uint8_t *rgba;                                // w*h*4 output
};
int video_open(Video *v, uint8_t *data, size_t size, int track_height,int decode_audio=1);  // takes ownership of data
int video_decode(Video *v, int frame);           // decodes (seeking via keyframes) and fills v->rgba
void video_close(Video *v);

// ---- RIFF WAV (music)
int16_t *wav_decode(const uint8_t *d, size_t n, int *frames, int *channels, int *rate);
