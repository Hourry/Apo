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
#include <sys/socket.h>
#include <arpa/inet.h>

#define WIDTH 480
#define HEIGHT 320


const double I_C1 = -0.0305;
const double I_C2 = -0.0811;
font_descriptor_t font;

uint16_t pixels[HEIGHT][WIDTH];
double pattern[6][2];
int exit_cond = 0;
unsigned char *lcd_base;

unsigned int mod1 = 31;
unsigned int mod2 = 17;
unsigned int mod3 = 7;

int colourChange = 1;

double c1;
double c2;
int menu = 0;
int shopMod = 0;
int infMod = 0;
int udp_change = 0;


struct threadData {
    pthread_cond_t work_rdy;
    pthread_mutex_t work_mtx;
};

struct networkThreadData {
    pthread_cond_t *work_rdy;
    pthread_mutex_t *work_mtx;
    int socket;
};


void pattern_init();

void generateJuliaSet(double, double, int);

static void *render(void *);

void writeData();

int enterShopMod(struct threadData *data);

void showCurrentCoords();

static void *udpProcessor(void *);

void processString(char *, int, int, uint16_t);

void processChar(char, int, int, uint16_t);

void generateBackground(int, int, int, int, uint16_t);


/*!
 * function used to fill the pixels[6][2] array with values by our pattern.
 */

void pattern_init() {
    pattern[0][0] = -0.835;
    pattern[0][1] = 0, 6;
    pattern[1][0] = -0.4;
    pattern[1][1] = 0;
    pattern[2][0] = 0.285;
    pattern[2][1] = -0.3842;
    pattern[3][0] = 0.45;
    pattern[3][1] = 0.1428;
    pattern[4][0] = 0.285;
    pattern[4][1] = -0.2321;
    pattern[5][0] = -0.70176;
    pattern[5][1] = 0.01;
}

/*!
 * Funtion used to generate julia set and its colour, which works with global variables of modulo.
 *
 * @param cX current X coord
 * @param cY current Y coord
 * @param iter_count number of iterations to be made
 */

void generateJuliaSet(double cX, double cY, int iter_count) {
    uint16_t color;
    double x, y;
    unsigned int r, g, b;
    int i;

    for (int pixelX = 0; pixelX < WIDTH; pixelX++) {
        for (int pixelY = 0; pixelY < HEIGHT; pixelY++) {
            x = 1.5 * (pixelX - 0.5 * WIDTH) / (0.5 * WIDTH);
            y = (pixelY - 0.5 * HEIGHT) / (0.5 * HEIGHT);
            for (i = 0; i < iter_count; i++) {
                if (x * x + y * y < 4) {
                    y = 2.0 * x * y + cY;
                    x = x * x - y * y + cX;
                } else break;
            }

            if (i < 100) {
                r = i % mod1;
                g = i % mod2;
                b = i % mod3;
            } else if (i < 150) {
                r = i % mod3;
                g = i % mod2;
                b = i % mod1;
            } else if (i < 250) {
                r = i % mod3;
                g = i % mod1;
                b = i % mod2;
            } else {
                r = i % mod1;
                g = i % mod1;
                b = i % mod1;
            }


            color = (r << 1) | (g << 20) | b;
            pixels[pixelY][pixelX] = color * colourChange;
        }
    }
}

/*!
 * Funtion that renders data on the lcd, utilizing the previous GenerateJuliaSet function and accessing pixels array.
 *
 * @param thread_data the thread to which will the function render
 * @return NULL if there goes something wrong that had not been caught in the process
 */

static void *render(void *thread_data) {
    struct threadData *data = (struct threadData *) thread_data;
    int index = 0;
    pthread_mutex_lock(&data->work_mtx);

    while (!exit_cond) {
        pthread_cond_wait(&data->work_rdy, &data->work_mtx);
        if (shopMod) {
            while (shopMod) {
                fputs("shopMod thread\n", stderr);
                generateJuliaSet(pattern[index][0], pattern[index][1], 300);
                writeData();
                index++;
                index = index % 5;
                sleep(1);
            }
        } else if (infMod) {
            writeData();
            infMod = 0;
        } else {
            fputs("render thread\n", stderr);
            generateJuliaSet(c1, c2, 300);
            writeData();
        }
    }
    pthread_mutex_unlock(&data->work_mtx);

    return NULL;
}

/*!
 * Function to display data on the lcd using parlcd_write_data2x utility.
 */

void writeData() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x += 2) {
            parlcd_write_data2x(lcd_base, (uint32_t) *(&pixels[y][x]));
        }
    }
}

