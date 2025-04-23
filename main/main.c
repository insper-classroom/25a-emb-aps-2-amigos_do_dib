#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/adc.h"

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
        adc_select_input(0); // Canal para eixo X, ex: GPIO26
        int valor_adc = adc_read();
        buffer[index] = valor_adc;
        index = (index + 1) % 5;
        int media = media_movel(buffer);
        int transformacao = aplicar_transformacao(media);
        data.axis = 0;
        data.val = transformacao;
        if (data.val != 0){
             xQueueSend(xQueueADC, &data, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void y_task(void *p) {
    adc_init();
    adc_gpio_init(27);
    int buffer[5] = {0};
    int index = 0;
    adc_t data;
    while (1) {
        adc_select_input(1); // Canal para eixo X, ex: GPIO26
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
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void x2_task(void *p) {
    adc_init();
    adc_gpio_init(28);
    int buffer[5] = {0};
    int index = 0;
    adc_t data;
    while (1) {
        adc_select_input(2); // Canal para eixo X, ex: GPIO26
        int valor_adc = adc_read();
        buffer[index] = valor_adc;
        index = (index + 1) % 5;
        int media = media_movel(buffer);
        int transformacao = aplicar_transformacao(media);
        data.axis = 0;
        data.val = transformacao;
        if (data.val != 0){
             xQueueSend(xQueueADC, &data, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void y2_task(void *p) {
    adc_init();
    adc_gpio_init(27);
    int buffer[5] = {0};
    int index = 0;
    adc_t data;
    while (1) {
        adc_select_input(3); // Canal para eixo X, ex: GPIO26
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
        vTaskDelay(pdMS_TO_TICKS(200));
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
           //printf("%d %d" , mais, menos);
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



    xTaskCreate(x_task, "foo task", 4095, NULL, 1, NULL);
    xTaskCreate(y_task, "foo task", 4095, NULL, 1, NULL);
    xTaskCreate(uart_task, "foo task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}