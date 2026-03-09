#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_RATE 44100
#define PI 3.14159265359
#define MAX_SAMPLES 44100

typedef struct 
{
    float freq1;
    float freq2;
    float mix2;        

    int use_fm;
    float fm_amount;   
    float fm_k;        

    int use_noise;
    float noise_amt;

    int use_env;
    float env_k;

    int length;
} DrumParams;


DrumParams params = 
{
    .freq1 = 180.0f,
    .freq2 = 350.0f,
    .mix2 = 0.0f,

    .use_fm = 0,
    .fm_amount = 80.0f,
    .fm_k = 12.0f,

    .use_noise = 0,
    .noise_amt = 0.4f,

    .use_env = 0,
    .env_k = 7.0f,

    .length = 12000
};

float sample_buf[MAX_SAMPLES];
int play_pos = 0;
int playing = 0;

void generate_sample(float *buffer, DrumParams *p);
void audio_callback(void *u, Uint8 *stream, int len);
void draw_text(SDL_Renderer *r, TTF_Font *f, int x, int y, const char *t);
void draw_waveform(SDL_Renderer *ren,float *buffer, int length, int x, int y, int w, int h);
void play_kick(DrumParams *p);
void play_snare(DrumParams *p);
void play_hihat(DrumParams *p);
void play_tom(DrumParams *p);

int main()
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window *win = SDL_CreateWindow(
        "Drum Sample Synth",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        700, 600, 0
    );

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font *font = TTF_OpenFont(
        "/System/Library/Fonts/Supplemental/Arial.ttf", 18
    );

    SDL_AudioSpec spec = {0};
    spec.freq = SAMPLE_RATE;
    spec.format = AUDIO_F32SYS;
    spec.channels = 1;
    spec.samples = 512;
    spec.callback = audio_callback;
    SDL_OpenAudio(&spec, NULL);
    SDL_PauseAudio(0);

    generate_sample(sample_buf, &params);

    SDL_Event e;
    int run = 1;

    while (run)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) run = 0;

            if (e.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                    case SDLK_ESCAPE: run = 0; break;

                    /* play */
                    case SDLK_SPACE:
                        generate_sample(sample_buf, &params);
                        playing = 1;
                        play_pos = 0;
                        break;

                    /* osc frequencies */
                    case SDLK_1: params.freq1 -= 10; break;
                    case SDLK_2: params.freq1 += 10; break;
                    case SDLK_3: params.freq2 -= 10; break;
                    case SDLK_4: params.freq2 += 10; break;

                    /* osc mix */
                    case SDLK_q: params.mix2 -= 0.05f; break;
                    case SDLK_w: params.mix2 += 0.05f; break;

                    /* pitch envelope (FM) */
                    case SDLK_f: params.use_fm ^= 1; break;
                    case SDLK_e: params.fm_amount -= 10; break;
                    case SDLK_r: params.fm_amount += 10; break;
                    case SDLK_t: params.fm_k -= 1; break;
                    case SDLK_y: params.fm_k += 1; break;

                    /* noise */
                    case SDLK_n: params.use_noise ^= 1; break;
                    case SDLK_b: params.noise_amt -= 0.05f; break;
                    case SDLK_m: params.noise_amt += 0.05f; break;

                    /* amplitude envelope */
                    case SDLK_z: params.use_env ^= 1; break;
                    case SDLK_x: params.env_k -= 1; break;
                    case SDLK_c: params.env_k += 1; break;

                    /* length */
                    case SDLK_g: params.length -= 500; break;
                    case SDLK_h: params.length += 500; break;

                    /* presets */
                    case SDLK_5: play_kick(&params); break;
                    case SDLK_6: play_snare(&params); break;
                    case SDLK_7: play_tom(&params); break;
                    case SDLK_8: play_hihat(&params); break;
                }

                /* защита от бредовых значений */
                if (params.mix2 < 0) params.mix2 = 0;
                if (params.mix2 > 1) params.mix2 = 1;
                if (params.noise_amt < 0) params.noise_amt = 0;
                if (params.env_k < 0.1f) params.env_k = 0.1f;
                if (params.fm_k < 0.1f) params.fm_k = 0.1f;
                if (params.length < 100) params.length = 100;
                if (params.length > MAX_SAMPLES) params.length = MAX_SAMPLES;
            }
        }

        SDL_SetRenderDrawColor(ren, 20,20,20,255);
        SDL_RenderClear(ren);

        char buf[128];
        draw_text(ren, font, 30, 30, "SPACE: play / regenerate | PRESETS: 5=Kick 6=Snare 7=Tom 8=HiHat");

        /* OSCILLATORS */
        sprintf(buf, "OSC1 Freq: %.0f Hz  (1 / 2)", params.freq1);
        draw_text(ren, font, 30, 80, buf);

        sprintf(buf, "OSC2 Freq: %.0f Hz  (3 / 4)   Mix: %.2f  (Q / W)",
                params.freq2, params.mix2);
        draw_text(ren, font, 30, 110, buf);

        /* PITCH ENVELOPE (FM) */
        sprintf(buf, "Pitch Env (F): %s  Amount: %.0f Hz (E / R)  Speed: %.1f (T / Y)",
                params.use_fm ? "ON" : "OFF",
                params.fm_amount,
                params.fm_k);
        draw_text(ren, font, 30, 150, buf);

        /* NOISE */
        sprintf(buf, "Noise: %s  Amount: %.2f  (N / B / M)",
                params.use_noise ? "ON" : "OFF",
                params.noise_amt);
        draw_text(ren, font, 30, 190, buf);

        /* AMPLITUDE ENVELOPE */
        sprintf(buf, "Amp Env: %s  Decay: %.1f  (Z / X / C)",
                params.use_env ? "ON" : "OFF",
                params.env_k);
        draw_text(ren, font, 30, 230, buf);

        /* LENGTH */
        sprintf(buf, "Length: %d samples  (G / H)", params.length);
        draw_text(ren, font, 30, 270, buf);

        draw_waveform(ren, sample_buf, params.length,
              160, 350, 360, 200);

    
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_Quit();
}

