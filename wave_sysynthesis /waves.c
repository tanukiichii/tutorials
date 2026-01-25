#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE 44100
#define PI 3.14159265359

typedef enum 
{
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_SAW
} WaveType;

WaveType currentWave = WAVE_SINE;

double frequency = 110.0;
double amplitude = 0.7;

double sample_dt;
unsigned int sample_N;
unsigned int i_t = 0;

/* ===== генерация волн ===== */

double sine_wave(unsigned int i) 
{
    return sin(2 * PI * sample_dt * i);
}

double square_wave(unsigned int i) 
{
   float noise = sin( 2 * PI * sample_dt * i);
   if(noise >= 0)
   {
    noise = 1.0;
   }
   else 
   {
    noise = -1.0;
   }

   return noise;
}

double triangle_wave(unsigned int i) 
{
    return asin(sin(2 * PI * sample_dt * i));
}

double saw_wave(unsigned int i)
{
   float noise = 0.0;

   for (int n = 1 ; n < 40 ; n++)
   {
        noise += sin(n * 2 * PI * sample_dt * i) / n;
   }

   return noise;
}

void update_frequency() 
{
    sample_dt = frequency / SAMPLE_RATE;
    sample_N  = SAMPLE_RATE / frequency;
}

/* ===== аудио ===== */

void audio_callback(void *userdata, Uint8 *stream, int len) 
{
    float *buffer = (float *)stream;
    int samples = len / sizeof(float);

    for (int n = 0; n < samples; n++) 
    {
        double sample = 0.0;

        switch (currentWave) 
        {
            case WAVE_SINE:     sample = sine_wave(i_t); break;
            case WAVE_SQUARE:   sample = square_wave(i_t); break;
            case WAVE_TRIANGLE: sample = triangle_wave(i_t); break;
            case WAVE_SAW:      sample = saw_wave(i_t); break;
        }

        buffer[n] = sample * amplitude;

        i_t++;
        if (i_t >= sample_N) i_t = 0;
    }
}

/* ===== текст ===== */

const char* wave_name()
{
    switch (currentWave)
    {
        case WAVE_SINE: return "Sine wave";
        case WAVE_SQUARE: return "Square wave";
        case WAVE_TRIANGLE: return "Triangle wave";
        case WAVE_SAW: return "Saw wave";
    }
    return "";
}

void draw_text(SDL_Renderer *r, TTF_Font *font, int x, int y, const char *text)
{
    SDL_Color white = {255,255,255,255};

    SDL_Surface *surf = TTF_RenderText_Solid(font, text, white);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);

    SDL_Rect dst = {x, y, surf->w, surf->h};

    SDL_FreeSurface(surf);
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

/* ===== main ===== */

int main()
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
    TTF_Init();

    update_frequency();

    SDL_AudioSpec spec = {0};
    spec.freq = SAMPLE_RATE;
    spec.format = AUDIO_F32SYS;
    spec.channels = 1;
    spec.samples = 512;
    spec.callback = audio_callback;

    SDL_OpenAudio(&spec, NULL);
    SDL_PauseAudio(0);

    SDL_Window *window = SDL_CreateWindow(
        "Wave generator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        500, 300,
        0
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font *font = TTF_OpenFont("/System/Library/Fonts/SFNS.ttf", 18);

    int running = 1;
    SDL_Event e;

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) running = 0;

            if (e.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                    case SDLK_1: currentWave = WAVE_SINE; break;
                    case SDLK_2: currentWave = WAVE_SQUARE; break;
                    case SDLK_3: currentWave = WAVE_TRIANGLE; break;
                    case SDLK_4: currentWave = WAVE_SAW; break;

                    case SDLK_UP:   frequency += 50; update_frequency(); break;
                    case SDLK_DOWN: if (frequency > 50) frequency -= 50; update_frequency(); break;

                    case SDLK_RIGHT: if (amplitude < 1.0) amplitude += 0.05; break;
                    case SDLK_LEFT:  if (amplitude > 0.0) amplitude -= 0.05; break;

                    case SDLK_ESCAPE: running = 0; break;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

        draw_text(renderer, font, 20, 20, "1-4 change wave");
        draw_text(renderer, font, 20, 50, "Up/Down = frequency");
        draw_text(renderer, font, 20, 80, "Left/Right = amplitude");

        char info[128];
        sprintf(info, "Wave: %s", wave_name());
        draw_text(renderer, font, 20, 140, info);

        sprintf(info, "Freq: %.1f Hz", frequency);
        draw_text(renderer, font, 20, 170, info);

        sprintf(info, "Amp: %.2f", amplitude);
        draw_text(renderer, font, 20, 200, info);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_CloseAudio();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