/*!
 * Function to display current coords.
 * Trigerred by the press of green button in normal mode (non menu mode).
 */

void showCurrentCoords() {
    char line1[WIDTH + 1];
    char line2[HEIGHT + 1];

    snprintf(line1, WIDTH + 1, "C1: %6.6f", c1);
    snprintf(line2, WIDTH + 1, "C2: %6.6f", c2);

    generateBackground(200, 0, 240, WIDTH, 0xFFFF);

    processString(line1, 200, 10, 0x0000);
    processString(line2, 220, 10, 0x0000);

}

/*!
 * Function to recognize input through UDP
 *
 * @param tdata used to initialize networkThreadData.
 * @return if some error that I havent yet seen raises.
 *
 */

void *udpProcessor(void *tdata) {
    struct networkThreadData *data = (struct networkThreadData *) tdata;
    char buf[100];
    int sock = data->socket;
    double lc1, lc2;

    while (!exit_cond) {
        recvfrom(sock, &buf, 100 * sizeof(char), 0, NULL, NULL);

        if (sscanf(buf, "<%lf> <%lf> ", &lc1, &lc2) == 2) {
            fputs("received data\n", stderr);
            pthread_mutex_lock(data->work_mtx);
            fputs("udp processor mutex locked", stderr);
            c1 = lc1;
            c2 = lc2;
            udp_change = 1;
            pthread_mutex_unlock(data->work_mtx);
        } else {
            fputs("wrong udp data\n", stderr);
        }
    }

    return NULL;
}

/*!
 * Function used to show string on the lcd.
 * Utilizes void processChar
 *
 * @param str input string
 * @param y0 coordinate of the first char in string
 * @param x0 height of the string
 * @param color color of the string
 */

void processString(char *str, int y0, int x0, uint16_t color) {
    int length = strlen(str);
    int x = x0;

    for (int i = 0; i < length; i++) {
        processChar(str[i], y0, x, color);
        x += 16;
    }
}

/*!
 * Function used to process basic char.
 * Utilizes embeded font_bits.
 *
 * @param ch input char
 * @param y0 height of the char
 * @param x0 size of the first char
 * @param color color of the char
 */

void processChar(char ch, int y0, int x0, uint16_t color) {
    const font_bits_t *curr_char = font.bits + ((ch - font.firstchar) * font.height);
    font_bits_t curr_line;

    for (int y = y0; y < (y0 + font.height); y++) {
        curr_line = *(curr_char + (y - y0));
        for (int x = x0; x < (x0 + 16); x++) {
            generateBackground(200, 0, 240, WIDTH, 0xFFFF);
            pixels[y][x] = (curr_line & 0x8000) ? color : pixels[y][x];
            curr_line <<= 1;
        }
    }
}

/*!
 * Function used to generate a rectangle. Accesses the global array pixels.
 *
 * @param y0 starting y coordinate of the rectangle
 * @param x0 starting x coordinate of the rectangle
 * @param y1 ending y coordinate of the rectangle
 * @param x1 ending x coordinate of the rectangle
 * @param color color of the rectangle
 */

void generateBackground(int y0, int x0, int y1, int x1, uint16_t color) {
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            pixels[y][x] = color;
        }
    }
}

/*!
 * Function to decide what to do when the red button is pressed while in non menu mode.
 * Pressing the red button again will set the global variable menu to zero and jumps straight back to the main function,
 * to continue the cycle until the user shuts it off.
 *
 * Also uses the global varibles of ColourChange which serves to multiply the tiles in pixels in the function GenerateJuliaSET
 * and mod1, mod2, mod3, where it modifies the modulo by 3 each time the knob is spinned.
 *
 * @param mem_base memory base to work with from the main function, so we dont have to initialize a new one
 * @param delay also inherited from the main function, so the delay stays the same
 * @param tdata the thread in which are the dat being processed
 */

