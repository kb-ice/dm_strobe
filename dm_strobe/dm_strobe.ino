/*
    dm_strobe
    Copyright (c) kb-ice. All rights reserved.
    Licensed under the MIT License. See ../LICENSE for license information.
*/

#include <Arduino.h>
#include <wiring_private.h>
#include <util/delay.h>

#define TIMER1_CLK_MASK 0b11111000      //Timer1の分周比選択に関わるビットが0
#define TIMER1_CLK_DIV8 0b00000010      //Timer1を8分周で開始

#define LED_PERIOD      10      //LEDを制御するパルスの幅
#define DEFAULT_PERIOD  2000    //モーターを回転させないときのPWMの周期

#define DDR_LEDOUT  DDRB    //LED制御出力ピン(PB4,OC1B)
#define NUM_LEDOUT  4
#define PORT_BTNIN  PORTD   //ボタン入力ピン(PD2)
#define PIN_BTNIN   PIND
#define NUM_BTNIN   2
#define PORT_MTROUT PORTD   //モーター制御出力ピン(PD3)
#define DDR_MTROUT  DDRD
#define NUM_MTROUT  3
#define DDR_DIVOUT  DDRD    //2分周器出力ピン(PD5,OC0B)
#define NUM_DIVOUT  5

volatile uint32_t clk_count = 0;    //Timer1のクロック回数
volatile uint8_t pulse_count = 0;   //LEDが光った回数
volatile uint16_t period = 0;       //Timer1の前回の周期


int main() {
                                    //PB4 (OC1B)...LED制御出力
                                    //PD6 (ICP) ...矩形波入力
    sbi(PORT_BTNIN, NUM_BTNIN);     //PB2       ...ボタン入力(プルアップ)
    sbi(DDR_MTROUT, NUM_MTROUT);    //PD3       ...モーター制御出力
                                    //PD4 (T0)  ...信号入力
    sbi(DDR_DIVOUT, NUM_DIVOUT);    //PD5 (OC0B)...2分周器出力

    TCCR1A = 0b00100011;  //OC1B非反転出力
    TCCR1B = 0b01011000;  //上昇端キャプチャ，高速PWM(TOP=OCR1A)
    TIMSK  = 0b10101000;  //キャプチャ割り込み，OCR1B割り込み，オーバーフロー割り込み許可
    OCR1A = DEFAULT_PERIOD;
    OCR1B = LED_PERIOD;

    TCCR0A = 0b00100011;  //OC0B非反転出力，高速PWM(TOP=OCR0A)
    TCCR0B = 0b00001111;  //T0ピンの上昇端
    OCR0A = 1;
    OCR0B = 0;

    
    _delay_ms(500);
    while(1) {
        while(PIN_BTNIN & _BV(NUM_BTNIN));      //ボタンが押されるまで待つ

        //モーターを回さずにLEDだけ光らせるパート
        OCR1A = DEFAULT_PERIOD;
        sbi(DDR_LEDOUT, NUM_LEDOUT);            //LED出力を有効化
        TCCR1B |= TIMER1_CLK_DIV8;              //Timer1開始


        while(!(PIN_BTNIN & _BV(NUM_BTNIN)));   //ボタンが放されるまで待つ
        _delay_ms(100);                         //チャタリング防止
        while(PIN_BTNIN & _BV(NUM_BTNIN));      //ボタンが押されるまで待つ

        //モーターを回すパート
        sbi(PORT_MTROUT, NUM_MTROUT);           //モーター起動
        _delay_ms(500);
        TCCR1B &= TIMER1_CLK_MASK;              //Timer1停止
        cbi(DDR_LEDOUT, NUM_LEDOUT);            //LED出力を無効化
        sei();                                  //周波数逓倍開始


        while(!(PIN_BTNIN & _BV(NUM_BTNIN)));   //ボタンが放されるまで待つ
        _delay_ms(100);                         //チャタリング防止
        while(PIN_BTNIN & _BV(NUM_BTNIN));      //ボタンが押されるまで待つ

        //モーター停止・LED消灯処理
        cli();                                  //周波数逓倍終了
        TCCR1B &= TIMER1_CLK_MASK;              //Timer1停止
        cbi(DDR_LEDOUT, NUM_LEDOUT);            //LED出力を無効化
        cbi(PORT_MTROUT, NUM_MTROUT);           //モーター停止

        while(!(PIN_BTNIN & _BV(NUM_BTNIN)));   //ボタンが放されるまで待つ
        _delay_ms(100);                         //チャタリング防止
    }

    return 0;
}


ISR(TIMER1_OVF_vect) {
    if(clk_count == 0) {
        clk_count = 1;                  //初回はオーバーフローする1クロック分
    } else {
        clk_count += period + 1UL;      //Timer1の1周期にかかったクロック数
    }

    period = OCR1A;
}

ISR(TIMER1_COMPB_vect) {
    //pulse_count = 0,1,2,3,4,5,6,7,(8,...), 0,1,...

    if(pulse_count == 6) {
        OCR1A = 0xFFFF;                 //次回以降のTimer1の周期を最大に
    } else if(pulse_count == 7) {
        cbi(DDR_LEDOUT, NUM_LEDOUT);    //LED出力を無効化
    }

    pulse_count++;
}

ISR(TIMER1_CAPT_vect) {
    TCCR1B &= TIMER1_CLK_MASK;      //Timer1停止

    TCNT1 = 0xFFFF;                 //カウンタ値をオーバーフロー直前の値に設定

    clk_count += ICR1;              //clk_count...信号の入力間隔
    OCR1A = clk_count >> 3;         //clk_count / 8
    if(OCR1A <= LED_PERIOD) {
        OCR1A = LED_PERIOD + 1;     //周期が小さすぎる場合修正
    }

    clk_count = 0;                  //リセット
    pulse_count = 0;                //リセット
    sbi(DDR_LEDOUT, NUM_LEDOUT);    //LED出力を有効化

    TCCR1B |= TIMER1_CLK_DIV8;      //Timer1開始
}
