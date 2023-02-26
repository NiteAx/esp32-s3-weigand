#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define pinD0 GPIO_NUM_0 //D0 connected to GPIO0
#define pinD1 GPIO_NUM_1 //D1 connected to GPIO1

static QueueHandle_t gpio_evt_queue = NULL; //Define a queue handle
volatile uint64_t bitstream = 0; //Store bits we are reading
volatile int bitcount = 0; //Keep track of bits received

//ISR Handler function that is called when either pin interrupt is triggered
//ISR cannot have blocking functions and must execute as fast as possible
static void isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    if (gpio_num == pinD1) //swap these back
    {
        bitcount++; //Increment bitstream length
        bitstream = bitstream << 1; //Shift left, adding 0 to LSB
    }
    else if (gpio_num == pinD0) //swap these back
    {
        bitcount++; //Increment bitstream length
        bitstream = bitstream << 1; //Shift left, adding 0 to LSB
        bitstream |= 1; //OR with 1 sets LSB to 1
    }
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL); //Passes 
}

static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("Bitcount: %i, Bitstream: %llx\n", bitcount, bitstream);
            if (bitcount == 34)
            {
                bitstream = 0x1FFFFFFFF; //remove this later, forces bitstream to be a specific value
                bool even; //Even bit
                bool odd; //Odd bit
                uint16_t higher; //16 higher bits
                uint16_t lower; //16 lower bits

                even = (bitstream >> 33) & 1; //Shift right till MSB is LSB
                odd = bitstream & 1; //Store LSB
                
                higher = (bitstream >> 17) & 0xFFFF; //Shift right and discard MSB
                lower = (bitstream >> 1) & 0xFFFF; //Discard LSB and store lower 16 bits
                printf("Even: %i, Odd: %i, Higher: %x, Lower: %x\n", even, odd, higher,  lower);
                
                //16-bit Even parity algorithm
                higher ^= higher >> 8;
                higher ^= higher >> 4;
                higher ^= higher >> 2;
                higher ^= higher >> 1;
                higher = higher & 1; //LSB 0 is even, else odd

                if (higher == even)
                {
                    printf("Even check passed!\n");
                }
                else 
                {
                    printf("Even check failed!\n");
                }

                //16-bit Even parity algorithm
                lower ^= lower >> 8;
                lower ^= lower >> 4;
                lower ^= lower >> 2;
                lower ^= lower >> 1;
                lower = lower & 1; //LSB 0 is even, else odd

                if (!lower == odd)
                {
                    printf("Odd check passed!\n");
                }
                else 
                {
                    printf("Odd check failed!\n");
                }

                if (higher == even && !lower == odd) //If both checks pass
                {
                    uint32_t card_serial_num = (bitstream >> 1) & 0xFFFFFFFF; //Discard even and odd parity bits
                    printf("Card serial number : %lu, %lx\n", card_serial_num, card_serial_num);
                    bitstream = 0;
                    bitcount = 0;
                }
                else //If either checks fail
                {
                    printf("Error reading bitstream.");
                    bitstream = 0;
                    bitcount = 0;
                }
            }
        }
    }
}

void app_main(void)
{
    gpio_set_direction(pinD0, GPIO_MODE_INPUT); //Set D0 as input
    gpio_set_direction(pinD1, GPIO_MODE_INPUT); //Set D1 as input
    gpio_set_pull_mode(pinD0, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(pinD1, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(pinD0, GPIO_INTR_NEGEDGE); //Set D0 to trigger on falling edge
    gpio_set_intr_type(pinD1, GPIO_INTR_NEGEDGE); //Set D1 to trigger on falling edge

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(pinD0, isr_handler, (void *)pinD0);
    //gpio_isr_handler_add(pinD1, isr_handler, (void *)pinD1);
}