/*
// All code in this file written by: Ju Yong Michael Yoon
// 
// Whack-a-Mole
// Microcontroller #2 ATMega1284
//
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "usart.h"
#include "timer.h"

// ----------- Global Function Declarations -----------
unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b);
unsigned char GetBit(unsigned char x, unsigned char k);
void highScoreButton();

// -------- global variables -------
unsigned char play; //BOOLEAN '1' = game start
unsigned long gameTime;
unsigned char moleNum;
unsigned char currScore;
unsigned char moleHit;
unsigned char aliveMoles;
unsigned char moleLife[8];
unsigned char killedMoles;
unsigned char uMessage;
unsigned char i;

// ------- Tick Function States --------
enum SM1_state{init1, birthWait, newMole} currState1;
enum SM2_state{init, killWait, decreMole} currState2;

//----- TICK FUNCTIONS ------
void birthMoleTick();
void killMoleTick();




int main(void)
{
	//---- PORT Initializations -----
	DDRB = 0xFF; PORTB = 0x00;
	DDRA = 0x00; PORTA = 0xFF;
	
	// --- Function Initializations ---
	srand(512);
	TimerSet(5);
	TimerOn();
	initUSART();
	USART_Flush();
	
	// --- Variable Initializations ---
	currState1 = 0;
	currState2 = 0;
	currScore = 0;
	play = 0;

	while(1){
		
		if(USART_HasReceived()){
			uMessage = USART_Receive();
			//PORTB = uMessage;
			if(uMessage == 0x41){
				play = 1;
			}
			else if(uMessage == 0x40) //check id '01'
			play = 0;
		}
		if(play){
			if(USART_IsSendReady()){
				uMessage = currScore;
				uMessage = SetBit(uMessage,7,1);
				uMessage = SetBit(uMessage,6,0); //setting id '10'
				USART_Send(uMessage);
			}
		}

		birthMoleTick();
		killMoleTick();
		highScoreButton();
		while(!TimerFlag);
		TimerFlag = 0;
	}

}



// ---------- Global Function Definitions -----------

unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
	return (b ? x | (0x01 << k) : x & ~(0x01 << k));
}

unsigned char GetBit(unsigned char x, unsigned char k) {
	return ((x & (0x01 << k)) != 0);
}

void birthMoleTick(){
	static unsigned short birthTime;
	static unsigned short timeCnt;
	switch(currState1){
		case init1:
			if(play){
				currState1 = birthWait;
				birthTime = (rand()%100)+80; //random range of birth
				timeCnt = 0;
			}
			else
				currState1 = init1;
			break;
		case birthWait:
			if(!play){
				currState1 = init1;
				PORTB = 0;
			}
			else if(birthTime == timeCnt)
				currState1 = newMole;
			else
				currState1 = birthWait;
			break;
		case newMole:
			currState1 = birthWait;
			birthTime = (rand()%80)+30; // random time of birth
			timeCnt = 0;
			break;
		default:
			currState1 = init1;
	}
	switch(currState1){
		case init1:
			aliveMoles = 0;
			killedMoles = 0;
			for(i = 0; i<8 ; i++){
				moleLife[i] = 0;
			}
			break;
		case birthWait:
			timeCnt++;
			PORTB = aliveMoles;
			break;
		case newMole:
			moleNum = rand()%8;
			if(!GetBit(aliveMoles,moleNum)){
				aliveMoles = SetBit(aliveMoles, moleNum, 1);
				moleLife[moleNum] = 120; //MOLE LIFETIME
			}
			break;
		default:
			currState1 = init1;
			break;
	}
}

void killMoleTick(){
	static unsigned char buttonPress;
	buttonPress = ~PINA;
	switch(currState2){
		case init:
			if(play){
				currState2 = killWait;
				currScore = 0;
			}
			else
				currState2 = init;
			break;
		case killWait:
			if(!play){
				currState2 = init;
			}
			else if(buttonPress){
				for(i=0; i<8 ; i++)
				if(GetBit(buttonPress,i) && GetBit(aliveMoles,i)){
					aliveMoles = SetBit(aliveMoles,i,0);
					moleLife[i] = 0;
					currScore++;
				}
				currState2 = decreMole;
			}
			else
				currState2 = killWait;
			break;
		case decreMole:
			currState2 = killWait;
			break;
	}
	switch(currState2){
		case init:
			currScore = 0;
			break;
		case killWait:
			for(i=0 ; i<8 ; i++){
				if(moleLife[i] == 0)
					aliveMoles = SetBit(aliveMoles, i , 0);
				else if(moleLife[i] > 0)
					moleLife[i]--;
			}
			break;
		case decreMole:
			for(i=0 ; i<8 ; i++){
				if(moleLife[i] == 0 && GetBit(aliveMoles,i))
				aliveMoles = SetBit(aliveMoles, i , 0);
				else if(moleLife[i] > 0)
				moleLife[i]--;
			}
			break;
	}

}

void highScoreButton(){
	static unsigned char hsButton;
	static unsigned long hsTime = 1000;
	
	if(currState2 == init){
		hsTime++;
		hsButton = ~PINA;
		if(hsButton && (hsTime > 520)){ //any button pressed
			if(hsButton == 0x02)
				USART_Send(0xC2);
			else	
				USART_Send(0xC1);
			hsTime = 0;
		}
	}
}