void show_menu(unsigned char *mem_base, struct timespec *delay, struct threadData *tdata) {


    uint32_t old_r, old_g;
    volatile uint32_t knob_val;

    knob_val = *(volatile uint32_t *) (mem_base + SPILED_REG_KNOBS_8BIT_o);
    old_r = knob_val >> 16 & 255;
    old_g = knob_val >> 8 & 255;


    while (menu == 1) {
        sleep(1);
        knob_val = *(volatile uint32_t *) (mem_base + SPILED_REG_KNOBS_8BIT_o);
        if ((knob_val >> 16 & 255) > old_r) {
            // fputs("adjusting colour\n", stderr);
            old_r = knob_val >> 16 & 255;
            colourChange++;
            if (colourChange <= 16)colourChange = 16;
            if (pthread_mutex_trylock(&tdata->work_mtx) != 0) {
                //fputs("still rendering, deffering, colour\n", stderr);
            } else {
                fputs("trying to render\n", stdout);
                //c1 = lc1;
                //c2 = lc2;
                if (pthread_cond_signal(&tdata->work_rdy) != 0) {
                    pthread_mutex_unlock(&tdata->work_mtx);
                } else {
                    pthread_mutex_unlock(&tdata->work_mtx);
                }
            }
        } else if ((knob_val >> 16 & 255) < old_r) {
            // fputs("adjusting colour\n", stderr);
            old_r = knob_val >> 16 & 255;
            colourChange--;
            if (colourChange <= 0)colourChange = 1;
            if (pthread_mutex_trylock(&tdata->work_mtx) != 0) {
                //fputs("still rendering, deffering, colour\n", stderr);
            } else {
                //fputs("trying to render\n", stdout);
                if (pthread_cond_signal(&tdata->work_rdy) != 0) {
                    pthread_mutex_unlock(&tdata->work_mtx);
                } else {
                    pthread_mutex_unlock(&tdata->work_mtx);
                }
            }


        } else if ((knob_val >> 8 & 255) > old_g) {
            fputs("changing brightness\n", stderr);
            old_g = knob_val >> 8 & 255;
            mod1 += 3, mod2 += 3, mod3 += 3;
            if (mod1 > 255 || mod2 > 255 || mod3 > 255) {
                mod1 = 31, mod2 = 17, mod3 = 7;
            }
            if (pthread_mutex_trylock(&tdata->work_mtx) != 0) {
                fputs("still rendering, deffering, brightness\n", stderr);
            } else {
                fputs("trying to render\n", stdout);
                if (pthread_cond_signal(&tdata->work_rdy) != 0) {
                    pthread_mutex_unlock(&tdata->work_mtx);
                } else {
                    pthread_mutex_unlock(&tdata->work_mtx);
                }
            }
        } else if ((knob_val >> 8 & 255) < old_g) {
            fputs("changing brightness\n", stderr);
            old_g = knob_val >> 8 & 255;
            mod1 += -3, mod2 += -3, mod3 += -3;
            if (mod1 < 0 || mod2 < 0 || mod3 < 0) {
                mod1 = 31, mod2 = 17, mod3 = 7;
            }
            if (pthread_mutex_trylock(&tdata->work_mtx) != 0) {
                fputs("still rendering, deffering, brightness\n", stderr);
            } else {
                fputs("trying to render\n", stdout);
                if (pthread_cond_signal(&tdata->work_rdy) != 0) {
                    pthread_mutex_unlock(&tdata->work_mtx);
                } else {
                    pthread_mutex_unlock(&tdata->work_mtx);
                }
            }

        } else if ((knob_val >> 26 & 1) == 1) {
            fputs("exiting menu\n", stderr);
            menu = 0;
        } else {

        }
        clock_nanosleep(CLOCK_MONOTONIC, 0, delay, NULL);

    }


}

/*!
 * Function to enter and process the shopmode.
 *
 * @param thread thread where are the values going to be stored
 * @return 1 in case of success of the function
 */

int enterShopMod(struct threadData *thread) {
    if (shopMod) {
        shopMod = 0;
    } else {
        fputs("entered shop mode", stderr);
        if (pthread_mutex_trylock(&thread->work_mtx) != 0) {
            fputs("still rendering, deffering work", stderr);
            return 0;
        } else {
            shopMod = 1;
            pthread_cond_signal(&thread->work_rdy);
            pthread_mutex_unlock(&thread->work_mtx);
        }
    }
    return 1;
}

/*!
 * Main function of the whole project.
 * At first we initialize the pixels by function pattern_init(), then we procceed to initialize some variable so we can start
 * the two main threads. Then we initialize the UDP connection at port 44444, there are also several condtitions in case that something went wrong.
 * Then we procceed to the input diven while cycle, which is kept in check with human reactions by using clock_nanospeed().
 */


