#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE 44100
#define PI 3.14159265359
#define MAX_VOICES 8
#define WAVE_BUF 1024

typedef enum {WAVE_SIN, WAVE_SAW, WAVE_SQUARE, WAVE_TRIANGLE} WaveType;
typedef enum {HARMONIC, INHARMONIC} FModType;

const char *wave_names[] = {"Sine", "Saw", "Square", "Triangle"};

typedef struct {
    int active;
    SDL_Keycode key;
    float freq;
    float phase;
    float ang_freq;
    WaveType wave;
} Voice;

typedef struct {
    float freq;
    float phase;
    float betta;
    float ratio; 
    WaveType fwave;
    FModType type;
} FMod;

typedef struct {
    SDL_Keycode key;
    float freq;
} KeyNote;

KeyNote keymap[] = {
    {SDLK_z, 220.0f},       // A3
    {SDLK_s, 233.1f},       // A#3
    {SDLK_x, 247.0f},       // B3
    {SDLK_c, 261.6f},       // C4
    {SDLK_f, 277.2f},       // C#4
    {SDLK_v, 293.7f},       // D4
    {SDLK_g, 311.1f},       // D#4
    {SDLK_b, 329.6f},       // E4
    {SDLK_n, 349.2f},       // F4
    {SDLK_j, 370.0f},       // F#4
    {SDLK_m, 392.0f},       // G4
    {SDLK_k, 415.3f},       // G#4
    {SDLK_COMMA, 440.0f},   // A4
    {SDLK_l, 466.2f},       // A#4
    {SDLK_PERIOD, 493.9f},  // B4
    {SDLK_SLASH, 523.3f},   // C5
};

int keymap_size = sizeof(keymap) / sizeof(KeyNote);
int last_key = -1;

Voice voices[MAX_VOICES];
FMod fm[MAX_VOICES];
float phase = 0;
int playing = 0;
int fmod_on = 0;
float wave_vis[WAVE_BUF];
int wave_pos = 0;

SDL_Color white = {240,240,240,255};
SDL_Color black = {30,30,30,255};
SDL_Rect button_add = {20, 20, 60, 40};

/* ==== octave ==== */
void octave_up();
void octave_down();

/* ==== poliphony ==== */
void init_voices();
int find_voice(SDL_Keycode key);
int alloc_voice();
int is_key_active(SDL_Keycode key);

/* ===== oscillator ===== */
float gen_voice(Voice *v);

/* ===== fmod ===== */
float process_fmod(FMod *f, Voice *v);
void add_fmod();

/* ===== audio ===== */
void audio_callback(void *u, Uint8 *stream, int len);

/* ===== UI ===== */
void draw_text(SDL_Renderer *r, TTF_Font *f, int x, int y, const char *txt, SDL_Color c);
void draw_wave(SDL_Renderer *r);
void draw_keyboard_hint(SDL_Renderer *r, TTF_Font *font);


