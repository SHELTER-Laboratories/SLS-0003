///////////////////////////////////////////////////////////////////////////////
//                             UART                                           //
///////////////////////////////////////////////////////////////////////////////
void uart_init(void)
{
    UCA0CTL1 |= UCSWRST;
    UCA0CTL1 |= UCSSEL_2;

    UCA0BR0 = 8;
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS_6 | UCBRF_0;

    P3SEL |= BIT3;
    P3DIR |= BIT3;

    UCA0CTL1 &= ~UCSWRST;
}

void uart_send_byte(unsigned char byte)
{
    while (!(UCA0IFG & UCTXIFG)) { }
    UCA0TXBUF = byte;
}

void uart_send_string(const char* str)
{
    while (*str)
    {
        uart_send_byte(*str++);
    }
}