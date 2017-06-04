#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include "mzapo_parlcd.h"
#include "mzapo_phys.h"
#include "mzapo_regs.h"
#include "font_types.h"
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <errno.h>
 
unsigned int pixels[480][320];
 
 
double interestingC[7][2];
 
void drawJuliaSet(double c1, double c2, int maxIter) {
        int width = 480;
        int height = 320;
        double cX = c1;
        double cY = c2;
        double x, y;

        for (int pixelX = 0; pixelX < width; pixelX++) {
            for (int pixelY = 0; pixelY < height; pixelY++) {
                 
                x = 1.5*(pixelX - 0.5*width) / (0.5 * width);
                y = (pixelY - 0.5*height) / (0.5 * height);
                int i=0;
                for (i = 0; i < maxIter; i++) {
                    if (x * x + y * y < 4) {
                        double tmp = x * x - y * y + cX;
                        y = 2.0 * x * y + cY;
                        x = tmp;
                        i++;
                    }
                    else break;
                }
// CHANGE ME  
        unsigned int c;
        int r,g,b;

        if(i < 100){
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
         
         
        c = (r << 11) | (g << 5) | b;   
        pixels[pixelX][pixelY]=c;

// END OF CHANGE ME
 
            }
        }
    }
 
//from wikipedia :))))))
void setConst(){
    interestingC[0][0] = -0.4;
    interestingC[0][1] = 0.6;
    interestingC[1][0] = 0.285;
    interestingC[1][1] = 0;
    interestingC[2][0] = 0.285;
    interestingC[2][1] = 0.01;
    interestingC[3][0] = 0.45;
    interestingC[3][1] = 0.1428;
    interestingC[4][0] = -0.70176;
    interestingC[4][1] = -0.3842;
    interestingC[5][0] = -0.835;
    interestingC[5][1] = -0.2321;
    interestingC[6][0] = -0.8;
    interestingC[6][1] = 0.156;
     
}
 
 
    int main(int argc, char *argv[]){
        setConst();
        unsigned char *mem_base;
        unsigned char *parlcd_mem_base;
        int i, j;
        parlcd_mem_base = map_phys_address(PARLCD_REG_BASE_PHYS, PARLCD_REG_SIZE, 0);
        mem_base = map_phys_address(SPILED_REG_BASE_PHYS, SPILED_REG_SIZE, 0); 
        parlcd_hx8357_init(parlcd_mem_base);
        parlcd_write_cmd(parlcd_mem_base, 0x2c);
        if (mem_base == NULL)
            exit(1);
        uint32_t oldR;
        uint32_t oldG;
        uint32_t oldB;
        double c1 = -0.70176;
        double c2 = -0.3842;
        /*double c1 = -0.4;
        double c2 = 0.6;*/
        uint32_t rgb_knobs_value;
        rgb_knobs_value = *(volatile uint32_t*)(mem_base+SPILED_REG_KNOBS_8BIT_o);
        oldR = rgb_knobs_value>>16 & 255;
        oldG = rgb_knobs_value>>8 & 255;
        oldB = rgb_knobs_value & 255;
        // CHANGE_ME 
        while (1) {
            struct timespec loop_delay = {.tv_sec = 0, .tv_nsec = 200000000};
            rgb_knobs_value = *(volatile uint32_t*)(mem_base+SPILED_REG_KNOBS_8BIT_o);
            if((rgb_knobs_value>>16 & 255) > oldR){
                printf("You healed KennyR u bastard!\n");
                oldR = rgb_knobs_value>>16 & 255;
                if(c1 < 1){
                    c1 += 0.02;             
                }
                else{
                    c1 = -1;
                }
            }
            else if((rgb_knobs_value>>16 & 255) < oldR){
                printf("You killed KennyR u bastard!\n");
                oldR = rgb_knobs_value>>16 & 255;
                if(c1 < 1){
                    c1-=0.02;               
                }
                else{
                    c1=1;               
                }
            }
            if((rgb_knobs_value>>8 & 255) > oldG){
                printf("You healed KennyG u bastard!\n");
                oldG = rgb_knobs_value>>8 & 255;
                if(c2 < 1){
                    c2+=0.02;               
                }
                else{
                    c2=-1;              
                }
            }
            else if((rgb_knobs_value>>8 & 255) < oldG){
                printf("You killed KennyG u bastard!\n");
                oldG = rgb_knobs_value>>8 & 255;
                if(c2 < 1){
                    c2-=0.02;               
                }
                else{
                    c2=1;               
                }
            }
            if((rgb_knobs_value >> 24 & 1)== 1){
                printf("Blue pressed! Starting shopping settings\n");
                int index = 0;          
                while(1){
                    printf("%d\n",index);
                    clock_nanosleep(CLOCK_MONOTONIC, 0, &loop_delay, NULL);
                    rgb_knobs_value = *(volatile uint32_t*)(mem_base+SPILED_REG_KNOBS_8BIT_o);
                    if((rgb_knobs_value >> 24 & 1)== 1){
                        printf("Stopping.\n");  
                        rgb_knobs_value = 0;                    
                        break;
                    } 
                    drawJuliaSet(interestingC[index][0],interestingC[index][1],300);
                    for (i = 0; i < 320 ; i++) {
                        for (j = 0; j < 480 ; j++) {
                            parlcd_write_data(parlcd_mem_base, pixels[j][i]);
                        }
                    }
                     
                     
                    if(index == 6) index = 0;
                    else index++;
                    clock_nanosleep(CLOCK_MONOTONIC, 0, &loop_delay, NULL);
                }
                 
                 
                 
            }
 
            drawJuliaSet(c1,c2,300);    
            for (i = 0; i < 320 ; i++) {
                for (j = 0; j < 480 ; j++) {
                    parlcd_write_data(parlcd_mem_base, pixels[j][i]);
                }
            }
            clock_nanosleep(CLOCK_MONOTONIC, 0, &loop_delay, NULL);
        }
        // END OF CHANGE_ME
        return 0;
    }