int main()
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window *win = SDL_CreateWindow("Subtractive Synth",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 700, 700, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font *font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 18);
    TTF_Font *font_main = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial Black.ttf", 18);


    SDL_AudioSpec spec = {0};
    spec.freq = SAMPLE_RATE;
    spec.format = AUDIO_F32SYS;
    spec.channels = 1;
    spec.samples = 512;
    spec.callback = audio_callback;

    SDL_OpenAudio(&spec, NULL);
    SDL_PauseAudio(0);

    SDL_Event e;

    init_voices();
    int run = 1;

    while (run)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) run = 0;

            if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                int x = e.button.x;
                int y = e.button.y;

                if (x > button_add.x && x < button_add.x + button_add.w &&
                    y > button_add.y && y < button_add.y + button_add.h)
                {
                    add_fmod();
                }
            }

            if (e.type == SDL_KEYDOWN && !e.key.repeat)
            {
                if (e.key.keysym.sym == SDLK_ESCAPE) run = 0;

                if (e.key.keysym.sym == SDLK_EQUALS) add_fmod();

                if (e.key.keysym.sym == SDLK_TAB)
                    voices[0].wave = (voices[0].wave + 1) % 4;
                if (e.key.keysym.sym == SDLK_1) 
                    if(keymap[0].freq > 27.50f) octave_down();
                if (e.key.keysym.sym == SDLK_2) 
                    if (keymap[0].freq < 1760.0) octave_up();

                for (int i = 0; i < keymap_size; i++)
                {
                    if (e.key.keysym.sym == keymap[i].key)
                    {
                        int v = find_voice(keymap[i].key);
                        if (v < 0)
                            v = alloc_voice();

                        voices[v].active = 1;
                        voices[v].key = keymap[i].key;
                        voices[v].freq = keymap[i].freq;

                        if (fmod_on == 1 && fm[0].type == HARMONIC)
                            fm[v].freq = keymap[i].freq * fm[0].ratio;

                        break;
                    }
                }

                if (fmod_on == 1)
                {
                    FMod *f = &fm;

                    if (e.key.keysym.sym == SDLK_q) f->fwave = (f->fwave + 1) % 4;
                    if (e.key.keysym.sym == SDLK_w) f->type = (f->type + 1) % 2;
                    if (e.key.keysym.sym == SDLK_RIGHT && f->betta <1.0f) f->betta += 0.05f;
                    if (e.key.keysym.sym == SDLK_LEFT && f->betta > 0) f->betta -= 0.05f;

                    if (f->type == HARMONIC)
                    {
                        if (e.key.keysym.sym == SDLK_e && f->ratio > 1) f->ratio -= 1.0f;
                        if (e.key.keysym.sym == SDLK_r && f->ratio >= 1) f->ratio += 1.0f;
                        if (e.key.keysym.sym == SDLK_e && f->ratio <= 1) f->ratio = 1 / (pow(f->ratio, -1.0f) + 1);
                        if (e.key.keysym.sym == SDLK_r && f->ratio < 1) f->ratio = 1 / (pow(f->ratio, -1.0f) - 1);
                    }
                    if (e.key.keysym.sym == SDLK_BACKSPACE)
                    {
                        fmod_on = 0;
                        continue;
                    }
                }
            }
            if (e.type == SDL_KEYUP)
            {
                for (int i = 0; i < keymap_size; i++)
                {
                    if (e.key.keysym.sym == keymap[i].key)
                    {
                        for (int i = 0; i < MAX_VOICES; i++)
                        {
                            if (voices[i].active && voices[i].key == e.key.keysym.sym)
                            {
                                voices[i].active = 0;
                                break;
                            }
                        }
                    }
                }
            }
        }

        SDL_SetRenderDrawColor(ren, 20,20,20,255);
        SDL_RenderClear(ren);

        SDL_SetRenderDrawColor(ren, 60,60,60,240);
        SDL_RenderFillRect(ren, &button_add);
        draw_text(ren, font, 45, 28, "+", white);

        char wbuf[64];
        sprintf(wbuf, "Waveform: %s (TAB to change)", wave_names[voices[0].wave]);
        draw_text(ren, font_main, 120, 28, wbuf, white);

        int y = 90;

        if (fmod_on == 1)
        {
            SDL_SetRenderDrawColor(ren, 80,80,80,255);
            SDL_Rect hl = {20, y - 4, 640, 60};
            SDL_RenderFillRect(ren, &hl);
        }

        if(fmod_on == 1 && fm[0].type == INHARMONIC)
        {
            char buf[128];
            sprintf(buf, "Inharmonic modulator");

            draw_text(ren, font_main, 30, y, buf, white);
            y += 30;
            sprintf(buf, "Waveform: %s  Freq=%.0fHz  Frequency modulation index=%.1f",
                wave_names[fm[0].fwave], fm[0].freq, fm[0].betta);

            draw_text(ren, font, 30, y, buf, white);
        }
        else if (fmod_on == 1 && fm[0].type == HARMONIC)
        {   
            char buf[128];
            sprintf(buf, "Harmonic modulator");

            draw_text(ren, font_main, 30, y, buf, white);
            y += 30;
            sprintf(buf, "Waveform: %s  Ratio:%.3f  Frequency modulation index=%.1f",
                wave_names[fm[0].fwave], fm[0].ratio, fm[0].betta);

            draw_text(ren, font, 30, y, buf, white);
        }

        draw_text(ren, font, 200, 640,
            " + add modulator | 1/2 octave | ESC exit", white);

        draw_text(ren, font, 70, 670,    
            "Q modulator waveform | W modulator type | arrows freq/amp | A/S ratio", white);
        
        draw_wave(ren);
        draw_keyboard_hint(ren, font);

        SDL_RenderPresent(ren);
        SDL_Delay(16);

    }

    SDL_CloseAudio();
    SDL_Quit();
}
/* ==== octave ==== */

