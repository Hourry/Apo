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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#define WIDTH 480
#define HEIGHT 320
#define CL_BLACK 0x0000
#define CL_WHITE 0x1111

//const int WIDTH = 480;
//const int HEIGHT = 320;
const int ITER = 300;
const double I_C1 = -0.0835;
const double I_C2 = -0.2321;
font_descriptor_t font;
 
uint16_t fb[HEIGHT][WIDTH];
double c_data[6][2];
int exit_cond = 0;
unsigned char *lcd_base;

double c1;
double c2;
int shopmod = 0;
int infomod = 0;

struct rt_data {
    pthread_cond_t work_rdy;
    pthread_mutex_t work_mtx;
};

struct ut_data {
    pthread_cond_t *work_rdy;
    pthread_mutex_t *work_mtx;
    int sock;
};
 
void gen_julia(double, double, int, int, int);
void c_data_init();
static void *render_worker(void *);
void show_fb();
int go_shop(struct rt_data *data);
static void *udp_worker(void *);
void show_info();
void display_str(char *, int , int , uint16_t);
void display_char(char , int , int , uint16_t);
void draw_rect(int , int , int , int , uint16_t);


void gen_julia(double cX, double cY, int oX, int oY,  int iter_count)
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
 
static void *render_worker(void *thread_data)
{
    struct rt_data *data = (struct rt_data *) thread_data;
    int c_idx = 0;
    fputs("thread init\n", stdout);

    pthread_mutex_lock(&data->work_mtx);
    fputs("mutex locked\n", stderr);

    while (!exit_cond) {
        pthread_cond_wait(&data->work_rdy, &data->work_mtx);
	    fputs("cond unlocked\n", stderr);

        if (shopmod) {
            while(shopmod) {
		        fputs("shopmod thread\n", stderr);
                gen_julia(c_data[c_idx][0], c_data[c_idx][1], 0, 0, ITER);
                show_fb();
                c_idx = c_idx + 1;
                c_idx = c_idx % 5;
                sleep(1);
            }
        } else if (infomod) {
            show_fb();
            infomod = 0;
        } else {
	        fputs("render thread\n", stderr);
            gen_julia(c1, c2, 0, 0, ITER);
            show_fb();
        }
    }

    pthread_mutex_unlock(&data->work_mtx);
}

void show_fb()
{
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x += 2) {
            parlcd_write_data2x(lcd_base, (uint32_t) *(&fb[y][x]));
        }
    }
}

void *udp_worker(void *tdata)
{
    struct ut_data *data = (struct ut_data *) tdata;
    char buf[100];
    int sock = data->sock;
    int x, y, c;

    while (!exit_cond) {
        recvfrom(sock, &buf, 100 * sizeof(char), 0, NULL, NULL);

        if (sscanf(buf, "<%d> <%d> <%d>", &x, &y, &c) == 3) {
            fputs("received data\n", stderr);
        } else {
            fputs("wrong udp data\n", stderr);
        }
    }
}

void show_info()
{
    char line1[WIDTH+1];
    char line2[HEIGHT+1];

    snprintf(line1, WIDTH+1, "C1: %6.6f", c1);
    snprintf(line2, WIDTH+1, "C2: %6.6f", c2);

    draw_rect(200, 0, 240, WIDTH, CL_WHITE);

    display_str(line1, 200, 10, CL_BLACK);
    display_str(line2, 220, 10, CL_BLACK);
}

void display_str(char *str, int y0, int x0, uint16_t color)
{
    int str_len = strlen(str);
    int x = x0;

    for (int i = 0; i < str_len; i++) {
        display_char(str[i], y0, x, color);
        x += font.maxwidth;
    }
}

void display_char(char ch, int y0, int x0, uint16_t color)
{
    const font_bits_t *curr_char = font.bits + ((ch - font.firstchar) * font.height);
    font_bits_t curr_line;

    for (int y = y0; y < (y0 + font.height); y++) {
        curr_line = *(curr_char + y);
        for (int x = (x0 + font.maxwidth-1); x >= x0; x--) {
            fb[y][x] = (curr_line & 0x1) ? color : fb[y][x];
            curr_line >>= 1;
        }
    }
}

