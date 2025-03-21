/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const uint TRIGGER = 17;
const uint ECHO = 16;

QueueHandle_t xQueueButId;
QueueHandle_t xQueueButId_d;

SemaphoreHandle_t xSemaphore_e;

void pin_callback(uint gpio, uint32_t events) {
    if (events == 0x8) {
        absolute_time_t echo_init = get_absolute_time();
        xQueueSend(xQueueButId, &echo_init, pdMS_TO_TICKS(10));
    } else if (events == 0x4) {
        absolute_time_t echo_end = get_absolute_time();
        xQueueSend(xQueueButId, &echo_end, pdMS_TO_TICKS(10));
    }
}

void trigger_task(void *p) {
    gpio_init(TRIGGER);
    gpio_set_dir(TRIGGER, GPIO_OUT);

    while (true) {
        gpio_put(TRIGGER, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_put(TRIGGER, 0);
        xSemaphoreGive(xSemaphore_e);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void echo_task(void *p) {
    gpio_init(ECHO);
    gpio_set_dir(ECHO, GPIO_IN);

    gpio_set_irq_enabled_with_callback(ECHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &pin_callback);

    int echo_init = 0;
    int echo_end = 0;
    while (true) {
        if (xQueueReceive(xQueueButId, &echo_init, pdMS_TO_TICKS(10))) {
            if (xQueueReceive(xQueueButId, &echo_end, pdMS_TO_TICKS(5))) {
                int echo_duration = absolute_time_diff_us(echo_init, echo_end);
                    int dist = (0.0340 * echo_duration) / 2.0;
                xQueueSend(xQueueButId_d, &dist, pdMS_TO_TICKS(5));
            }
        }
    }
}

void oled_task(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    char cnt = 15;
    while (1) {

        int dist = 0;

            // gfx_draw_string(&disp, 0, 10, 1, dist);
        // if (xSemaphoreTake(xSemaphore_e, pdMS_TO_TICKS(500)) == pdTRUE) {
        if (xQueueReceive(xQueueButId_d, &dist, pdMS_TO_TICKS(50))) {
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "DISTANCIA:");

            if ((dist < 2)||(dist > 400)){
                gfx_draw_string(&disp, 0, 10, 1, "Falha");
            } else {
                char dist_str[10];                          // Assuming a reasonable length for the integer to string conversion
                sprintf(dist_str, "%d", dist);              // Convert int to string
                gfx_draw_string(&disp, 0, 10, 1, dist_str); // Draw the string representation

                cnt = dist*112/75;
                gfx_draw_line(&disp, 15, 27, cnt, 27);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        } else {
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 10, 1, "Falha");
        }
        gfx_show(&disp);
        //}
    }
}

int main() {
    stdio_init_all();

    xQueueButId = xQueueCreate(32, sizeof(int));
    xQueueButId_d = xQueueCreate(32, sizeof(int));

    xSemaphore_e = xSemaphoreCreateBinary();

    // xTaskCreate(oled1_demo_1, "Demo 1", 4095, NULL, 1, NULL);
    // xTaskCreate(oled1_demo_2, "Demo 2", 4095, NULL, 1, NULL);oled_task
    xTaskCreate(trigger_task, "Trigger Task", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "Oled Task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
