#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "mzapo_parlcd.h"
#include "mzapo_phys.h"
#include "mzapo_regs.h"
#include "font_types.h"
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <errno.h>

#define WIDTH 480
#define HEIGHT 320

//const int WIDTH = 480;
//const int HEIGHT = 320;
const int ITER = 300;
const double I_C1 = -0.0835;
const double I_C2 = -0.2321;
 
uint16_t fb[HEIGHT][WIDTH];
double c_data[6][2];
int exit_cond = 0;
unsigned char *lcd_base;

double c1;
double c2;
int shopmod;

struct t_init {
    pthread_cond_t work_rdy;
    pthread_mutex_t work_mtx;
};
 
void gen_julia(double, double, int);
void c_data_init();
static void *render_worker(void *);
void show_fb();
int go_shop(struct t_init *data);


void gen_julia(double cX, double cY, int iter_count)
{
    uint16_t color;
    double x, y;
    unsigned int r, g, b;
    int i;

    for (int pixelX = 0; pixelX < WIDTH; pixelX++) {
        for (int pixelY = 0; pixelY < HEIGHT; pixelY++) {
            // x & y between (-2;2) 
            x = 1.5*(pixelX - 0.5*WIDTH) / (0.5 * WIDTH);
            y = (pixelY - 0.5*HEIGHT) / (0.5 * HEIGHT);
            for (i = 0; i < iter_count; i++) {
                if (x * x + y * y < 4) {
                    y = 2.0 * x * y + cY;
                    x = x * x - y * y + cX;
                } else break;
            }

            if(i < 100) {
                r = i%31;
                g = i%17;
                b = i%7;
            } else if(i < 150) {
                r = i%7;
                g = i%17;
                b = i%31;
            } else if(i < 250) {
                r = i%7;
                g = i%31;
                b = i%17;
            } else {
                r = i%31;
                g = i%31;
                b = i%31;
            }
             
             
            color = (r << 11) | (g << 5) | b;   
            fb[pixelY][pixelX] = color;
        }
    }
}
 
void c_data_init()
{
    c_data[0][0] = 0.285;
    c_data[0][1] = 0;
    c_data[1][0] = -0.4;
    c_data[1][1] = 0.6;
    c_data[2][0] = 0.285;
    c_data[2][1] = 0.01;
    c_data[3][0] = 0.45;
    c_data[3][1] = 0.1428;
    c_data[4][0] = -0.835;
    c_data[4][1] = -0.2321;
    c_data[5][0] = -0.70176;
    c_data[5][1] = -0.3842;
}
 
static void *render_worker(void *data)
{
    struct t_init *init_data = (struct t_init *) data;
    double lastC1 = 0;
    double lastC2 = 0;
    int c_idx = 0;

    pthread_mutex_lock(&init_data->work_mtx);

    while (!exit_cond) {
        pthread_cond_wait(&init_data->work_rdy, &init_data->work_mtx);

        if (shopmod) {
            while(shopmod) {
                gen_julia(c_data[c_idx][0], c_data[c_idx][1], ITER);
                show_fb();
                c_idx = ++c_idx % 5;
            }
        } else {
            if (c1 != lastC1 && c2 != lastC2) {
                lastC1 = c1;
                lastC2 = c2;
                gen_julia(lastC1, lastC2, ITER);
                show_fb();
            }
        }
    }

    pthread_mutex_unlock(&init_data->work_mtx);
}

void show_fb()
{
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x += 2) {
            parlcd_write_data2x(lcd_base, (uint32_t) *(&fb[y][x]));
        }
    }
}
 
int main(int argc, char *argv[])
{
    c_data_init();
    unsigned char *mem_base;
    uint32_t old_r, old_g, old_b;
    double lc1 = I_C1;
    double lc2 = I_C2;
    volatile uint32_t knob_val;
    struct t_init *thread_init;
    pthread_t tinfo;
    int d_shop = 0;
    struct timespec delay = {.tv_sec = 0, .tv_nsec = 200000000};

    if ((lcd_base = map_phys_address(PARLCD_REG_BASE_PHYS, PARLCD_REG_SIZE, 0)) == NULL) return 1;
    if ((mem_base = map_phys_address(SPILED_REG_BASE_PHYS, SPILED_REG_SIZE, 0)) == NULL) return 1;
    parlcd_hx8357_init(lcd_base);
    parlcd_write_cmd(lcd_base, 0x2c);

    if ((thread_init = malloc(sizeof(struct t_init))) == NULL) {
        fputs("failed to initialize thread data", stderr);
        return 1;
    }

    // thread init
    pthread_cond_init(&thread_init->work_rdy, NULL);
    pthread_mutex_init(&thread_init->work_mtx, NULL);
    if (pthread_create(&tinfo, 0, &render_worker, &thread_init) != 0) {
        fputs("failed to create thread", stderr);
        return 1;
    }

    knob_val = *(volatile uint32_t*)(mem_base+SPILED_REG_KNOBS_8BIT_o);
    old_r = knob_val >> 16 & 255;
    old_g = knob_val >> 8 & 255;
    old_b = knob_val  & 255;

    while (1) {
        if (d_shop) {
            d_shop = (go_shop(thread_init)) ? 0 : 1;
        } else if ((knob_val >> 16 & 255) > old_r) {
            fputs("c1 inc", stderr);
            old_r = knob_val >> 16 & 255;
            lc1 = (lc1 < 1) ? lc1 + 0.02 : -1;
        } else if ((knob_val >> 16 & 255) < old_r) {
            fputs("c1 dec", stderr);
            old_r = knob_val >> 16 & 255;
            lc1 = (lc1 > -1) ? lc1 - 0.02 : 1;
        } if ((knob_val >> 8 & 255) > old_g) {
            fputs("c2 inc", stderr);
            old_g = knob_val >> 8 & 255;
            lc2 = (lc2 < 1) ? lc2 + 0.02 : -1;
        } else if ((knob_val >> 8 & 255) < old_g) {
            fputs("c2 dec", stderr);
            old_g = knob_val >> 8 & 255;
            lc2 = (lc2 > -1) ? lc2 - 0.02 : 1;
        } else if ((knob_val >> 24 & 1) == 1) {
            d_shop = (go_shop(thread_init)) ? 0 : 1;
        } else {
            if (pthread_mutex_trylock(&thread_init->work_mtx) != 0) {
                fputs("still rendering, deffering", stderr);
            } else {
                c1 = lc1;
                c2 = lc2;
                shopmod = 0;
                pthread_cond_signal(&thread_init->work_rdy);
                pthread_mutex_unlock(&thread_init->work_mtx);
            }
        }
        clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, NULL);
    }
    return 0;
}

int go_shop(struct t_init *thread)
{
    if (shopmod) {
        shopmod = 0;
    } else {
        fputs("shop mode", stderr);
        if (pthread_mutex_trylock(&thread->work_mtx) != 0) {
            fputs("still rendering, deffering work", stderr);
            return 0;
        } else {
            shopmod = 1;
            pthread_cond_signal(&thread->work_rdy);
            pthread_mutex_unlock(&thread->work_mtx);
        }
    }
    return 1;
}
