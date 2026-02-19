#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE 44100
#define PI 3.14159265359
#define MAX_FILTERS 5
#define MAX_VOICES 8
#define WAVE_BUF 1024

typedef enum {WAVE_SAW, WAVE_SQUARE, WAVE_TRIANGLE} WaveType;
typedef enum {LPF, HPF, BPF, NOTCH} FilterType;

const char *wave_names[] = {"Saw", "Square", "Triangle"};
const char *filter_names[] = {"LPF", "HPF", "BPF", "Notch"};

typedef struct {
    FilterType type;

    float cutoff;   
    float res;      

    float b0, b1, b2;
    float a1, a2;

    float x1, x2;
    float y1, y2;
} Filter;


typedef struct {
    int active;
    SDL_Keycode key;
    float freq;
    float phase;
} Voice;

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
WaveType wave = WAVE_SAW;
Filter filters[MAX_FILTERS];
int filter_count = 0;
int selected = -1;
float phase = 0;
float current_freq = 0.0f;
int playing = 0;
float wave_vis[WAVE_BUF];
int wave_pos = 0;

SDL_Color white = {240,240,240};
SDL_Color black = {30,30,30};
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

/* ===== filter ===== */
void update_filter(Filter *f);
float process_filter(Filter *f, float x);
void add_filter();
void remove_filter(int index);

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
                    add_filter();
                }

                int start_y = 90;
                for (int i = 0; i < filter_count; i++)
                {
                    if (y > start_y + i * 35 && y < start_y + i * 35 + 30)
                        selected = i;
                }
            }

            if (e.type == SDL_KEYDOWN && !e.key.repeat)
            {
                if (e.key.keysym.sym == SDLK_ESCAPE) run = 0;

                if (e.key.keysym.sym == SDLK_EQUALS) add_filter();

                if (e.key.keysym.sym == SDLK_TAB)
                    wave = (wave + 1) % 3;
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

                        break;
                    }
                }

                if (selected >= 0)
                {
                    Filter *f = &filters[selected];

                    if (e.key.keysym.sym == SDLK_q)
                    {
                        f->type = (f->type + 1) % 4;
                        update_filter(f);
                    }
                    if (e.key.keysym.sym == SDLK_UP) 
                    {
                        f->cutoff += 100;
                        update_filter(f);
                    }
                    if (e.key.keysym.sym == SDLK_DOWN && f->cutoff > 50) 
                    {
                        f->cutoff -= 100;
                        update_filter(f);
                    }
                    if (e.key.keysym.sym == SDLK_RIGHT) 
                    {
                        f->res += 0.05f;
                        update_filter(f);
                    }
                    if (e.key.keysym.sym == SDLK_LEFT && f->res > 0) 
                    {
                        f->res -= 0.05f;
                        update_filter(f);
                    }
                    if (e.key.keysym.sym == SDLK_BACKSPACE && selected >= 0)
                    {
                        remove_filter(selected);
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
        sprintf(wbuf, "Waveform: %s (TAB to change)", wave_names[wave]);
        draw_text(ren, font, 120, 28, wbuf, white);

        int y = 90;
        for (int i = 0; i < filter_count; i++)
        {
            if (i == selected)
            {
                SDL_SetRenderDrawColor(ren, 80,80,80,255);
                SDL_Rect hl = {20, y - 4, 640, 30};
                SDL_RenderFillRect(ren, &hl);
            }

            char buf[128];
            sprintf(buf, "Filter %d: %s  Cutoff=%.0fHz  Res=%.2f",
                i+1, filter_names[filters[i].type],
                filters[i].cutoff, filters[i].res);

            draw_text(ren, font, 30, y, buf, white);
            y += 35;
        }

        draw_text(ren, font, 50, 650,
            " + add filter | Q type | arrows cutoff/res | 1/2 octave | click select | ESC exit", white);
        
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
        voices[i].active = 0;
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

    if (wave == WAVE_SAW)
        return 2.0f * (v->phase / (2 * PI)) - 1;
    if (wave == WAVE_SQUARE)
        return sin(v->phase) > 0 ? 1 : -1;
    if (wave == WAVE_TRIANGLE)
        return asin(sin(v->phase)) * 2 / PI;

    return 0;
}

/* ===== filter ===== */
void update_filter(Filter *f)
{
    if (f->cutoff < 20) f->cutoff = 20;
    if (f->cutoff > SAMPLE_RATE / 2 - 100) 
        f->cutoff = SAMPLE_RATE / 2 - 100;

    float w0 = 2.0f * PI * f->cutoff / SAMPLE_RATE;
    float cosw = cosf(w0);
    float sinw = sinf(w0);

    if (f->res < 0.05f) f->res = 0.05f;   

    float alpha = sinw / (2.0f * f->res);

    float b0, b1, b2, a0, a1, a2;

    switch (f->type)
    {
        case LPF:
            b0 = (1 - cosw) / 2;
            b1 = 1 - cosw;
            b2 = (1 - cosw) / 2;
            a0 = 1 + alpha;
            a1 = -2 * cosw;
            a2 = 1 - alpha;
            break;

        case HPF:
            b0 = (1 + cosw) / 2;
            b1 = -(1 + cosw);
            b2 = (1 + cosw) / 2;
            a0 = 1 + alpha;
            a1 = -2 * cosw;
            a2 = 1 - alpha;
            break;

        case BPF:
            b0 = alpha;
            b1 = 0;
            b2 = -alpha;
            a0 = 1 + alpha;
            a1 = -2 * cosw;
            a2 = 1 - alpha;
            break;

        case NOTCH:
            b0 = 1;
            b1 = -2 * cosw;
            b2 = 1;
            a0 = 1 + alpha;
            a1 = -2 * cosw;
            a2 = 1 - alpha;
            break;
    }

    f->b0 = b0 / a0;
    f->b1 = b1 / a0;
    f->b2 = b2 / a0;
    f->a1 = a1 / a0;
    f->a2 = a2 / a0;
}

float process_filter(Filter *f, float x)
{
    float y = f->b0 * x
            + f->b1 * f->x1
            + f->b2 * f->x2
            - f->a1 * f->y1
            - f->a2 * f->y2;

    f->x2 = f->x1;
    f->x1 = x;
    f->y2 = f->y1;
    f->y1 = y;

    return y;
}

void add_filter()
{
    if (filter_count >= MAX_FILTERS) return;

    filters[filter_count].type = LPF;
    filters[filter_count].cutoff = 800;
    filters[filter_count].res = 0.7f; 
    filters[filter_count].x1 = filters[filter_count].x2 = 0;
    filters[filter_count].y1 = filters[filter_count].y2 = 0;

    update_filter(&filters[filter_count]);

    selected = filter_count;
    filter_count++;
}

void remove_filter(int index)
{
    if (index < 0 || index >= filter_count) return;

    for (int i = index; i < filter_count - 1; i++)
        filters[i] = filters[i + 1];

    filter_count--;

    if (filter_count == 0)
        selected = -1;
    else if (selected >= filter_count)
        selected = filter_count - 1;
}

/* ===== audio ===== */
void audio_callback(void *u, Uint8 *stream, int len)
{
    float *buf = (float *)stream;
    int samples = len / sizeof(float);
    float target_amp;
    float amp;

    for (int i = 0; i < samples; i++)
    {
        float s = 0.0f;

        for (int v = 0; v < MAX_VOICES; v++)
        {
            if (voices[v].active)
                s += gen_voice(&voices[v]);
        }

        s /= 2.5;

        for (int f = 0; f < filter_count; f++)
            s = process_filter(&filters[f], s);

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

