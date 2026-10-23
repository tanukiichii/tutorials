#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE 44100
#define PI 3.14159265359
#define MAX_FILTERS 4
#define MAX_VOICES 8
#define WAVE_BUF 1024
#define NUM_SUBS 4              /* число делителей (субгармоник) в голосе */
#define DIV_MIN 1
#define DIV_MAX 32
#define NUM_VOWELS 5            /* A E I O U */
#define NUM_REGISTERS 3         /* bass / tenor / soprano */
#define FORMANTS_PER_PRESET 4   /* формант в одной заготовке, <= MAX_FILTERS */
#define FORMANT_MAKEUP_DB 12.0f   /* общая добавка громкости: узкие форманты заметно тише сырого сигнала */

#if FORMANTS_PER_PRESET > MAX_FILTERS
#error FORMANTS_PER_PRESET must not exceed MAX_FILTERS
#endif

typedef enum {LPF, BPF} FilterType;

const char *filter_names[] = {"LPF", "BPF"};

typedef struct {
    FilterType type;
    float cutoff;
    float res;
    float gain;            
    float z1, z2;
} Filter;

typedef struct {
    int active;
    SDL_Keycode key;
    float freq;
    float phase;                    /* основной осциллятор */
    float sub_phase[NUM_SUBS];      /* субгармоники */
    int   div[NUM_SUBS];            /* делители */
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
Filter filters[MAX_FILTERS];
int filter_count = 0;
int selected = -1;
float phase = 0;
float current_freq = 0.0f;
int playing = 0;
int line_playing = 0;
int line_x = 355;
float line_freq = 440.0f;
float wave_vis[WAVE_BUF];
int wave_pos = 0;

int div_cfg[NUM_SUBS] = {2, 4, 6, 8};
const float sub_gain[NUM_SUBS] = {0.25f, 0.25f, 0.125f, 0.125f};

int filter_parallel = 0;

/* ===== formant presets ===== */
typedef struct { float freq, bw, amp_db; } Formant;

const char *vowel_names[NUM_VOWELS] = {"A", "E", "I", "O", "U"};  
const char *register_names[NUM_REGISTERS] = {"Bass", "Tenor", "Soprano"};

const Formant vowel_presets[NUM_REGISTERS][NUM_VOWELS][FORMANTS_PER_PRESET] = {
    /* ---- Bass ---- */
    {
        {{ 600, 60,   0}, {1040, 70,  -7}, {2250, 110,  -9}, {2450, 120,  -9}},  /* A */
        {{ 400, 40,   0}, {1620, 80, -12}, {2400, 100,  -9}, {2800, 120, -12}},  /* E */
        {{ 250, 60,   0}, {1750, 90, -30}, {2600, 100, -16}, {3050, 120, -22}},  /* I */
        {{ 400, 40,   0}, { 750, 80, -11}, {2400, 100, -21}, {2600, 120, -20}},  /* O */
        {{ 350, 40,   0}, { 600, 80, -20}, {2400, 100, -32}, {2675, 120, -28}},  /* U */
    },
    /* ---- Tenor ---- */
    {
        {{ 650, 80,   0}, {1080, 90,  -6}, {2650, 120,  -7}, {2900, 130,  -8}},  /* A */
        {{ 400, 70,   0}, {1700, 80, -14}, {2600, 100, -12}, {3200, 120, -14}},  /* E */
        {{ 290, 40,   0}, {1870, 90, -15}, {2800, 100, -18}, {3250, 120, -20}},  /* I */
        {{ 400, 40,   0}, { 800, 80, -10}, {2600, 100, -12}, {2800, 120, -12}},  /* O */
        {{ 350, 40,   0}, { 600, 80, -20}, {2700, 100, -17}, {2900, 120, -14}},  /* U */
    },
    /* ---- Soprano ---- */
    {
        {{ 800, 80,   0}, {1150, 90,  -6}, {2900, 120, -32}, {3900, 130, -20}},  /* A */
        {{ 350, 60,   0}, {2000, 100, -20}, {2800, 120, -15}, {3600, 150, -40}}, /* E */
        {{ 270, 60,   0}, {2140, 90, -12}, {2950, 100, -26}, {3900, 120, -26}},  /* I */
        {{ 450, 70,   0}, { 800, 80, -11}, {2830, 100, -22}, {3800, 130, -50}},  /* O */
        {{ 325, 50,   0}, { 700, 60, -16}, {2700, 170, -35}, {3800, 180, -40}},  /* U */
    },
};

int cur_vowel = -1;      
int cur_register = 1;    

SDL_Color white = {240,240,240,255};
SDL_Color black = {30,30,30,255};
SDL_Rect button_add = {20, 20, 60, 40};

SDL_Rect vowel_btn[NUM_VOWELS] = {
    {100, 20, 44, 40}, {150, 20, 44, 40}, {200, 20, 44, 40},
    {250, 20, 44, 40}, {300, 20, 44, 40}
};
SDL_Rect button_register = {360, 20, 100, 40};
SDL_Rect button_mode     = {470, 20, 110, 40};

#define DIV_TITLE_Y 232
#define DIV_Y       258
#define DIV_H       32
#define DIV_CELL_W  160
#define DIV_PITCH   165
#define DIV_BTN_W   32

#define PITCH_LINE_X0 80
#define PITCH_LINE_X1 630
#define PITCH_LINE_Y 560
#define PITCH_LINE_HIT 18
#define PITCH_MIN_HZ 110.0f
#define PITCH_MAX_HZ 880.0f


static float max_res(float cutoff)
{
    float c = 2 * sinf(PI * cutoff / SAMPLE_RATE);
    if (c > 0.9f) return 0.0f;                      
    float lim = (2 - c) / (1 - c) - 0.05f;
    return lim < 4.0f ? lim : 4.0f;
}

/* ===== UI helpers ===== */
static int in_rect(const SDL_Rect *r, int x, int y)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static SDL_Rect div_cell_rect(int i)
{
    SDL_Rect r = {20 + i * DIV_PITCH, DIV_Y, DIV_CELL_W, DIV_H};
    return r;
}

static SDL_Rect div_minus_rect(int i)
{
    SDL_Rect r = {20 + i * DIV_PITCH, DIV_Y, DIV_BTN_W, DIV_H};
    return r;
}

static SDL_Rect div_plus_rect(int i)
{
    SDL_Rect r = {20 + i * DIV_PITCH + DIV_CELL_W - DIV_BTN_W, DIV_Y, DIV_BTN_W, DIV_H};
    return r;
}

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
float process_filter(Filter *f, float x);
void add_filter();
void remove_filter(int index);
void make_formant_filter(Filter *f, float freq, float bw, float amp_db);
void apply_vowel(int reg, int vowel);
void cycle_register(void);
void toggle_parallel(void);

/* ===== dividers ===== */
void set_divider(int idx, int value);

/* ===== audio ===== */
void audio_callback(void *u, Uint8 *stream, int len);

/* ===== UI ===== */
void draw_text(SDL_Renderer *r, TTF_Font *f, int x, int y, const char *txt, SDL_Color c);
void draw_wave(SDL_Renderer *r);
void draw_pitch_line(SDL_Renderer *r, TTF_Font *font);
void update_line_pitch(int x);
void draw_button(SDL_Renderer *r, TTF_Font *font, SDL_Rect rc, const char *txt, int active);
void draw_presets(SDL_Renderer *r, TTF_Font *font);
void draw_dividers(SDL_Renderer *r, TTF_Font *font);


int main()
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window *win = SDL_CreateWindow("Trautonium",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 700, 700, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font *font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 18);

    init_voices();

    SDL_AudioSpec spec = {0};
    spec.freq = SAMPLE_RATE;
    spec.format = AUDIO_F32SYS;
    spec.channels = 1;
    spec.samples = 512;
    spec.callback = audio_callback;

    if (SDL_OpenAudio(&spec, NULL) < 0)
    {
        fprintf(stderr, "Не удалось открыть аудиоустройство: %s\n", SDL_GetError());
        if (font) TTF_CloseFont(font);
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_PauseAudio(0);

    SDL_Event e;

    int run = 1;

    while (run)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) run = 0;

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
            {
                int x = e.button.x;
                int y = e.button.y;

                if (y >= PITCH_LINE_Y - PITCH_LINE_HIT &&
                    y <= PITCH_LINE_Y + PITCH_LINE_HIT &&
                    x >= PITCH_LINE_X0 && x <= PITCH_LINE_X1)
                {
                    line_playing = 1;
                    update_line_pitch(x);
                }

                if (x > button_add.x && x < button_add.x + button_add.w &&
                    y > button_add.y && y < button_add.y + button_add.h)
                {
                    add_filter();
                }

                for (int i = 0; i < NUM_VOWELS; i++)
                    if (in_rect(&vowel_btn[i], x, y))
                        apply_vowel(cur_register, i);

                if (in_rect(&button_register, x, y)) cycle_register();
                if (in_rect(&button_mode, x, y))     toggle_parallel();

                for (int i = 0; i < NUM_SUBS; i++)
                {
                    SDL_Rect m = div_minus_rect(i);
                    SDL_Rect p = div_plus_rect(i);
                    if (in_rect(&m, x, y)) set_divider(i, div_cfg[i] - 1);
                    if (in_rect(&p, x, y)) set_divider(i, div_cfg[i] + 1);
                }

                int start_y = 90;
                for (int i = 0; i < filter_count; i++)
                {
                    if (y > start_y + i * 35 && y < start_y + i * 35 + 30)
                        selected = i;
                }
            }

            if (e.type == SDL_MOUSEWHEEL && e.wheel.y != 0)
            {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                int dy = e.wheel.y;
                if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) dy = -dy;

                for (int i = 0; i < NUM_SUBS; i++)
                {
                    SDL_Rect c = div_cell_rect(i);
                    if (in_rect(&c, mx, my))
                        set_divider(i, div_cfg[i] + (dy > 0 ? 1 : -1));
                }
            }

            if (e.type == SDL_MOUSEMOTION && line_playing &&
                (e.motion.state & SDL_BUTTON_LMASK))
            {
                update_line_pitch(e.motion.x);
            }

            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
            {
                if (line_playing)
                {
                    line_playing = 0;
                    SDL_LockAudio();
                    voices[0].active = 0;
                    SDL_UnlockAudio();
                }
            }

            if (e.type == SDL_KEYDOWN && !e.key.repeat)
            {
                if (e.key.keysym.sym == SDLK_ESCAPE) run = 0;

                if (e.key.keysym.sym == SDLK_EQUALS) add_filter();

                if (e.key.keysym.sym >= SDLK_3 && e.key.keysym.sym <= SDLK_7)
                    apply_vowel(cur_register, e.key.keysym.sym - SDLK_3);
                if (e.key.keysym.sym == SDLK_8) cycle_register();
                if (e.key.keysym.sym == SDLK_9) toggle_parallel();

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

                    SDL_Keycode ek = e.key.keysym.sym;
                    if (ek == SDLK_q || ek == SDLK_UP || ek == SDLK_DOWN ||
                        ek == SDLK_LEFT || ek == SDLK_RIGHT ||
                        ek == SDLK_LEFTBRACKET || ek == SDLK_RIGHTBRACKET)
                        cur_vowel = -1;

                    if (e.key.keysym.sym == SDLK_q)
                        f->type = (f->type + 1) % 2;

                    if (e.key.keysym.sym == SDLK_UP) f->cutoff += 100;
                    if (e.key.keysym.sym == SDLK_DOWN && f->cutoff > 50) f->cutoff -= 100;

                    if (e.key.keysym.sym == SDLK_RIGHT) f->res += 0.05f;
                    if (e.key.keysym.sym == SDLK_LEFT && f->res > 0) f->res -= 0.05f;

                    if (e.key.keysym.sym == SDLK_LEFTBRACKET && f->gain > 0.01f) f->gain /= 1.122f;  /* -1 dB */
                    if (e.key.keysym.sym == SDLK_RIGHTBRACKET && f->gain < 10.0f) f->gain *= 1.122f; /* +1 dB */

                    /* Res не выше границы устойчивости для текущего cutoff */
                    float lim = max_res(f->cutoff);
                    if (f->res > lim) f->res = lim;
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
            snprintf(buf, sizeof(buf), "Filter %d: %s  Cutoff=%.0fHz  Res=%.2f  Gain=%+.1fdB",
                i+1, filter_names[filters[i].type],
                filters[i].cutoff, filters[i].res,
                20.0f * log10f(filters[i].gain > 0.0001f ? filters[i].gain : 0.0001f));

            draw_text(ren, font, 30, y, buf, white);
            y += 35;
        }

        draw_text(ren, font, 20, 620,
            "+ add | Q type | arrows cutoff/res | [ ] gain | 1/2 octave | ESC exit", white);
        draw_text(ren, font, 20, 645,
            "3-7 vowel A E I O U | 8 register | 9 serial/parallel | wheel on divider", white);

        draw_presets(ren, font);
        draw_dividers(ren, font);
        draw_wave(ren);
        draw_pitch_line(ren, font);

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
        voices[i].freq = 0.0f;
        voices[i].phase = 0.0f;
        for (int k = 0; k < NUM_SUBS; k++)
        {
            voices[i].sub_phase[k] = 0.0f;
            voices[i].div[k] = div_cfg[k];
        }
    }
}

