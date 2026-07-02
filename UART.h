#ifndef UART_H
#define UART_H

#include <stdint.h>

// Initialize UART module (UCA0)
void uart_init(void);

// Send a single byte over UART
void uart_send_byte(uint8_t byte);

// Send a null-terminated string over UART
void uart_send_string(const char* str);

#endif // UART_H