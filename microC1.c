/*
// All code in this file written by: Ju Yong Michael Yoon
// 
// Whack-a-Mole
// Microcontroller #1 ATMega1284
//
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include "usart.h"
#include "timer.h"
#include "io.c"

// ------- Global Function Declarations ------
unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b);
unsigned char GetBit(unsigned char x, unsigned char k);
void updateScore(unsigned char);
void setCustomCharacters();

// ------ Global Variable Declarations ------
unsigned long playTime;
unsigned char currScore;
unsigned char tempMessage;
unsigned char currHighScore;

// ----------------- Tick Function States ----------------
enum gameStatuses{intro, highscore, resetHighScore, play, gameOver} gameStatus;
enum soundStatuses{none, normal, special, bad} soundStatus;

//-------- Tick Functions -----------
void lcdTick();
void mainTick();
void playSound();
void displayIntro();

int main(void)
{
	//---- PORT Initializations -----
	DDRB = 0xFF; PORTB = 0x00;
	DDRA = 0x00; PORTA = 0xFF;
	DDRC = 0xFF; PORTC = 0x00; //used for LCD display
	DDRD = 0xFF; PORTD = 0x00; //used for LCD display
	
	// --- Function Initializations ---
	TimerSet(5);
	TimerOn();
	PWM_on();
	initUSART();
	setCustomCharacters();
	LCD_init();

	//---if eeprom address was not initialized----
	if(eeprom_read_byte((uint8_t*)46) == 0xFF)
		eeprom_write_byte((uint8_t*)46 , 0);
		
	//----load old high score saved in EEPROM----
	currHighScore = eeprom_read_byte((uint8_t*)46);
	
	gameStatus = 0;
	soundStatus = 0;
	lcdTick();
	
	while(1){	
		mainTick();
		playSound();
		while(!TimerFlag);
		TimerFlag = 0;
	}
}

unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
	return (b ? x | (0x01 << k) : x & ~(0x01 << k));
}
unsigned char GetBit(unsigned char x, unsigned char k) {
	return ((x & (0x01 << k)) != 0);
}

void mainTick(){
	static unsigned char button;
	static unsigned char pressed; //decrease states for buttonpress
	button = ~PINA;
	switch(gameStatus){
		case intro:
			if(GetBit(button,0)){
				gameStatus = play;
				playTime = 0;
				pressed = 1;
				currScore = 0;
				lcdTick();
				if(USART_IsSendReady())
					USART_Send(0x41);
			}
			else if(USART_HasReceived()){
				tempMessage = USART_Receive();
				if( tempMessage == 0xC1 ){
					gameStatus = highscore;
					playTime = 0;
					lcdTick();
				}
				else if( tempMessage == 0xC2){
					gameStatus = resetHighScore;
					currHighScore = 0;
					eeprom_write_byte ((uint8_t*) 46, currHighScore);
					playTime = 0;
					lcdTick();
				}
			}
			else
				gameStatus = intro;
			break;
		case highscore:
			if(playTime == 500){
				gameStatus = intro;
				lcdTick();
			}
			else
				gameStatus = highscore;
			break;
		case resetHighScore:
			if(playTime == 500){
				gameStatus = intro;
				lcdTick();
			}
			else
				gameStatus = resetHighScore;
			break;
		case play:
			if(!(GetBit(button,0) && !pressed) && playTime < 4000 )
				gameStatus = play;
			else if((GetBit(button,0) && !pressed) || playTime == 4000){
				gameStatus = gameOver;
				lcdTick();
				playTime = 0;
				if(currScore > currHighScore){
					currHighScore = currScore;
					eeprom_write_byte ((uint8_t*) 46, currHighScore);
				}
				if(USART_IsSendReady())
				USART_Send(0x40);
			}
			else
				gameStatus = play;
			break;
		case gameOver:
			if(playTime == 500){
				gameStatus = intro;
				lcdTick();
			}
			else if(playTime < 500)
				gameStatus = gameOver;
			else
				gameStatus = gameOver;
			break;
	}
	switch(gameStatus){
		case intro:
			PORTB = 0;
			currScore = 0;
			break;
		case highscore:
			playTime++;
			break;
		case resetHighScore:
			playTime++;
			break;
		case play:
			if(!GetBit(button,0))
				pressed = 0;
			playTime++;
			if(USART_HasReceived()){
				tempMessage = USART_Receive();
				if(GetBit(tempMessage,7) && !GetBit(tempMessage,6))
				if((tempMessage & 0x3F) != currScore)
					updateScore(tempMessage & 0x3F);
			}
			break;
		case gameOver:
			playTime++;
			break;
	}
}

void lcdTick(){
	LCD_ClearScreen();
	switch(gameStatus){
		case intro:
			LCD_DisplayString(4," WHACKAMOLE!");
			displayIntro();
			break;
		case highscore:
			LCD_DisplayString(3, " HIGHSCORE: " );
			LCD_Cursor(24);
			if(currHighScore > 9){
				unsigned char temp = currHighScore/10;
				LCD_WriteData(temp+'0');
				temp = currHighScore % 10;
				LCD_WriteData(temp+'0');
			}
			else
				LCD_WriteData(currHighScore + '0');
			break;
		case resetHighScore:
			LCD_DisplayString(3, " RESETTING    ...HIGHSCORE... " );
			break;
		case play:
			LCD_DisplayString(3, " HIT MOLES!");
			LCD_DisplayString(17, " Score: ");
			LCD_Cursor(30);
			LCD_WriteData(0 +'0');
			break;
		case gameOver:
			LCD_DisplayString(3, " GAME OVER!");
			LCD_DisplayString(17, " YOUR SCORE: ");
			LCD_Cursor(30);
			if(currScore > 9){
				unsigned char temp = currScore/10;
				LCD_WriteData(temp+'0');
				temp = currScore % 10;
				LCD_WriteData(temp+'0');
			}
			else
			LCD_WriteData(currScore + '0');
			break;
	}
	LCD_Cursor(40);
}

void updateScore(unsigned char newScore){
	if(newScore == currScore+1)
		soundStatus = normal;
	else if(newScore == currScore+2)
		soundStatus = special;
	else if(newScore == currScore-2)
		soundStatus = bad;
	currScore = newScore;
	LCD_Cursor(30);
	if(currScore > 9){
		unsigned char temp = currScore/10;
		LCD_WriteData(temp+'0');
		temp = currScore % 10;
		LCD_WriteData(temp+'0');
	}
	else
	LCD_WriteData(currScore + '0');
	
	LCD_Cursor(40);
}

void playSound(){
	static double currFreq;
	static unsigned char soundTime = 0;
	
	soundTime++;
	if(soundTime == 10){
		soundTime = 0;
		switch(soundStatus){
			case none:
				currFreq = 0;
				set_PWM(currFreq);
				break;
			case normal:
				if(currFreq == 0){
					currFreq = 261.63;
					set_PWM(currFreq);
				}
				else if(currFreq == 261.63){
					currFreq = 293.66;
					set_PWM(currFreq);
				}
				else if(currFreq == 293.66){
					currFreq = 329.63;
					set_PWM(currFreq);
				}
				else if(currFreq == 329.63){
					currFreq = 0;
					set_PWM(currFreq);
					soundStatus = none;
				}
				break;
			case special:
				if(currFreq == 0){
					currFreq = 261.63;
					set_PWM(currFreq);
				}
				else if(currFreq == 261.63){
					currFreq = 293.66;
					set_PWM(currFreq);
				}
				else if(currFreq == 293.66){
					currFreq = 329.63;
					set_PWM(currFreq);
				}
				else if(currFreq == 329.63){
					currFreq = 0;
					set_PWM(currFreq);
					soundStatus = none;
				}
				break;
			case bad:
				if(currFreq == 0){
					currFreq = 261.63;
					set_PWM(currFreq);
				}
				else if(currFreq == 261.63){
					currFreq = 293.66;
					set_PWM(currFreq);
				}
				else if(currFreq == 293.66){
					currFreq = 329.63;
					set_PWM(currFreq);
				}
				else if(currFreq == 329.63){
					currFreq = 0;
					set_PWM(currFreq);
					soundStatus = none;
				}
				break;
		}
	}
}


//==== Functions to setup the LCD Display ====
//====		Must be hardcoded			=====
void setCustomCharacters(){
	LCD_WriteCommand(0x40);

	//bottom hammer - 0
	LCD_WriteData(0xe);
	LCD_WriteData(0xe);
	LCD_WriteData(0xe);
	LCD_WriteData(0xe);
	LCD_WriteData(0xe);
	LCD_WriteData(0x4);
	LCD_WriteData(0x0);
	LCD_WriteData(0x1f);

	//hammer - middle - 1
	LCD_WriteData(0x0);
	LCD_WriteData(0xe);
	LCD_WriteData(0x1f);
	LCD_WriteData(0x1f);
	LCD_WriteData(0x1f);
	LCD_WriteData(0x1f);
	LCD_WriteData(0x1f);
	LCD_WriteData(0xe);

	//hammer - right - 2
	LCD_WriteData(0x0);
	LCD_WriteData(0x2);
	LCD_WriteData(0x18);
	LCD_WriteData(0x1c);
	LCD_WriteData(0x1d);
	LCD_WriteData(0x1c);
	LCD_WriteData(0x18);
	LCD_WriteData(0x2);

	//hammer - left - 3
	LCD_WriteData(0x0);
	LCD_WriteData(0x8);
	LCD_WriteData(0x3);
	LCD_WriteData(0x7);
	LCD_WriteData(0x17);
	LCD_WriteData(0x7);
	LCD_WriteData(0x3);
	LCD_WriteData(0x8);

	//hammer - grass - 4
	LCD_WriteData(0x0);
	LCD_WriteData(0x0);
	LCD_WriteData(0x0);
	LCD_WriteData(0x0);
	LCD_WriteData(0x0);
	LCD_WriteData(0xa);
	LCD_WriteData(0x1f);
	LCD_WriteData(0x1f);

	//hammer - mole - 5
	LCD_WriteData(0x4);
	LCD_WriteData(0x11);
	LCD_WriteData(0x6);
	LCD_WriteData(0xf);
	LCD_WriteData(0xb);
	LCD_WriteData(0x1f);
	LCD_WriteData(0x1f);
	LCD_WriteData(0x1f);
}

void displayIntro(){
	LCD_Cursor(1);
	LCD_WriteData(3); //left hammer
	LCD_Cursor(2);
	LCD_WriteData(1);
	LCD_Cursor(3);
	LCD_WriteData(2);
	LCD_Cursor(17);
	LCD_WriteData(4);
	LCD_Cursor(18);
	LCD_WriteData(0);
	LCD_Cursor(19);
	LCD_WriteData(4);
	LCD_Cursor(20);
	LCD_WriteData(4);
	LCD_Cursor(21);
	LCD_WriteData(5);
	LCD_Cursor(22);
	LCD_WriteData(4);
	LCD_Cursor(23);
	LCD_WriteData(5);
	LCD_Cursor(24);
	LCD_WriteData(4);
	LCD_Cursor(25);
	LCD_WriteData(4);
	LCD_Cursor(26);
	LCD_WriteData(4);
	LCD_Cursor(27);
	LCD_WriteData(5);
	LCD_Cursor(28);
	LCD_WriteData(4);
	LCD_Cursor(29);
	LCD_WriteData(4);
	LCD_Cursor(30);
	LCD_WriteData(5);
	LCD_Cursor(31);
	LCD_WriteData(4);
	LCD_Cursor(32);
	LCD_WriteData(4);
}