int main(int argc, char *argv[]) {
    pattern_init();
    unsigned char *mem_base;
    uint32_t old_r, old_g, old_b;
    double lc1 = I_C1;
    double lc2 = I_C2;
    volatile uint32_t knob_val;
    int d_shop = 0;
    int sock;
    font = font_winFreeSystem14x16;

    pthread_t rtinfo, utinfo;
    struct threadData tdata = {.work_mtx = PTHREAD_MUTEX_INITIALIZER, .work_rdy = PTHREAD_COND_INITIALIZER};
    struct networkThreadData utdata = {.work_mtx = &tdata.work_mtx, .work_rdy = &tdata.work_rdy};
    struct sockaddr_in addr;
    struct timespec delay = {.tv_sec = 0, .tv_nsec = 200000000};

    if ((lcd_base = map_phys_address(PARLCD_REG_BASE_PHYS, PARLCD_REG_SIZE, 0)) == NULL) {
        return 1;
    }
    if ((mem_base = map_phys_address(SPILED_REG_BASE_PHYS, SPILED_REG_SIZE, 0)) == NULL) {
        return 1;
    }
    parlcd_hx8357_init(lcd_base);
    parlcd_write_cmd(lcd_base, 0x2c);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fputs("failed to init udp socket", stderr);
        return 1;
    }
    //UDP sock
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(44444);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((bind(sock, (struct sockaddr *) &addr, sizeof(addr))) != 0) {
        fputs("failed to bind the udp socket to port 44 444", stderr);
        return 1;
    }
    utdata.socket = sock;

    // thread init
    if (pthread_create(&rtinfo, 0, &render, &tdata) != 0) {
        fputs("failed to create render thread", stderr);
        return 1;
    }
    if (pthread_create(&utinfo, 0, &udpProcessor, &utdata) != 0) {
        fputs("failed to create udp thread", stderr);
        return 1;
    }
    sleep(2);


    knob_val = *(volatile uint32_t *) (mem_base + SPILED_REG_KNOBS_8BIT_o);
    old_r = knob_val >> 16 & 255;
    old_g = knob_val >> 8 & 255;
    old_b = knob_val & 255;

    while (1) {
        knob_val = *(volatile uint32_t *) (mem_base + SPILED_REG_KNOBS_8BIT_o);
        if (udp_change) {
            puts("udp message received, locking mutex\n");
            pthread_mutex_lock(&tdata.work_mtx);
            lc1 = c1;
            lc2 = c2;
            udp_change = 0;
            if (pthread_cond_signal(&tdata.work_rdy) != 0) {
                fputs("udp failed to signal render thread\n", stderr);
            } else puts("udp signalled render thread");
            pthread_mutex_unlock(&tdata.work_mtx);
        } else if (d_shop) {
            fputs("d_shop\n", stdout);
            d_shop = (enterShopMod(&tdata)) ? 0 : 1;
        } else if ((knob_val >> 16 & 255) > old_r) {
            //fputs("c1 is being increased\n", stderr);
            old_r = knob_val >> 16 & 255;
            lc1 = (lc1 < 1) ? lc1 + 0.02 : -1;
        } else if ((knob_val >> 16 & 255) < old_r) {
            //fputs("c1 is being decreased\n", stderr);
            old_r = knob_val >> 16 & 255;
            lc1 = (lc1 > -1) ? lc1 - 0.02 : 1;
        }
        if ((knob_val >> 8 & 255) > old_g) {
            //fputs("c2 is being increased\n", stderr);
            old_g = knob_val >> 8 & 255;
            lc2 = (lc2 < 1) ? lc2 + 0.02 : -1;
        } else if ((knob_val >> 8 & 255) < old_g) {
            //fputs("c2 is being decreased\n", stderr);
            old_g = knob_val >> 8 & 255;
            lc2 = (lc2 > -1) ? lc2 - 0.02 : 1;
        } else if ((knob_val >> 24 & 1) == 1) {
            fputs("EnterShopMod\n", stdout);
            d_shop = (enterShopMod(&tdata)) ? 0 : 1;
        } else if ((knob_val >> 25 & 1) == 1) {
            showCurrentCoords();
            infMod = 1;
        } else if ((knob_val >> 26 & 1) == 1) {
            fputs("entering menu\n", stdout);
            show_menu(mem_base, &delay, &tdata);
            menu = 1;
        } else {
            if (lc1 != c1 || lc2 != c2 || infMod) {
                if (pthread_mutex_trylock(&tdata.work_mtx) != 0) {
                    fputs("still rendering, deffering\n", stderr);
                } else {
                    fputs("trying to render\n", stdout);
                    c1 = lc1;
                    c2 = lc2;
                    if (pthread_cond_signal(&tdata.work_rdy) != 0) {
                        fputs("failed to signal render thread\n", stderr);
                    } else puts("render thread signalled");
                    pthread_mutex_unlock(&tdata.work_mtx);
                }
            }
        }
        clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, NULL);
    }
    return 0;
}
