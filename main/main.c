#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include <stdio.h>

typedef struct {
    int axis;
    int val;
} adc_t;

#define SWITCH_PIN 14   // GP14 lê o switch
#define LED_PIN    15   // GP15 acende o LED

static QueueHandle_t xQueueADC;
static volatile bool controlEnabled = false;

// Média móvel simples de 5 leituras
static int media_movel(const int *buffer) {
    int soma = 0;
    for (int i = 0; i < 5; i++) soma += buffer[i];
    return soma / 5;
}

// Dead-zone e escala para [-255..255]
static int aplicar_transformacao(int leitura) {
    int cent     = leitura - 2048;
    int reduzido = (cent * 255) / 2047;
    if (reduzido > -30 && reduzido < 30) reduzido = 0;
    return reduzido;
}

// Task que monitora o switch e controla o LED
void switch_task(void *p) {
    gpio_init(SWITCH_PIN);
    gpio_set_dir(SWITCH_PIN, GPIO_IN);
    gpio_pull_up(SWITCH_PIN);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    bool last = false;
    while (1) {
        bool closed = !gpio_get(SWITCH_PIN);  // LOW = switch fechado
        if (closed != last) {
            controlEnabled = closed;
            gpio_put(LED_PIN, closed ? 1 : 0);
            last = closed;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Eixo X analógico (GP26 → ADC0)
void x_task(void *p) {
    adc_init();
    adc_gpio_init(26);
    int buffer[5] = {0}, idx = 0;
    adc_t data;

    while (1) {
        if (!controlEnabled) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        adc_select_input(0);
        buffer[idx] = adc_read();
        idx = (idx + 1) % 5;

        int t = aplicar_transformacao(media_movel(buffer));
        t = -t;

        data.axis = 0;
        data.val  = t;
        if (t != 0) xQueueSend(xQueueADC, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Eixo Y analógico (GP27 → ADC1)
void y_task(void *p) {
    adc_init();
    adc_gpio_init(27);
    int buffer[5] = {0}, idx = 0;
    adc_t data;

    while (1) {
        if (!controlEnabled) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        adc_select_input(1);
        buffer[idx] = adc_read();
        idx = (idx + 1) % 5;

        int t = aplicar_transformacao(media_movel(buffer));

        data.axis = 1;
        data.val  = t;
        if (t != 0) xQueueSend(xQueueADC, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Botões digitais (GP2…GP6): envia 1 ao pressionar, 0 ao soltar
void button_task(void *p) {
    const uint8_t pins[5] = {2, 3, 4, 5, 6};
    bool prev[5] = {false};

    for (int i = 0; i < 5; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }

    while (1) {
        if (!controlEnabled) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        for (int i = 0; i < 5; i++) {
            bool pressed = !gpio_get(pins[i]);
            if (pressed != prev[i]) {
                adc_t d = {
                    .axis = 4 + i,
                    .val  = pressed ? 1 : 0
                };
                xQueueSend(xQueueADC, &d, portMAX_DELAY);
                prev[i] = pressed;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Joystick digital (GP10=UP, GP11=DOWN, GP12=LEFT, GP13=RIGHT)
// axis_map → {9,10,11,12} para casar com seu key_map no Python
void joystick_task(void *p) {
    const uint8_t pins[4]     = {10, 11, 12, 13};
    
    const uint8_t axis_map[4] = {11, 9, 12, 10 };
    bool prev[4] = {false};

    for (int i = 0; i < 4; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }

    while (1) {
        if (!controlEnabled) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        for (int i = 0; i < 4; i++) {
            bool pressed = !gpio_get(pins[i]);
            if (pressed != prev[i]) {
                adc_t d = {
                    .axis = axis_map[i],
                    .val  = pressed ? 1 : 0
                };
                xQueueSend(xQueueADC, &d, portMAX_DELAY);
                prev[i] = pressed;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// UART: envia [axis][lo][hi][0xFF]
void uart_task(void *p) {
    adc_t data;
    while (1) {
        if (!controlEnabled) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (xQueueReceive(xQueueADC, &data, portMAX_DELAY)) {
            uint8_t axis = data.axis;
            uint16_t v   = data.val;
            uint8_t lo   = v & 0xFF;
            uint8_t hi   = (v >> 8) & 0xFF;

            putchar_raw(axis);
            putchar_raw(lo);
            putchar_raw(hi);
            putchar_raw(0xFF);
        }
    }
}

int main() {
    stdio_init_all();
    xQueueADC = xQueueCreate(32, sizeof(adc_t));

    xTaskCreate(switch_task,   "SWITCH",  1024, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(x_task,        "XAXIS",   4096, NULL, 1, NULL);
    xTaskCreate(y_task,        "YAXIS",   4096, NULL, 1, NULL);
    xTaskCreate(button_task,   "BUTTON",  2048, NULL, 1, NULL);
    xTaskCreate(joystick_task, "JOYSTICK",2048, NULL, 1, NULL);
    xTaskCreate(uart_task,     "UART",    4096, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1) {;}
    return 0;
}
