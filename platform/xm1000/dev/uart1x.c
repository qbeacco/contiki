/*
 * Copyright (c) 2010, Swedish Institute of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)$Id: uart1x.c,v 1.1 2010/08/24 16:23:20 joxe Exp $
 */

/*
 * Machine dependent MSP430X UART1 code.
 */
#include <stdlib.h>

#include "contiki.h"
#ifdef __IAR_SYSTEMS_ICC__
#include <msp430.h>
#else
#include <msp430.h>
#include <io.h>
#include <signal.h>
#endif

#include "contiki.h"
#include "sys/energest.h"
#include "dev/uart1.h"
#include "dev/watchdog.h"
#include "lib/ringbuf.h"
#include "isr_compat.h"
#include "dev/leds.h"
static int (*uart1_input_handler)(unsigned char c);

static volatile uint8_t transmitting;

#ifdef UART1_CONF_TX_WITH_INTERRUPT
#define TX_WITH_INTERRUPT UART1_CONF_TX_WITH_INTERRUPT
#else /* UART1_CONF_TX_WITH_INTERRUPT */
#define TX_WITH_INTERRUPT 0
#endif /* UART1_CONF_TX_WITH_INTERRUPT */

#if TX_WITH_INTERRUPT
#define TXBUFSIZE 64

static struct ringbuf txbuf;
static uint8_t txbuf_data[TXBUFSIZE];
#endif /* TX_WITH_INTERRUPT */

/*---------------------------------------------------------------------------*/
uint8_t
uart1_active(void)
{
  return (UCA1STAT & UCBUSY) | transmitting;
}
/*---------------------------------------------------------------------------*/
void
uart1_set_input(int (*input)(unsigned char c))
{
  uart1_input_handler = input;
}
/*---------------------------------------------------------------------------*/
void
uart1_writeb(unsigned char c)
{
  watchdog_periodic();
  
#if TX_WITH_INTERRUPT
  /* Put the outgoing byte on the transmission buffer. If the buffer
     is full, we just keep on trying to put the byte into the buffer
     until it is possible to put it there. */
  leds_on(LEDS_RED);
  while(ringbuf_put(&txbuf, c) == 0);

  /* If there is no transmission going, we need to start it by putting
     the first byte into the UART. */
  if(transmitting == 0) {
    transmitting = 1;
    UCA1TXBUF = ringbuf_get(&txbuf);	
    leds_on(LEDS_BLUE);
  }

#else /* TX_WITH_INTERRUPT */
  
  /* Loop until the transmission buffer is available. */
  /*Enric while((IFG2 & UCA0TXIFG) == 0); */
  while((UCA1STAT & UCBUSY));

  /* Transmit the data. */
  UCA1TXBUF = c;
#endif /* TX_WITH_INTERRUPT */
}
/*---------------------------------------------------------------------------*/
#if ! WITH_UIP /* If WITH_UIP is defined, putchar() is defined by the SLIP driver */
#endif /* ! WITH_UIP */
/*---------------------------------------------------------------------------*/
/**
 * Initalize the RS232 port.
 *
 */
void
uart1_init(unsigned long ubr)
{

  /* RS232 */
  UCA1CTL0 = 0x00;
  UCA1CTL1 |= UCSWRST;            /* Hold peripheral in reset state */
  UCA1CTL1 |= UCSSEL_2;           /* CLK = SMCLK */
  UCA1BR0 = 0x45;                 /* 8MHz/115200 = 69 = 0x45 */
  UCA1BR1 = 0x00;
  UCA1MCTL = UCBRS2;             /* Modulation UCBRSx = 3 */

  P3DIR &= ~0x80;			/* Select P37 for input (UART1RX) */
  P3DIR |= 0x40;			/* Select P36 for output (UART1TX) */
  P3SEL |= 0xC0;			/* Select P36,P37 for UART1{TX,RX} */
					// UART ASYNC MODE, 8N1, LSB
  //UCA1CTL1 &= ~UCSWRST;       /* Initialize USCI state machine */

  transmitting = 0;


 
  /* XXX Clear pending interrupts before enable */
  IFG2 &= ~UCA1RXIFG;
  IFG2 &= ~UCA1TXIFG;
  UCA1CTL1 &= ~UCSWRST;                   /* Initialize USCI state machine **before** enabling interrupts */
  IE2 |= UCA1RXIE;                        /* Enable UCA1 RX interrupt */
  /* Enable USCI_A1 TX interrupts (if TX_WITH_INTERRUPT enabled) */
#if TX_WITH_INTERRUPT
  ringbuf_init(&txbuf, txbuf_data, sizeof(txbuf_data));
  IE2 |= UCA1TXIE;                        /* Enable UCA1 TX interrupt */
#endif /* TX_WITH_INTERRUPT */



}
#ifdef __IAR_SYSTEMS_ICC__
#pragma vector=USCIAB1RX_VECTOR
__interrupt void
#else
interrupt(USCIAB1RX_VECTOR)
#endif
uart1_rx_interrupt(void)
{
  uint8_t c;

  ENERGEST_ON(ENERGEST_TYPE_IRQ);
  leds_toggle(LEDS_RED);
  if(UCA1STAT & UCRXERR) {
    c = UCA1RXBUF;   /* Clear error flags by forcing a dummy read. */
  } else {
    c = UCA1RXBUF;
    if(uart1_input_handler != NULL) {
      if(uart1_input_handler(c)) {
	LPM4_EXIT;
      }
    }
  }
  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
/*---------------------------------------------------------------------------*/
#if TX_WITH_INTERRUPT
#ifdef __IAR_SYSTEMS_ICC__
#pragma vector=USCIAB0TX_VECTOR
__interrupt void
#else
interrupt(USCIAB1TX_VECTOR)
#endif
uart1_tx_interrupt(void)
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);
  if((IFG2 & UCA1TXIFG)){

    if(ringbuf_elements(&txbuf) == 0) {
      transmitting = 0;
    } else {
      UCA1TXBUF = ringbuf_get(&txbuf);
    }
  }

  /* In a stand-alone app won't work without this. Is the UG misleading? */
  IFG2 &= ~UCA1TXIFG;

  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
#endif /* TX_WITH_INTERRUPT */
/*---------------------------------------------------------------------------*/
