/********************************************************************************
Author: Marco Antônio Habitzreuter (mahabitzreuter@gmail.com)
Date: ago 29 2015

                       -------O------
               +5V---01|VCC        GND|20---0V
           CRISTAL---02|RA5        RA0|19---GATE3
           CRISTAl---03|RA4        RA1|18---SOBRECORRENTE
              MCLR---04|RA3        RA2|17---INT
             GATE1---05|RC5        RC0|16---LCD_D4
             GATE2---06|RC4        RC1|15---LCD_D5
            LCD_D7---07|RC3        RC2|14---LCD_D6
            LCD_RS---08|RC6        RB4|13---BOTOES
            LCD_EN---09|RC7        RB5|12---SERIAL
            SERIAL---10|RB7        RB6|11---BYPASS
                       --------------

 ********************************************************************************/
/********************************************************************************
 Included files
 ********************************************************************************/
#include <htc.h>
#include "lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/********************************************************************************
 PIC configuration
 ********************************************************************************/
__CONFIG(FOSC_HS & WDTE_OFF & PWRTE_ON & MCLRE_ON);
#define _XTAL_FREQ 2000000
/********************************************************************************
 Defines
 ********************************************************************************/
#define GATE1 RC5
#define GATE2 RC4
#define GATE3 RA0
#define BYPASS RB6
#define MAX 150
#define MIN 40
#define COMMAND_LENGHT 6
/********************************************************************************
 Global variables
 ********************************************************************************/
char time_up = 20;
char time_down = 20;
//lcd
char mostra = 0; // controla escrita
bit select = 0; // 0: rampa de subida; 1: rampa de descida
//disparo
bit process = 0; //0: rampa de subida; 1: rampa de descida
bit cicle = 0; //enable pos reset;
//sobrecorrente
bit i_flag = 0; // sinaliza sobrecorrente
int trigger = MAX; // posição do pulso de disparo em graus
//deslocamento do trigger
char time_count = 0;
char time_compare = 0;
int pos = 0; // posição da varredura;
//botões
bit bt_busy = 0; // evita repique no AD
//comunicação e comando serial
char command[COMMAND_LENGHT];
char *rx_p;
bit rx_flag = 0;
/********************************************************************************
 Function prototypes
 ********************************************************************************/
void interrupt interr();
void disableRamp();
void enableRamp();
void buttonTest();
void currentTest();
void command_clear();

/********************************************************************************
 Main function
 ********************************************************************************/
void main() {
    ANSEL = ANSELH = 0;
    TRISA = 0b00000110;
    TRISB = 0b00110000;
    TRISC = 0b00110000;
    nRABPU = WPUA0 = 0;
    PORTA = PORTB = PORTC = 0;
    //LCD
    lcd_init();
    __delay_ms(30); // LCD needs this to work ok
    mostra = 1;
    //INT
    INTCON = 0b11010000;
    INTEDG = 1;
    //TMR2
    PR2 = 28;
    T2CON = 0b00111000;
    TMR2IE = 1;
    TMR2IF = 0;
    //AD
    ANSEL = 0b00000010;
    ANSELH = 0b00000100;
    ADCON0 = 0b00101001; // AN0, ADON
    ADCON1 = 0b00100000; // FOSC/32
    WPUB4 = 0;
    //SERIAL 10417bps
    SPBRG = 119;
    RCIE = 1;
    RCSTA = 0b10010000;
    TXSTA = 0b00100100;
    command_clear();
    __delay_ms(1000);
    while (1) {
        if (rx_flag) {
            char *p;
            p = command;
            while (*p != '*') p++;
            *p = '\0';
            time_up = atoi(command);
            p++;
            char *p1;
            p1 = p;
            while (*p1 != '/') p1++;
            *p1 = '\0';
            time_down = atoi(p);
            BYPASS = 0;
            enableRamp();
            mostra = 1;
            rx_flag = 0;
        }
        /*if (BYTE_RECEBIDO != 0) {
            printf("%c", BYTE_RECEBIDO);
            while (!TRMT) {}
            TXREG = BYTE_RECEBIDO;
            BYTE_RECEBIDO = 0;
        }*/
        /*GO = 1;
        __delay_ms(30);
        lcd_clear();
        __delay_ms(50);
        printf("%d", ADRESH);
        __delay_ms(1000);*/
        if (!INTE) __delay_us(400), INTE = 1;
        if (mostra) {
            lcd_clear();
            __delay_ms(50);
            if (TMR2ON) {
                if (!process) printf("Acelerando");
                else printf("Parando");
            } else {
                if (!select) printf("SUBIDA\n%d", time_up);
                else printf("DESCIDA\n%d", time_down);
            }
            mostra = 0;
        }
        if ((!TMR2ON && !BYPASS) || BYPASS) { //parado ou velocidade total
            buttonTest();
        }
        if ((TMR2ON && !BYPASS) || BYPASS) { //acelerando ou velocidade total
            currentTest();
        }
    }
}

