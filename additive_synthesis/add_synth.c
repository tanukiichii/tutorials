#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE 22100
#define PI 3.14159265359
#define MAX_OSC 8
#define WAVE_BUF 1024

float wave_vis[WAVE_BUF];
int wave_pos = 0;

typedef enum { WAVE_SINE, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SAW } WaveType;

const char *wave_names[] = {"Sine", "Square", "Triangle", "Saw"};

typedef struct 
{
    WaveType type;
    double freq;
    double amp;
    double phase;
} Oscillator;

Oscillator osc[MAX_OSC];
int osc_count = 0;
int selected = -1;   
int playing = 0;

double gen_wave(Oscillator *o) 
{
    double x = o->phase;
    switch (o->type) 
    {
        case WAVE_SINE: return sin(x);
        case WAVE_SQUARE: return sin(x) >= 0 ? 1 : -1;
        case WAVE_TRIANGLE: return asin(sin(x)) * 2 / PI;
        case WAVE_SAW: return (2.0 * (x / (2 * PI))) - 1.0;
    }
    return 0;
}

void audio_callback(void *u, Uint8 *stream, int len) 
{
    float *buf = (float *)stream;
    int samples = len / sizeof(float);

    for (int i = 0; i < samples; i++) 
    {
        double mix = 0;

        if (playing) 
        {
            for (int k = 0; k < osc_count; k++) 
            {
                mix += gen_wave(&osc[k]) * osc[k].amp;

                osc[k].phase += 2 * PI * osc[k].freq / SAMPLE_RATE;
                if (osc[k].phase > 2 * PI)
                    osc[k].phase -= 2 * PI;
            }
        }

        if (osc_count > 0)
            mix /= osc_count;

        buf[i] = (float)mix;

        wave_vis[wave_pos] = (float)mix;
        wave_pos = (wave_pos + 1) % WAVE_BUF;
    }
}

SDL_Rect button_add = {20, 20, 60, 40};

void draw_text(SDL_Renderer *r, TTF_Font *f, int x, int y, const char *txt) 
{
    SDL_Color c = {255,255,255};
    SDL_Surface *s = TTF_RenderText_Solid(f, txt, c);
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect dst = {x, y, s->w, s->h};
    SDL_RenderCopy(r, t, NULL, &dst);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

void add_oscillator() 
{
    if (osc_count >= MAX_OSC) return;

    osc[osc_count].type = WAVE_SINE;
    osc[osc_count].freq = 220;
    osc[osc_count].amp = 0.5;
    osc[osc_count].phase = 0;

    selected = osc_count;
    osc_count++;
}

void remove_oscillator(int index)
{
    if (index < 0 || index >= osc_count) return;

    for (int i = index; i < osc_count - 1; i++)
        osc[i] = osc[i + 1];

    osc_count--;

    if (osc_count == 0)
        selected = -1;
    else if (selected >= osc_count)
        selected = osc_count - 1;
}

void draw_wave(SDL_Renderer *r)
{
    int ox = 20;
    int oy = 300;
    int w = 640;
    int h = 100;

    SDL_SetRenderDrawColor(r, 40,40,40,255);
    SDL_Rect bg = {ox, oy, w, h};
    SDL_RenderFillRect(r, &bg);

    SDL_SetRenderDrawColor(r, 0,255,120,255);

    for (int i = 0; i < WAVE_BUF - 1; i++)
    {
        int i1 = (wave_pos + i) % WAVE_BUF;
        int i2 = (wave_pos + i + 1) % WAVE_BUF;

        float v1 = wave_vis[i1];
        float v2 = wave_vis[i2];

        int x1 = ox + i * w / WAVE_BUF;
        int x2 = ox + (i + 1) * w / WAVE_BUF;

        int y1 = oy + h/2 - v1 * h/2;
        int y2 = oy + h/2 - v2 * h/2;

        SDL_RenderDrawLine(r, x1, y1, x2, y2);
    }
}


int main() 
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window *win = SDL_CreateWindow("Synthesizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 700, 500, 0);
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

    int run = 1;
    SDL_Event e;

    while (run) {
        while (SDL_PollEvent(&e)) 
        {
            if (e.type == SDL_QUIT)
                run = 0;

            if (e.type == SDL_MOUSEBUTTONDOWN) 
            {
                int x = e.button.x;
                int y = e.button.y;

                if (x > button_add.x && x < button_add.x + button_add.w &&
                    y > button_add.y && y < button_add.y + button_add.h) 
                {
                    add_oscillator();
                }

                int start_y = 90;
                for (int i = 0; i < osc_count; i++) 
                {
                    if (y > start_y + i * 35 && y < start_y + i * 35 + 30) 
                    {
                        selected = i;
                    }
                }
            }

            if (e.type == SDL_KEYDOWN)
            {

                if (e.key.keysym.sym == SDLK_ESCAPE)
                    run = 0;

                if (e.key.keysym.sym == SDLK_SPACE)
                    playing = !playing;

                if (selected >= 0) 
                {
                    Oscillator *o = &osc[selected];

                    if (e.key.keysym.sym == SDLK_TAB)
                        o->type = (o->type + 1) % 4;

                    if (e.key.keysym.sym == SDLK_UP) o->freq += 10;
                    if (e.key.keysym.sym == SDLK_DOWN && o->freq > 10) o->freq -= 10;
                    if (e.key.keysym.sym == SDLK_RIGHT && o->amp < 1) o->amp += 0.05;
                    if (e.key.keysym.sym == SDLK_LEFT && o->amp > 0) o->amp -= 0.05;
                    if (e.key.keysym.sym == SDLK_BACKSPACE && selected >= 0)
                    {
                        remove_oscillator(selected);
                        continue;
                    }
                }
            }
        }

        SDL_SetRenderDrawColor(ren, 20,20,20,255);
        SDL_RenderClear(ren);

        SDL_SetRenderDrawColor(ren, 60,60,200,255);
        SDL_RenderFillRect(ren, &button_add);
        draw_text(ren, font, 35, 28, "+");

        int y = 90;
        for (int i = 0; i < osc_count; i++) 
        {

            if (i == selected) 
            {
                SDL_SetRenderDrawColor(ren, 80,80,80,255);
                SDL_Rect hl = {20, y - 4, 640, 30};
                SDL_RenderFillRect(ren, &hl);
            }

            char buf[128];
            sprintf(buf, "Wave %d: %s  F=%.0fHz  A=%.2f", i+1, wave_names[osc[i].type], osc[i].freq, osc[i].amp);
            draw_text(ren, font, 30, y, buf);
            y += 35;
        }

        draw_text(ren, font, 30, 420, "SPACE play | TAB type | arrows freq/amp | click select | ESC exit");
        draw_wave(ren);

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_CloseAudio();
    TTF_Quit();
    SDL_Quit();
}