void draw_rect(int y0, int x0, int y1, int x1, uint16_t color)
{
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            fb[y][x] = color;
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
    int d_shop = 0;
    int sock;
    font = font_winFreeSystem14x16;

    pthread_t rtinfo, utinfo;
    struct rt_data tdata = { .work_mtx = PTHREAD_MUTEX_INITIALIZER, .work_rdy = PTHREAD_COND_INITIALIZER };
    struct ut_data utdata = { .work_mtx = &tdata.work_mtx, .work_rdy = &tdata.work_rdy };
    struct sockaddr_in addr;
    struct timespec delay = {.tv_sec = 0, .tv_nsec = 200000000};

    if ((lcd_base = map_phys_address(PARLCD_REG_BASE_PHYS, PARLCD_REG_SIZE, 0)) == NULL) return 1;
    if ((mem_base = map_phys_address(SPILED_REG_BASE_PHYS, SPILED_REG_SIZE, 0)) == NULL) return 1;
    parlcd_hx8357_init(lcd_base);
    parlcd_write_cmd(lcd_base, 0x2c);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fputs("failed to init udp socket", stderr);
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(44444);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((bind(sock, (struct sockaddr *) &addr, sizeof(addr))) != 0) {
        fputs("failed to bind the udp socket to port 44 444", stderr);
        return 1;
    }
    utdata.sock = sock;

    // thread init
    if (pthread_create(&rtinfo, 0, &render_worker, &tdata) != 0) {
        fputs("failed to create render thread", stderr);
        return 1;
    }
    if (pthread_create(&utinfo, 0, &udp_worker, &utdata) != 0) {
        fputs("failed to create udp thread", stderr);
        return 1;
    }
    sleep(2);


    knob_val = *(volatile uint32_t*)(mem_base+SPILED_REG_KNOBS_8BIT_o);
    old_r = knob_val >> 16 & 255;
    old_g = knob_val >> 8 & 255;
    old_b = knob_val  & 255;

    while (1) {
        knob_val = *(volatile uint32_t*)(mem_base+SPILED_REG_KNOBS_8BIT_o);
        if (d_shop) {
	    fputs("d_shop\n", stdout);
            d_shop = (go_shop(&tdata)) ? 0 : 1;
        } else if ((knob_val >> 16 & 255) > old_r) {
            fputs("c1 inc\n", stderr);
            old_r = knob_val >> 16 & 255;
            lc1 = (lc1 < 1) ? lc1 + 0.02 : -1;
        } else if ((knob_val >> 16 & 255) < old_r) {
            fputs("c1 dec\n", stderr);
            old_r = knob_val >> 16 & 255;
            lc1 = (lc1 > -1) ? lc1 - 0.02 : 1;
        } if ((knob_val >> 8 & 255) > old_g) {
            fputs("c2 inc\n", stderr);
            old_g = knob_val >> 8 & 255;
            lc2 = (lc2 < 1) ? lc2 + 0.02 : -1;
        } else if ((knob_val >> 8 & 255) < old_g) {
            fputs("c2 dec\n", stderr);
            old_g = knob_val >> 8 & 255;
            lc2 = (lc2 > -1) ? lc2 - 0.02 : 1;
        } else if ((knob_val >> 24 & 1) == 1) {
	        fputs("go_shop\n", stdout);
            d_shop = (go_shop(&tdata)) ? 0 : 1;
        } else if ((knob_val >> 25 & 1) == 1) {
            show_info();
            infomod = 1;
        } else {
            if (lc1 != c1 || lc2 != c2 || infomod) {
                if (pthread_mutex_trylock(&tdata.work_mtx) != 0) {
                    fputs("still rendering, deffering\n", stderr);
                } else {
		            fputs("trying to render\n", stdout);
                    c1 = lc1;
                    c2 = lc2;
                    if (pthread_cond_signal(&tdata.work_rdy) != 0) {
                        pthread_mutex_unlock(&tdata.work_mtx);
                    } else {
                        pthread_mutex_unlock(&tdata.work_mtx);
                    }
                }
            }
        }
        clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, NULL);
    }
    return 0;
}

int go_shop(struct rt_data *thread)
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
