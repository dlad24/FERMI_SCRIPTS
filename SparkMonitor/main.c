#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

//CODE RUN ON PI PICO

const uint delay = 100;

float average(float arr[]){
    float sum = 0;
    for (int i = 0;i<10;i++){
        sum+= arr[i];
    }
    return sum/10;
}

int main(){
    stdio_init_all();
    printf("ADC readout GPIO27");

    //Initialize adc pins
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);
    adc_gpio_init(28);
    const float conversion_factor = 3.3f/(1 <<12); //conversion factor for pin

    float background[3][50]; // background avg to remove pedestal
    int index = 0; // index of current background array
    int heartbeat = 0; 
    while(1){
        float values[3]; // arr to store current values

        //cycle through each pin, read pin value, add to values arr and background
        for (int i = 0;i<3;i++){
            adc_select_input(i);
            uint16_t result = adc_read();
            background[i][index] = result*conversion_factor;
            values[i] = result*conversion_factor - average(background[i]);            
        }
        sleep_ms(delay); // mininum delay to prevent serial clog
        // if spark is detected (above background) or if heartbeat hits 50, print out values 
        // provides full resolution of sparks while avoiding excess serial clog
        for (int i = 0;i<3;i++){
            if (values[i] > 0.01 || heartbeat > 48){
                absolute_time_t current_time = get_absolute_time();
                uint32_t t = time_us_64();
                printf("%f %f %f \n",values[0],values[1],values[2]);
                break;
            }
        }

        //deal with index values
        index++;
        heartbeat++;
        if (index > 49){
            index = 0;
            heartbeat = 0;
        }
        

    }
}