int find_voice(SDL_Keycode key)
{
    for (int i = 1; i < MAX_VOICES; i++)
        if (voices[i].active && voices[i].key == key)
            return i;
    return -1;
}

int alloc_voice()
{
    for (int i = 1; i < MAX_VOICES; i++)
        if (!voices[i].active)
            return i;
    return MAX_VOICES - 1;
}

int is_key_active(SDL_Keycode key)
{
    for (int i = 1; i < MAX_VOICES; i++)
        if (voices[i].active && voices[i].key == key)
            return 1;
    return 0;
}


/* ===== oscillator ===== */
float gen_voice(Voice *v)
{
    float inc = 2 * PI * v->freq / SAMPLE_RATE;

    v->phase += inc;
    if (v->phase > 2 * PI) v->phase -= 2 * PI;

    float out = (2.0f * (v->phase / (2 * PI)) - 1.0f) * 0.25f;

    for (int i = 0; i < NUM_SUBS; i++)
    {
        v->sub_phase[i] += inc / v->div[i];
        if (v->sub_phase[i] > 2 * PI) v->sub_phase[i] -= 2 * PI;

        out += (2.0f * (v->sub_phase[i] / (2 * PI)) - 1.0f) * sub_gain[i];
    }

    return out;
}

/* ===== dividers ===== */