void octave_up()
{
    for (int i = 0; i < keymap_size; i++)
    {
        keymap[i].freq *= 2;
    }
}

void octave_down()
{
    for (int i = 0; i < keymap_size; i++)
    {
        keymap[i].freq /= 2;
    }
}
/* ==== poliphony ==== */

void init_voices()
{
    for (int i = 0; i < MAX_VOICES; i++)
    {    
        voices[i].active = 0;
        voices[i].wave = WAVE_SIN;
    }
}

int find_voice(SDL_Keycode key)
{
    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].active && voices[i].key == key)
            return i;
    return -1;
}

int alloc_voice()
{
    for (int i = 0; i < MAX_VOICES; i++)
        if (!voices[i].active)
            return i;
    return 0; 
}

int is_key_active(SDL_Keycode key)
{
    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].active && voices[i].key == key)
            return 1;
    return 0;
}


/* ===== oscillator ===== */
float gen_voice(Voice *v)
{
    v->phase += 2 * PI * v->freq / SAMPLE_RATE;

    if (v->phase > 2 * PI) v->phase -= 2 * PI;

    if (v->wave == WAVE_SIN) return sinf(v->phase);
    if (v->wave == WAVE_SQUARE) return sinf(v->phase) > 0 ? 1.0f : -1.0f;
    if (v->wave == WAVE_TRIANGLE) return asinf(sinf(v->phase)) * 2 / PI;
    if (v->wave == WAVE_SAW) return 2.0f * (v->phase / (2 * PI)) - 1.0f;
}

/* ===== frequency modulation  ===== */

float process_fmod(FMod *f, Voice *v)
{
    f->phase += 2 * PI * f->freq / SAMPLE_RATE;
    if (f->phase > 2 * PI) f->phase -= 2 * PI;
    
    v->phase += 2 * PI * v->freq / SAMPLE_RATE;
    if (v->phase > 2 * PI) v->phase -= 2 * PI;

    float mod; 

    if (f->fwave == WAVE_SIN) mod = sinf(f->phase);
    if (f->fwave == WAVE_SQUARE) mod = (sinf(f->phase) > 0 ? 1.0f : -1.0f);
    if (f->fwave == WAVE_TRIANGLE) mod = asinf(sinf(f->phase)) * 2 / PI;
    if (f->fwave == WAVE_SAW) mod = 2.0f * (f->phase / (2 * PI)) - 1.0f;

    if (v->wave == WAVE_SIN) return sinf(v->phase + f->betta * mod);
    if (v->wave == WAVE_SQUARE) return sinf(v->phase + f->betta * mod) > 0 ? 1.0f : -1.0f;
    if (v->wave == WAVE_TRIANGLE) return asinf(sinf(v->phase + f->betta * mod)) * 2 / PI;
    if (v->wave == WAVE_SAW) return 2.0f * ((v->phase + f->betta * mod) / (2 * PI)) - 1.0f;
}

void add_fmod()
{
    if (fmod_on == 1) return;

    fm[0].freq = 20.0f;
    fm[0].betta = 0.2f;
    fm[0].phase = 0.0f;
    fm[0].ratio = 1.0f;
    fm[0].fwave = WAVE_SIN;
    fm[0].type = INHARMONIC;

    fmod_on = 1;
}

/* ===== audio ===== */
void audio_callback(void *u, Uint8 *stream, int len)
{
    float *buf = (float *)stream;
    int samples = len / sizeof(float);

    for (int i = 0; i < samples; i++)
    {
        float s = 0.0f;

        for (int v = 0; v < MAX_VOICES; v++)
        {  
            if (!voices[v].active) continue;

            if(fmod_on)
                s += process_fmod(&fm, &voices[v]);
            else if (voices[v].active)
                s += gen_voice(&voices[v]);
        }

        buf[i] = s * 0.5f;

        wave_vis[wave_pos] = s;
        wave_pos = (wave_pos + 1) % WAVE_BUF;

    }
}

