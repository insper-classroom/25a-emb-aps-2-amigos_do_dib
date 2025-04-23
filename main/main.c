#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/adc.h"
#include "hardware/gpio.h"

typedef struct adc {
    int axis;
    int val;
} adc_t;

QueueHandle_t xQueueADC;

int media_movel(const int *buffer){
    int soma = 0;
    for (int i = 0; i < 5; i++) {
        soma += buffer[i];
    }
    return soma / 5;
}

int aplicar_transformacao(int leitura){
    int cent = leitura - 2048;
    int reduzido = (cent * 255)/2047;
    if (reduzido > -30 && reduzido < 30) {
        reduzido = 0;
    }
    return reduzido;
}

void x_task(void *p) {
    adc_init();
    adc_gpio_init(26);
    int buffer[5] = {0};
    int index = 0;
    adc_t data;
    while (1) {
        adc_select_input(0);
        int valor_adc = adc_read();
        buffer[index] = valor_adc;
        index = (index + 1) % 5;
        int media = media_movel(buffer);
        int transformacao = aplicar_transformacao(media);
        transformacao = -transformacao;
        data.axis = 0;
        data.val = transformacao;
        if (data.val != 0) {
            xQueueSend(xQueueADC, &data, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void y_task(void *p) {
    adc_init();
    adc_gpio_init(27);
    int buffer[5] = {0};
    int index = 0;
    adc_t data;
    while (1) {
        adc_select_input(1);
        int valor_adc = adc_read();
        buffer[index] = valor_adc;
        index = (index + 1) % 5;
        int media = media_movel(buffer);
        int transformacao = aplicar_transformacao(media);
        data.axis = 1;
        data.val = transformacao;
        if (data.val != 0){
            xQueueSend(xQueueADC, &data, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void button_task(void *p) {
    const uint8_t pins[5] = {2, 3, 4, 5, 6};
    bool prev[5] = {false, false, false, false, false};

    for (int i = 0; i < 5; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }

    while (1) {
        for (int i = 0; i < 5; i++) {
            bool pressed = !gpio_get(pins[i]);
            if (pressed && !prev[i]) {
                adc_t data = { .axis = 4 + i, .val = 1 };
                xQueueSend(xQueueADC, &data, portMAX_DELAY);
            }
            prev[i] = pressed;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void joystick_task(void *p) {
    const uint8_t pins[4] = {10, 11, 12, 13};  // UP, DOWN, LEFT, RIGHT
    const uint8_t axis_map[4] = {9, 10, 11, 12};
    bool prev[4] = {false};

    for (int i = 0; i < 4; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }

    while (1) {
        for (int i = 0; i < 4; i++) {
            bool pressed = !gpio_get(pins[i]);
            if (pressed && !prev[i]) {
                adc_t data = {.axis = axis_map[i], .val = 1};
                xQueueSend(xQueueADC, &data, portMAX_DELAY);
            }
            prev[i] = pressed;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void uart_task(void *p){
    adc_t data;
    while (1) {
        if (xQueueReceive(xQueueADC, &data, portMAX_DELAY)) {
           uint8_t axis = data.axis;
           uint16_t value = data.val;
           uint8_t mais = (uint8_t)((value >> 8) & 0xFF);
           uint8_t menos = (uint8_t)(value & 0xFF);
           uint8_t eop = 0xFF;
           putchar_raw(axis);
           putchar_raw(menos);
           putchar_raw(mais);
           putchar_raw(eop);
        }
    }
}

int main() {
    stdio_init_all();
    xQueueADC = xQueueCreate(32, sizeof(adc_t));

    xTaskCreate(button_task, "button task", 2048, NULL, 1, NULL);
    xTaskCreate(joystick_task, "joystick task", 2048, NULL, 1, NULL);
    xTaskCreate(x_task, "x task", 4095, NULL, 1, NULL);
    xTaskCreate(y_task, "y task", 4095, NULL, 1, NULL);
    xTaskCreate(uart_task, "uart task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true);
}