void set_divider(int idx, int value)
{
    if (idx < 0 || idx >= NUM_SUBS) return;
    if (value < DIV_MIN) value = DIV_MIN;
    if (value > DIV_MAX) value = DIV_MAX;

    SDL_LockAudio();
    div_cfg[idx] = value;
    for (int i = 0; i < MAX_VOICES; i++)
        voices[i].div[idx] = value;
    SDL_UnlockAudio();
}

/* ===== filter ===== */
float process_filter(Filter *f, float x)
{
    float c = 2 * sinf(PI * f->cutoff / SAMPLE_RATE);
    float r = f->res;

    f->z1 += c * (x - f->z1 + r * (f->z1 - f->z2));
    f->z2 += c * (f->z1 - f->z2);

    switch(f->type)
    {
        case LPF: return f->z2;
        case BPF: return f->z1 - f->z2;
    }

    return x;
}

void add_filter()
{
    if (filter_count >= MAX_FILTERS) return;

    filters[filter_count].type = LPF;
    filters[filter_count].cutoff = 800;
    filters[filter_count].res = 0.1f;
    filters[filter_count].gain = 1.0f;
    filters[filter_count].z1 = filters[filter_count].z2 = 0;

    selected = filter_count;
    filter_count++;
    cur_vowel = -1;
}