/* ===== UI ===== */

void draw_text(SDL_Renderer *r, TTF_Font *f, int x, int y, const char *txt, SDL_Color c)
{
    SDL_Surface *s = TTF_RenderText_Solid(f, txt, c);
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect dst = {x, y, s->w, s->h};
    SDL_RenderCopy(r, t, NULL, &dst);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

void draw_wave(SDL_Renderer *r)
{
    int ox = 10;
    int oy = 300;
    int w  = 680;
    int h  = 180;

    SDL_SetRenderDrawColor(r, 40,40,40,255);
    SDL_Rect bg = {ox, oy, w, h};
    SDL_RenderFillRect(r, &bg);

    int start = -1;

    for (int i = 1; i < WAVE_BUF; i++)
    {
        int i0 = (wave_pos - i - 1 + WAVE_BUF) % WAVE_BUF;
        int i1 = (wave_pos - i + WAVE_BUF) % WAVE_BUF;

        if (wave_vis[i0] < 0.0f && wave_vis[i1] >= 0.0f)
        {
            start = i1;
            break;
        }
    }

    if (start < 0)
        start = wave_pos;

    SDL_SetRenderDrawColor(r, 255,192,203,240);

    for (int i = 0; i < WAVE_BUF - 1; i++)
    {
        int i1 = (start + i) % WAVE_BUF;
        int i2 = (start + i + 1) % WAVE_BUF;

        float v1 = wave_vis[i1];
        float v2 = wave_vis[i2];

        int x1 = ox + i * w / WAVE_BUF;
        int x2 = ox + (i + 1) * w / WAVE_BUF;

        int y1 = oy + h/2 - (int)(v1 * h/2);
        int y2 = oy + h/2 - (int)(v2 * h/2);

        SDL_RenderDrawLine(r, x1, y1, x2, y2);
    }
}

void draw_keyboard_hint(SDL_Renderer *r, TTF_Font *font)
{
    int x0 = 80;
    int y0 = 500;

    int white_w = 55;
    int white_h = 120;

    int black_w = 35;
    int black_h = 70;

    const char *white_labels[] = {"Z","X","C","V","B","N","M",",",".","/"};

    const int black_pos[] = {0,2,3,5,6,7};
    const char *black_labels[] = {"S","F","G","J","K","L"};

    SDL_Keycode white_keys[] = 
    {
        SDLK_z, SDLK_x, SDLK_c, SDLK_v, SDLK_b,
        SDLK_n, SDLK_m, SDLK_COMMA, SDLK_PERIOD, SDLK_SLASH
    };

    SDL_Keycode black_keys[] = 
    {
        SDLK_s, SDLK_f, SDLK_g, SDLK_j, SDLK_k, SDLK_l
    };


    /* === white keys === */
    for (int i = 0; i < 10; i++)
    {
        SDL_Rect k = {
            x0 + i * white_w,
            y0,
            white_w,
            white_h
        };

        if (is_key_active(white_keys[i]))
            SDL_SetRenderDrawColor(r, 194, 155, 162, 255); 
        else
            SDL_SetRenderDrawColor(r, 230, 230, 230,255);

        SDL_RenderFillRect(r, &k);

        SDL_SetRenderDrawColor(r, 0,0,0,255);
        SDL_RenderDrawRect(r, &k);

        draw_text(
            r,
            font,
            k.x + white_w / 2 - 8,
            k.y + white_h - 28,
            white_labels[i],
            black
        );
    }

    /* === black keys === */
    for (int i = 0; i < 6; i++)
    {
        SDL_Rect k = {
            x0 + (black_pos[i] + 1) * white_w - black_w / 2,
            y0,
            black_w,
            black_h
        };

        if (is_key_active(black_keys[i]))
            SDL_SetRenderDrawColor(r, 150, 120, 126,255); 
        else
            SDL_SetRenderDrawColor(r, 40,40,40,255);

        SDL_RenderFillRect(r, &k);

        draw_text(
            r,
            font,
            k.x + black_w / 2 - 6,
            k.y + black_h - 26,
            black_labels[i],
            white
        );
    }
}