/* ===== sample generation ===== */
void generate_sample(float *buffer, DrumParams *p)
{
    float phase1 = 0.0f;
    float phase2 = 0.0f;

    float phase_inc1 = 2.0f * PI * p->freq1 / SAMPLE_RATE;
    float phase_inc2 = 2.0f * PI * p->freq2 / SAMPLE_RATE;

    for (int i = 0; i < p->length; i++)
    {
        float t = (float)i / SAMPLE_RATE;

        /* ===== amplitude envelope ===== */
        float env = 1.0f;
        if (p->use_env)
            env = expf(-p->env_k * t);

        /* ===== pitch envelope ===== */
        float freq_mod = 0.0f;
        if (p->use_fm)
            freq_mod = p->fm_amount * expf(-p->fm_k * t);

        /* ===== main oscillator ===== */
        float osc1 = sinf(phase1);
        float osc = osc1;

        /* ===== wave addition ===== */
        if (p->mix2 > 0.0f)
        {
            float osc2 = sinf(phase2);
            osc = (1.0f - p->mix2) * osc1 + p->mix2 * osc2;
        }

        /* ===== noise ===== */
        float noise = 0.0f;
        if (p->use_noise)
        {
            noise = p->noise_amt *
                    ((float)(rand() % 200 - 100) / 100.0f);
        }

        buffer[i] = env * (osc + noise);

        /* ===== phase advancement ===== */
        float inc1 = 2.0f * PI * (p->freq1 + freq_mod) / SAMPLE_RATE;
        float inc2 = 2.0f * PI * p->freq2 / SAMPLE_RATE;

        phase1 += inc1;
        phase2 += inc2;

        if (phase1 > 2 * PI) phase1 -= 2 * PI;
        if (phase2 > 2 * PI) phase2 -= 2 * PI;
    }
}

void play_kick(DrumParams *p)
{
    p->freq1 = 80.0f;
    p->freq2 = 0.0f;
    p->mix2 = 0.0f;

    p->use_fm = 1;
    p->fm_amount = 40.0f;
    p->fm_k = 15.0f;

    p->use_noise = 0;
    p->noise_amt = 0.0f;

    p->use_env = 1;
    p->env_k = 10.0f;

    p->length = 12000;
}

void play_snare(DrumParams *p)
{
    p->freq1 = 180.0f;
    p->freq2 = 0.0f;
    p->mix2 = 0.0f;

    p->use_fm = 0;
    p->fm_amount = 0.0f;
    p->fm_k = 0.0f;

    p->use_noise = 1;
    p->noise_amt = 0.4f;

    p->use_env = 1;
    p->env_k = 7.0f;

    p->length = 12000;
}

void play_hihat(DrumParams *p)
{
    p->freq1 = 8000.0f;
    p->freq2 = 11000.0f;
    p->mix2 = 0.5f;

    p->use_fm = 0;

    p->use_noise = 1;
    p->noise_amt = 0.7f;

    p->use_env = 1;
    p->env_k = 8.0f;

    p->length = 15000;
}

void play_tom(DrumParams *p)
{
    p->freq1 = 200.0f;
    p->freq2 = 300.0f;
    p->mix2 = 0.3f;

    p->use_fm = 1;
    p->fm_amount = 50.0f;
    p->fm_k = 10.0f;

    p->use_noise = 0;
    p->noise_amt = 0.0f;

    p->use_env = 1;
    p->env_k = 8.0f;

    p->length = 10000;
}

/* ===== audio ===== */
void audio_callback(void *u, Uint8 *stream, int len)
{
    float *buf = (float *)stream;
    int samples = len / sizeof(float);

    for (int i = 0; i < samples; i++)
    {
        if (playing && play_pos < params.length)
            buf[i] = sample_buf[play_pos++] * 0.8f;
        else
        {
            buf[i] = 0;
            playing = 0;
            play_pos = 0;
        }
    }
}

/* ===== UI ===== */
void draw_text(SDL_Renderer *r, TTF_Font *f, int x, int y, const char *t)
{
    SDL_Color c = {255,255,255};
    SDL_Surface *s = TTF_RenderText_Solid(f, t, c);
    SDL_Texture *tx = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect d = {x,y,s->w,s->h};
    SDL_RenderCopy(r, tx, NULL, &d);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(tx);
}

void draw_waveform(SDL_Renderer *ren,
                   float *buffer,
                   int length,
                   int x, int y,
                   int w, int h)
{
    SDL_SetRenderDrawColor(ren, 0, 220, 160, 255);

    int mid_y = y + h / 2;

    for (int i = 1; i < w; i++)
    {
        int idx1 = (int)((float)(i - 1) / w * length);
        int idx2 = (int)((float)i / w * length);

        if (idx2 >= length) idx2 = length - 1;

        float s1 = buffer[idx1];
        float s2 = buffer[idx2];

        int y1 = mid_y - (int)(s1 * (h / 2));
        int y2 = mid_y - (int)(s2 * (h / 2));

        SDL_RenderDrawLine(ren,
            x + i - 1, y1,
            x + i,     y2);
    }

    SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
    SDL_RenderDrawLine(ren, x, mid_y, x + w, mid_y);
}