void remove_filter(int index)
{
    if (index < 0 || index >= filter_count) return;

    for (int i = index; i < filter_count - 1; i++)
        filters[i] = filters[i + 1];

    filter_count--;
    cur_vowel = -1;

    if (filter_count == 0)
        selected = -1;
    else if (selected >= filter_count)
        selected = filter_count - 1;
}

static float bpf_peak_gain(float cutoff, float res)
{
    float c = 2 * sinf(PI * cutoff / SAMPLE_RATE);
    float T = 2 - 2 * c + c * res * (1 - c);
    float D = (1 - c) * (1 - c + c * res);
    float w = 2 * PI * cutoff / SAMPLE_RATE;

    float nr = c * (1 - c) * (cosf(w) - 1);
    float ni = c * (1 - c) * sinf(w);
    float dr = cosf(2 * w) - T * cosf(w) + D;
    float di = sinf(2 * w) - T * sinf(w);

    return hypotf(nr, ni) / hypotf(dr, di);
}

void make_formant_filter(Filter *f, float freq, float bw, float amp_db)
{
    float c = 2 * sinf(PI * freq / SAMPLE_RATE);
    float D = expf(-2 * PI * bw / SAMPLE_RATE);      

    f->type = BPF;
    f->cutoff = freq;
    f->res = (D - (1 - c) * (1 - c)) / (c * (1 - c));
    f->z1 = f->z2 = 0;
    f->gain = powf(10.0f, (amp_db + FORMANT_MAKEUP_DB) / 20.0f) / bpf_peak_gain(f->cutoff, f->res);
}

