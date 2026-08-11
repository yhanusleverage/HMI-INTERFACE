#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/**
 * Placa: Guition / JCZN JC3248W535 (3.5" AXS15231B)
 * Producto: landscape lógico 480×320 (Canvas rot 1) + dark validado (LCD_IPS=0).
 * Ver docs/DISPLAY_BASELINE.md / docs/HANDOFF.md
 */

#define BOARD_JC3248W535 1

#define PANEL_W 320
#define PANEL_H 480

#define LCD_H_RES 480
#define LCD_V_RES 320
#define LCD_IPS   0
#define LCD_LANDSCAPE 1
#define LCD_CANVAS_ROT 1

/* QSPI → AXS15231B */
#define LCD_PIN_CS   45
#define LCD_PIN_SCK  47
#define LCD_PIN_D0   21
#define LCD_PIN_D1   48
#define LCD_PIN_D2   40
#define LCD_PIN_D3   39
#define LCD_PIN_RST  -1
#define LCD_PIN_BL    1
#define LCD_PIN_PWR  -1
#define LCD_BL_ACTIVE_HIGH 1
#define LCD_ROTATION 0

#ifndef DISP_SELFTEST
#define DISP_SELFTEST 0
#endif

#define TOUCH_I2C_SDA 4
#define TOUCH_I2C_SCL 8
#define TOUCH_I2C_ADDR 0x3B
#define TOUCH_PIN_INT 3
#define TOUCH_PIN_RST 38

#define MASTER_UART_TX 17
#define MASTER_UART_RX 18
#define MASTER_UART_BAUD 115200

#endif
