#pragma once

#include "driver/uart.h"

#define HTTP_PORT 80
#define AP_SSID "Spritschleuder-Setup"
#define AP_PASSWORD "cocktail"
#define WIFI_MAXIMUM_RETRY 8

// Arduino Kommunikation
#define UART_PORT_NUM   UART_NUM_1
#define UART_TX_PIN     6
#define UART_RX_PIN     7
#define UART_BAUD_RATE  115200

#define LCD_DC_PIN 0
#define LCD_CS_PIN 2
#define LCD_SCK_PIN 3
#define LCD_MOSI_PIN 4
#define LCD_RST_PIN 5
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 128
#define LCD_X_OFFSET 2
#define LCD_Y_OFFSET 3
#define LCD_SPI_HOST SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define ABORT_BUTTON_PIN 8
#define INFO_BUTTON_PIN 10

// Maximale Anzahl Cocktails
#define MAX_COCKTAILS   20
