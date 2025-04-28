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

#define SWITCH_PIN 14
#define LED_PIN    15

static QueueHandle_t xQueueADC;
static QueueHandle_t xQueueControl;

static int media_movel(const int *buffer) {
    int soma = 0;
    for (int i = 0; i < 5; i++) soma += buffer[i];
    return soma / 5;
}

static int aplicar_transformacao(int leitura) {
    int cent     = leitura - 2048;
    int reduzido = (cent * 255) / 2047;
    if (reduzido > -30 && reduzido < 30) reduzido = 0;
    return reduzido;
}

void switch_task(void *p) {
    gpio_init(SWITCH_PIN);
    gpio_set_dir(SWITCH_PIN, GPIO_IN);
    gpio_pull_up(SWITCH_PIN);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    bool last_state = false;
    bool closed;

    while (1) {
        closed = !gpio_get(SWITCH_PIN);
        if (closed != last_state) {
            xQueueOverwrite(xQueueControl, &closed);
            gpio_put(LED_PIN, closed ? 1 : 0);
            last_state = closed;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

#define CHECK_CONTROL_STATE(local_var)                         \
    do {                                                       \
        bool _new;                                             
        if (xQueueReceive(xQueueControl, &_new, 0) == pdPASS)  
            local_var = _new;                                  
    } while (0)

void x_task(void *p) {
    adc_init();
    adc_gpio_init(26);
    int buffer[5] = {0}, idx = 0;
    adc_t data;
    bool enabled = false;

    while (1) {
        CHECK_CONTROL_STATE(enabled);
        if (!enabled) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        adc_select_input(0);
        buffer[idx++] = adc_read();
        idx %= 5;

        int t = aplicar_transformacao(media_movel(buffer));
        t = -t;

        data.axis = 0;
        data.val  = t;
        if (t != 0) xQueueSend(xQueueADC, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void y_task(void *p) {
    adc_init();
    adc_gpio_init(27);
    int buffer[5] = {0}, idx = 0;
    adc_t data;
    bool enabled = false;

    while (1) {
        CHECK_CONTROL_STATE(enabled);
        if (!enabled) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        adc_select_input(1);
        buffer[idx++] = adc_read();
        idx %= 5;

        int t = aplicar_transformacao(media_movel(buffer));

        data.axis = 1;
        data.val  = t;
        if (t != 0) xQueueSend(xQueueADC, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void button_task(void *p) {
    const uint8_t pins[5] = {2, 3, 4, 5, 6};
    bool prev[5] = {false}, enabled = false;

    for (int i = 0; i < 5; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }

    while (1) {
        CHECK_CONTROL_STATE(enabled);
        if (!enabled) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        for (int i = 0; i < 5; i++) {
            bool pressed = !gpio_get(pins[i]);
            if (pressed != prev[i]) {
                adc_t d = { .axis = 4 + i, .val = pressed ? 1 : 0 };
                xQueueSend(xQueueADC, &d, portMAX_DELAY);
                prev[i] = pressed;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void joystick_task(void *p) {
    const uint8_t pins[4]     = {10, 11, 12, 13};
    const uint8_t axis_map[4] = {11, 9, 12, 10};
    bool prev[4] = {false}, enabled = false;

    for (int i = 0; i < 4; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }

    while (1) {
        CHECK_CONTROL_STATE(enabled);
        if (!enabled) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        for (int i = 0; i < 4; i++) {
            bool pressed = !gpio_get(pins[i]);
            if (pressed != prev[i]) {
                adc_t d = { .axis = axis_map[i], .val = pressed ? 1 : 0 };
                xQueueSend(xQueueADC, &d, portMAX_DELAY);
                prev[i] = pressed;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void uart_task(void *p) {
    adc_t data;
    bool enabled = false;

    while (1) {
        CHECK_CONTROL_STATE(enabled);
        if (!enabled) {
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
    xQueueADC     = xQueueCreate(32, sizeof(adc_t));
    xQueueControl = xQueueCreate(1, sizeof(bool));

    xTaskCreate(switch_task,    "SWITCH",   1024, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(x_task,         "XAXIS",    4096, NULL, 1, NULL);
    xTaskCreate(y_task,         "YAXIS",    4096, NULL, 1, NULL);
    xTaskCreate(button_task,    "BUTTON",   2048, NULL, 1, NULL);
    xTaskCreate(joystick_task,  "JOYSTICK", 2048, NULL, 1, NULL);
    xTaskCreate(uart_task,      "UART",     4096, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1) {}
    return 0;
}