void interrupt interr() {
    if (INTF) {
        INTE = 0;
        INTF = 0;
        cicle = !cicle;
        if (cicle) pos = 0;
        if (!process) {
            if (trigger == MIN) {
                disableRamp();
                BYPASS = 1;
                process = 1;
                mostra = 1;
            };
        } else {
            if (trigger == MAX) {
                disableRamp();
                process = 0;
                mostra = 1;
            }
        }
        __delay_us(600);
    } else if (TMR2IF) { //controle do disparo e da posição do pulso
        pos++;
        if (pos == trigger - 120) GATE3 = 1, __delay_us(100);
        else if (pos == trigger - 60) GATE2 = 1, __delay_us(100);
        else if (pos == trigger) GATE1 = 1, __delay_us(100);
        else if (pos == trigger + 60) GATE3 = 1, __delay_us(100);
        else if (pos == trigger + 120) GATE2 = 1, __delay_us(100);
        else if (pos == trigger + 180) GATE1 = 1, __delay_us(100);
        else if (pos == trigger + 240) GATE3 = 1, __delay_us(100);
        else if (pos == trigger + 300) GATE2 = 1, __delay_us(100);
        else if (pos == trigger + 360) GATE1 = 1, __delay_us(100);
        GATE1 = GATE2 = GATE3 = TMR2IF = 0;
        if (++time_count == 240) {// após (1/90)s
            time_count = 0;
            if (!process) {
                if (++time_compare == time_up) time_compare = 0, trigger -= 2;
            } else {
                if (++time_compare == time_down) time_compare = 0, trigger += 2;
            }
        }
    } else if (RCIF && !TMR2ON && RCIE) {
        char lixo;
        while (RCIF)
            if (FERR == 1 || OERR == 1) {
                SPEN = 0;
                SPEN = 1;
                lixo = RCREG;
            } else {
                *rx_p = RCREG;
                if (*rx_p == '/') {
                    rx_flag = 1;
                } else {
                    rx_p++;
                    *rx_p = '\0';
                }

            }
    }
}

void disableRamp() {
    RC5 = RC4 = RA0 = TMR2ON = 0;
    TRISC5 = TRISC4 = TRISA0 = 1;

}

void enableRamp() {
    //configurações dos gates e pulsos
    RC5 = RC4 = RA0 = TRISC5 = TRISC4 = TRISA0 = 0;
    TMR2ON = 1;
}

void buttonTest() {
    ADCON0 = 0b00101001; //seleciona AN10
    __delay_ms(10);
    GO = 1;
    __delay_ms(10);
    if (ADIF) {
        if (ADRESH > 240 && !bt_busy) { //--
            bt_busy = mostra = 1;
            if (!select) {
                if (--time_up < 5) time_up = 50;
            } else {
                if (--time_down < 5) time_down = 50;
            }
        } else if (ADRESH > 180 && ADRESH < 200 && !bt_busy) { //select
            bt_busy = mostra = 1;
            select = !select;
        } else if (ADRESH > 115 && ADRESH < 135 && !bt_busy) { //start
            bt_busy = mostra = 1;
            BYPASS = 0;
            enableRamp();
        } else if (ADRESH > 55 && ADRESH < 75 && !bt_busy) { //++
            bt_busy = mostra = 1;
            if (!select) {
                if (++time_up > 50) time_up = 5;
            } else {
                if (++time_down > 50) time_down = 5;
            }
        } else if (ADRESH < 50) {
            bt_busy = 0;
        }
        ADIF = 0;
    }
}

void currentTest() {
    ADCON0 = 0b00000101; //seleciona AN10
    __delay_ms(10);
    GO = 1;
    __delay_ms(10);
    if (ADIF) {
        if (ADRESH > 0b10000000) { // 200%
            disableRamp();
            BYPASS = 0;
            process = !process;
        } else if (ADRESH > 0b01100100 && i_flag == 0) { // 150
            i_flag = 1; // sinaliza overflow
            process = !process; // inverte rampa
        } else if (i_flag == 1 && ADRESH < 0b01100100) { //rampa foi invertida e corrente diminuiu p/ menos de 150%
            process = !process; // inverte rampa para estado original
            i_flag = 0;
        }
        ADIF = 0;
    }
}

void command_clear() {
    for (char a = 0; a < COMMAND_LENGHT; a++) command[a] = 0;
    rx_p = command;
}