void apply_vowel(int reg, int vowel)
{
    if (reg < 0 || reg >= NUM_REGISTERS || vowel < 0 || vowel >= NUM_VOWELS) return;

    SDL_LockAudio();
    for (int i = 0; i < FORMANTS_PER_PRESET; i++)
    {
        const Formant *fm = &vowel_presets[reg][vowel][i];
        make_formant_filter(&filters[i], fm->freq, fm->bw, fm->amp_db);
    }
    filter_count = FORMANTS_PER_PRESET;
    filter_parallel = 1;
    selected = 0;
    cur_register = reg;
    cur_vowel = vowel;
    SDL_UnlockAudio();
}

void cycle_register(void)
{
    cur_register = (cur_register + 1) % NUM_REGISTERS;
    if (cur_vowel >= 0)
        apply_vowel(cur_register, cur_vowel);
}

void toggle_parallel(void)
{
    SDL_LockAudio();
    filter_parallel = !filter_parallel;
    SDL_UnlockAudio();
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
            if (voices[v].active)
                s += gen_voice(&voices[v]);
        }

        s /= 2.5;

        if (filter_parallel && filter_count > 0)
        {
            float in = s, sum = 0.0f;
            for (int f = 0; f < filter_count; f++)
                sum += process_filter(&filters[f], in) * filters[f].gain;
            s = sum;
        }
        else
        {
            for (int f = 0; f < filter_count; f++)
                s = process_filter(&filters[f], s) * filters[f].gain;
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



/* ===== управление высотой как у траутониума ===== */
void update_line_pitch(int x)
{
    if (x < PITCH_LINE_X0) x = PITCH_LINE_X0;
    if (x > PITCH_LINE_X1) x = PITCH_LINE_X1;

    line_x = x;
    float position = (float)(x - PITCH_LINE_X0) /
                     (float)(PITCH_LINE_X1 - PITCH_LINE_X0);

    line_freq = PITCH_MIN_HZ * powf(PITCH_MAX_HZ / PITCH_MIN_HZ, position);

    SDL_LockAudio();
    voices[0].freq = line_freq;
    voices[0].key = SDLK_UNKNOWN;
    voices[0].active = 1;
    SDL_UnlockAudio();
}

void draw_pitch_line(SDL_Renderer *r, TTF_Font *font)
{
    SDL_SetRenderDrawColor(r, 65, 65, 65, 255);
    SDL_Rect hit_area = {PITCH_LINE_X0, PITCH_LINE_Y - PITCH_LINE_HIT,
                         PITCH_LINE_X1 - PITCH_LINE_X0, PITCH_LINE_HIT * 2};
    SDL_RenderFillRect(r, &hit_area);

    SDL_SetRenderDrawColor(r, 245, 190, 205, 255);
    SDL_RenderDrawLine(r, PITCH_LINE_X0, PITCH_LINE_Y,
                       PITCH_LINE_X1, PITCH_LINE_Y);

    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_Rect marker = {line_x - 4, PITCH_LINE_Y - 9, 8, 18};
    SDL_RenderFillRect(r, &marker);

    char label[128];
    if (line_playing)
        snprintf(label, sizeof(label), "Touch line: %.1f Hz  (%.2f kHz)",
                 line_freq, line_freq / 1000.0f);
    else
        snprintf(label, sizeof(label), "Click and drag along the line | %.1f Hz",
                 line_freq);
    draw_text(r, font, PITCH_LINE_X0, PITCH_LINE_Y + 24, label, white);
}

void draw_button(SDL_Renderer *r, TTF_Font *font, SDL_Rect rc, const char *txt, int active)
{
    if (active) SDL_SetRenderDrawColor(r, 194, 155, 162, 255);
    else        SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
    SDL_RenderFillRect(r, &rc);

    SDL_SetRenderDrawColor(r, 110, 110, 110, 255);
    SDL_RenderDrawRect(r, &rc);

    int tw = 0, th = 0;
    TTF_SizeText(font, txt, &tw, &th);
    draw_text(r, font, rc.x + (rc.w - tw) / 2, rc.y + (rc.h - th) / 2, txt,
              active ? black : white);
}

void draw_presets(SDL_Renderer *r, TTF_Font *font)
{
    for (int i = 0; i < NUM_VOWELS; i++)
        draw_button(r, font, vowel_btn[i], vowel_names[i], i == cur_vowel);

    draw_button(r, font, button_register, register_names[cur_register], 0);
    draw_button(r, font, button_mode, filter_parallel ? "PARALLEL" : "SERIAL", filter_parallel);
}

void draw_dividers(SDL_Renderer *r, TTF_Font *font)
{
    draw_text(r, font, 20, DIV_TITLE_Y, "Subharmonic dividers (-/+ or mouse wheel):", white);

    for (int i = 0; i < NUM_SUBS; i++)
    {
        SDL_Rect cell = div_cell_rect(i);
        SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
        SDL_RenderFillRect(r, &cell);

        draw_button(r, font, div_minus_rect(i), "-", 0);
        draw_button(r, font, div_plus_rect(i), "+", 0);

        char buf[16];
        snprintf(buf, sizeof(buf), "f/%d", div_cfg[i]);
        int tw = 0, th = 0;
        TTF_SizeText(font, buf, &tw, &th);
        draw_text(r, font, cell.x + (cell.w - tw) / 2, cell.y + (cell.h - th) / 2, buf, white);
    }
}
