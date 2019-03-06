/************************************************************************/
/*																		*/
/*	video_demo.c	--	ZYBO Video demonstration 						*/
/*																		*/
/************************************************************************/
/*	Author: Sam Bobrowicz												*/
/*	Copyright 2015, Digilent Inc.										*/
/************************************************************************/
/*  Module Description: 												*/
/*																		*/
/*		This file contains code for running a demonstration of the		*/
/*		Video input and output capabilities on the ZYBO. It is a good	*/
/*		example of how to properly use the display_ctrl and				*/
/*		video_capture drivers.											*/
/*																		*/
/*																		*/
/************************************************************************/
/*  Revision History:													*/
/* 																		*/
/*		11/25/2015(SamB): Created										*/
/*																		*/
/************************************************************************/

/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */

#include "video_demo.h"		// our structure and prodecure definitions is in here
#include "display_ctrl/display_ctrl.h"
#include "intc/intc.h"
#include <stdio.h>
#include "xuartps.h"
#include "math.h"
#include <ctype.h>
#include <stdlib.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "timer_ps/timer_ps.h"
#include "xil_exception.h"
#include "xparameters.h"
#include "xsdps.h"
#include "BTNInterrupt/interrupts.h"		//intterupts lib
#include "xtime_l.h"
#include "sprites/sprites.h"
#include "ff.h"
#include "platform/platform.h"





/* ------------------------------------------------------------ */
/*				Defines	macro definitions						*/
/* ------------------------------------------------------------ */

/*
 * XPAR redefines
 */
#define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_BASEADDR
#define VGA_VDMA_ID XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID
#define VID_VTC_IRPT_ID XPS_FPGA3_INT_ID
#define VID_GPIO_IRPT_ID XPS_FPGA4_INT_ID
#define SCU_TIMER_ID XPAR_SCUTIMER_DEVICE_ID
#define UART_BASEADDR XPAR_PS7_UART_1_BASEADDR



/*
 * Timer definitons
 */
#define TMR_LOAD				833333 //counts per millisecond
#define INTC_TMR_INTERRUPT_ID 	XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR
#define TMR_DEVICE				XPAR_TMRCTR_0_DEVICE_ID



/* ------------------------------------------------------------ */
/*				Global Variables								*/
/* ------------------------------------------------------------ */


/*
 * Framebuffers for video/vga data out
 */
u8 frameBufA[DEMO_MAX_FRAME];	//front-buffer (read from)
u8 frameBufB[DEMO_MAX_FRAME];	//back-buffer (write to)
u8 *ptr_frameBufA;
u8 *ptr_frameBufB;

Enemy alienNest[30];			//array instance of 30 aliens
Hero hero;						// Hero instance
Bullet bullet;

int opt;
int skew;
int xMove;
int yMove;
int bulletyMove;
int bulletFlag = 0;
int bulletHit = 0;

/*
 * Display and Video Driver structs
 */
DisplayCtrl dispCtrl;
XAxiVdma vdma;
INTC intc;
char fRefresh; //flag used to trigger a refresh of the Menu on video detect

FATFS FS_instance;

int result;
FIL file;

TCHAR *Path = "0:/";

FRESULT Res;
UINT NumBytesRead;
u32 BuffCnt;
const TCHAR* log_file;
char FileName[50];


int main(void)
{
	skew = 0;	//init incrementing value for alien movement
	xMove = 0;
	yMove = 0;

	for(int i = 0; i < 30; i++){
		alienNest[i].status = 0;
	}



	/* Interrupt initialization */
	int status;
	// Initialize Push Buttons
	status = XGpio_Initialize(&BTNInst, BTNS_DEVICE_ID);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Set all buttons direction to inputs
	XGpio_SetDataDirection(&BTNInst, 1, 0xFF);

	status = IntcInitFunction(INTC_DEVICE_ID, &BTNInst);
	if(status != XST_SUCCESS) return XST_FAILURE;


	DemoInitialize();					// init device drivers for screen
	ptr_frameBufB = frameBufB;			//set pointer to back-buffer



	while(1){

		DefineHero(dispCtrl.vMode.width, dispCtrl.vMode.height,skew);
		DrawSprites(1, ptr_frameBufB, dispCtrl.vMode.width , dispCtrl.vMode.height, DEMO_STRIDE);	//draw frame into back-buffer

		DefineAlien(dispCtrl.vMode.width, dispCtrl.vMode.height, xMove, yMove);
		DrawSprites(2, ptr_frameBufB, dispCtrl.vMode.width , dispCtrl.vMode.height, DEMO_STRIDE);

		if(btns == btnShoot){
			moveBullet = fire;
			DefineBullet(dispCtrl.vMode.width, dispCtrl.vMode.height,bulletyMove, skew);
			DrawBullet(ptr_frameBufB, dispCtrl.vMode.width , dispCtrl.vMode.height, DEMO_STRIDE);
			bulletFlag = 1;
		}
		if(bulletFlag == 1){
			DefineBullet(dispCtrl.vMode.width, dispCtrl.vMode.height,bulletyMove, skew);
			DrawBullet(ptr_frameBufB, dispCtrl.vMode.width , dispCtrl.vMode.height, DEMO_STRIDE);
		}

		MoveHero();
		MoveAliens();
		MoveBullet();
		SwapBuf();																			//copy back-buffer into front-buffer
	}
	
	return 0;
}

void MoveAliens(){

	int xMoveL = 600;
	int yMoveL = 200;
	int xMoveMax = 1100;
	int yMoveMax = 60;

	switch(moveAlien){

	case start:
		yMove = 0;
		xMove = 0;
		if(xMove < xMoveMax)
			moveAlien = xIncr;
		else moveAlien = start;
	break;

	case xIncr:
		xMove++;
		if(xMove == xMoveMax)
			moveAlien = moveDown;
		else if( xMove > xMoveL && yMove > yMoveL) // some point on the screen to identify the max steps the aliens can move
			moveAlien = gameOver;
	break;

	case xDecr:
		xMove--;
		if(xMove == yMoveMax)
			moveAlien = moveDown;
	break;

	case moveDown:
		yMove +=70;
		if(xMove == xMoveMax)
			moveAlien = xDecr;
		else if(xMove == yMoveMax)
			moveAlien = xIncr;
		break;
	case gameOver:
			for(int i = 0; i<30; i++){
				if(alienNest[i].status == 0){
					moveAlien = defeat;
				}
			}
			if(moveAlien != defeat)
				moveAlien = victory;


	break;
	case victory:
		DefineVictory(ptr_frameBufB,120,220,10, DEMO_STRIDE);
		if(btns == reset){
			xMove = 0;
			yMove = 0;
			for(int i = 0; i < 30; i++){
				alienNest[i].status = 0;
			}
			moveAlien = start;
		}
	break;

	case defeat:
		DefineGameOver(ptr_frameBufB,120,220,10, DEMO_STRIDE);
		if(btns == reset){
				xMove = 0;
				yMove = 0;
				for(int i = 0; i < 30; i++){
					alienNest[i].status = 0;
				}

				moveAlien = start;
		}


	break;
	}

TimerDelay(100);

}

void DefineGameOver(u8 *frame, u32 xcor, u32 ycor, u32 size, u32 stride){
	int arr[5][56] = {
			{2,2,2,2,0,0,2,2,0,0,2,0,0,0,2,0,2,2,2,0,0,0,0,0,2,2,0,0,2,0,0,0,2,0,2,2,2,0,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
			{2,0,0,0,0,2,0,0,2,0,2,2,0,2,2,0,2,0,0,0,0,0,0,2,0,0,2,0,2,0,0,0,2,0,2,0,0,0,2,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
			{2,0,2,2,0,2,2,2,2,0,2,0,2,0,2,0,2,2,2,0,0,0,0,2,0,0,2,0,2,0,0,0,2,0,2,2,2,0,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
			{2,0,0,2,0,2,0,0,2,0,2,0,0,0,2,0,2,0,0,0,0,0,0,2,0,0,2,0,0,2,0,2,0,0,2,0,0,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
			{2,2,2,2,0,2,0,0,2,0,2,0,0,0,2,0,2,2,2,0,0,0,0,0,2,2,0,0,0,0,2,0,0,0,2,2,2,0,2,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,}
	};
	for(int j = 0; j<5; j++){
		for(int i = 0; i<56; i++){
			int x =arr[j][i];
			switch(x){
			case 0 :
				xcor += size;
				break;
			case 2 :
				DrawStatescreen(frame, xcor, ycor, size,stride, 0, 0, 255);
				xcor += size;
				break;
			}
		}
		ycor += size;
		xcor -= 56*size;
	}
	//Xil_DCacheFlushRange((unsigned int) frameBufA, DEMO_MAX_FRAME);
}

void DefineVictory(u8 *frame, u32 xcor, u32 ycor, u32 size, u32 stride){
	int arr[5][56] = {
			{2,0,0,0,2,0,2,2,2,0,2,2,2,0,2,2,2,2,2,0,2,2,2,2,0,2,2,2,0,0,2,0,0,0,2,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0,},
			{2,0,0,0,2,0,0,2,0,0,2,0,0,0,0,0,2,0,0,0,2,0,0,2,0,2,0,0,2,0,0,2,0,2,0,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0,},
			{2,0,0,0,2,0,0,2,0,0,2,0,0,0,0,0,2,0,0,0,2,0,0,2,0,2,2,2,0,0,0,0,2,0,0,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0,},
			{0,2,0,2,0,0,0,2,0,0,2,0,0,0,0,0,2,0,0,0,2,0,0,2,0,2,0,2,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,},
			{0,0,2,0,0,0,2,2,2,0,2,2,2,0,0,0,2,0,0,0,2,2,2,2,0,2,0,0,2,0,0,0,2,0,0,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0,}
	};
	for(int j = 0; j<5; j++){
		for(int i = 0; i<56; i++){
			int x =arr[j][i];
			switch(x){
			case 0 :
				xcor += size;
				break;
			case 2 :
				DrawStatescreen(frame, xcor, ycor, size,stride, 255, 0, 0);
				xcor += size;
				break;
			}
		}
		ycor += size;
		xcor -= 56*size;
	}
	//Xil_DCacheFlushRange((unsigned int) frameBufA, DEMO_MAX_FRAME);
}

void DrawStatescreen(u8 *frame, u32 xcor, u32 ycor, u32 size, u32 stride, u8 Red, u8 Blue, u8 Green){
	xcor = xcor*3;
	u32 iPixelAddr;
	for(int x = 0; x < (size*3); x+=3){
		iPixelAddr = xcor+(stride*ycor)+x;
		for(int y = 0; y < size; y++){
			frame[iPixelAddr] = Red;
			frame[iPixelAddr + 1] = Blue;
			frame[iPixelAddr + 2] = Green;

			iPixelAddr += stride;
		}
	}
}

void DemoInitialize()
{
	int Status;
	XAxiVdma_Config *vdmaConfig;

	//initialize pointer to front-buffer (frameBufA)
	ptr_frameBufA = frameBufA;



	/*
	 * Initialize a timer used for a simple delay
	 */
	TimerInitialize(SCU_TIMER_ID);


	// Initialize VDMA driver
	vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
	if (!vdmaConfig)
	{
		xil_printf("No video DMA found for ID %d\r\n", VGA_VDMA_ID);
		return;
	}
	Status = XAxiVdma_CfgInitialize(&vdma, vdmaConfig, vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS)
	{
		xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Display controller and start it
	 */
	Status = DisplayInitialize(&dispCtrl, &vdma, DISP_VTC_ID, DYNCLK_BASEADDR, &ptr_frameBufA, DEMO_STRIDE);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Display Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}
	Status = DisplayStart(&dispCtrl);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Couldn't start display during demo initialization%d\r\n", Status);
		return;
	}
	SDwrite(2,3,4,5);
	return;
}

void SwapBuf(){

	int frameBufLen;
	frameBufLen = sizeof(frameBufB);

	memcpy(frameBufA, frameBufB, frameBufLen);							//copy frame from back-buffer to front-buffer
	memset(frameBufB, 0, frameBufLen);									//clear back-buffer
	Xil_DCacheFlushRange((unsigned int) frameBufA, DEMO_MAX_FRAME);		//print from  front-buffer

}


void DefineAlien(u32 width, u32 height, u32 xMove, u32 yMove){

	u8 i;
	u32 wh;						//width/height of enemy (is quadratic)
	u32 offset;					//offset of enemy relative to the enemy on the left hand side
	u32 margin;					//margin for aliens from screen edges
	u8 alienArrLen;

	//init param
	alienArrLen = 30;

	margin = width/5;
	wh = (width-margin)/20;


			//initialize alien positions in top of screen (3 rows with 10 aliens in each row)
			for(i=0; i<alienArrLen; i++){

				if(i < 10){
					alienNest[i].xLeft = xMove+((i+1)+i*3*wh);				//first row
					alienNest[i].xRight = alienNest[i].xLeft + 3*wh;
					alienNest[i].ytop = (0.125*height-0.5*wh)+yMove;
					alienNest[i].ybtm = alienNest[i].ytop+wh;
				}else if(i < 20){
					alienNest[i].xLeft = xMove+((i+1-10)+(i-10)*3*wh);		//second row
					alienNest[i].xRight = alienNest[i].xLeft + 3*wh;
					alienNest[i].ytop = (0.125*height-0.5*wh+1.5*wh)+yMove;
					alienNest[i].ybtm = alienNest[i].ytop+wh;
				}else if(i <30){
					alienNest[i].xLeft = xMove+((i+1-20)+(i-20)*3*wh);		//third row
					alienNest[i].xRight = alienNest[i].xLeft + 3*wh;
					alienNest[i].ytop = (0.125*height-0.5*wh+3*wh)+yMove;
					alienNest[i].ybtm = alienNest[i].ytop+wh;
				}
			}
}
void DefineHero(u32 width, u32 height, s32 skew){

	u32 wh;
	u8 margin;

	//init param
	wh = 32;		//wh=width/height of hero (quadratic)
	margin = 2*wh;


	hero.xLeft = (0.5*width-0.5*wh)*3+(3*skew);
	hero.xRight = hero.xLeft + wh*3;
	hero.ytop = height-(0.5*margin);
	hero.ybtm = hero.ytop+wh;

}

void DefineBullet(u32 width, u32 height, s32 bulletMove, s32 skew){

	u32 wh;
	u8 margin;

	//init param
	wh = 8;		//wh=width/height of bullet
	margin = 2*wh;

	bullet.xLeft = ((0.5*width-0.5*wh)*3)+(3*skew);
	bullet.xRight = bullet.xLeft + wh*3;
	bullet.ytop = height-(0.5*margin)+bulletMove;
	bullet.ybtm = bullet.ytop+wh;

}

void DrawBullet(u8 *frame, u32 width, u32 height, u32 stride){

		//init bullet settings
		u32 wh;
		u32 iSprite;

		//init Screen settings
		u32 xcoi, ycoi;				//x,y coordinates of screen
		u32 iPixelAddr;				//running index of pixel addresses

		wh = bullet.ybtm-bullet.ytop;
		iSprite = 0;

		for(xcoi=0; xcoi<wh*3; xcoi+=3){

			iPixelAddr = (bullet.ytop-20)*stride+bullet.xLeft + xcoi-7;

			for(ycoi=0; ycoi<wh;ycoi++){
				if(Shot[iSprite] != 128)
					frame[iPixelAddr] = Shot[iSprite];
				if(Shot[iSprite+1] != 128)
					frame[iPixelAddr+1] = Shot[iSprite+1];
				if(Shot[iSprite+2] != 128)
					frame[iPixelAddr+2] = Shot[iSprite+2];
							/*
							 * This pattern is printed one vertical line at a time, so the address must be incremented
							 * by the stride instead of just 1.
							 */
				iPixelAddr += stride;
				iSprite += 3;
			}
		}

}

void DrawSprites(u8 opt, u8 *frame, u32 width, u32 height, u32 stride){

		u8 i;
		u8 alienArrLen;
		u32 wh;
		u32 margin;

		//screen
		u32 xcoi, ycoi;				//x,y coordinates of screen
		u32 iPixelAddr;				//running index of pixel addresses
		u32 iSpritePlayer;
		u32 iSpriteAlien;


		if(opt == 1){			//OPT 1 = draw player

			//init param
			wh = hero.ybtm-hero.ytop;
			iSpritePlayer = 0;


			for(xcoi=0; xcoi<wh*3; xcoi+=3){

				iPixelAddr = (hero.ytop-1)*stride+hero.xLeft + xcoi;

				for(ycoi=0; ycoi<wh;ycoi++){
					if(Player[iSpritePlayer] != 128)  // make the spaces in the sprite as the background, by avoiding the numbers 128 in the array of player
						frame[iPixelAddr] = Player[iSpritePlayer];
					if(Player[iSpritePlayer+1] != 128)
						frame[iPixelAddr+1] = Player[iSpritePlayer+1];
					if(Player[iSpritePlayer+2] != 128)
						frame[iPixelAddr+2] = Player[iSpritePlayer+2];

					iPixelAddr += stride;
					iSpritePlayer += 3;
				}
			}
		}
		if(opt == 2){			//OPT 2 = draw aliens

			//init param
			alienArrLen = 30;
			margin = width/5;


			//checking for all aliens coordinates (in oppose for all pixels on screen)
			for(i=0;i<alienArrLen; i++){
				wh = 32;//alienNest[i].ybtm-alienNest[i].ytop;
				iSpriteAlien = 0;

				for(xcoi=0;xcoi<wh*3;xcoi+=3){
					iPixelAddr = (alienNest[i].ytop-1)*stride+alienNest[i].xLeft + xcoi;

					for(ycoi=0;ycoi<wh;ycoi++){

						if(alienNest[i].status == 0){
							if(Alien1_1[iSpriteAlien] != 128)
								frame[iPixelAddr] = Alien1_1[iSpriteAlien];
							if(Alien1_1[iSpriteAlien+1] != 128)
								frame[iPixelAddr+1] = Alien1_1[iSpriteAlien+1];
							if(Alien1_1[iSpriteAlien+2] != 128)
								frame[iPixelAddr+2] = Alien1_1[iSpriteAlien+2];
						}
//						else {
//							if(Alien1_1[iSpriteAlien] != 128)
//								frame[iPixelAddr] = DeathAlien[iSpriteAlien];
//							if(Alien1_1[iSpriteAlien+1] != 128)
//								frame[iPixelAddr+1] = DeathAlien[iSpriteAlien+1];
//							if(Alien1_1[iSpriteAlien+2] != 128)
//								frame[iPixelAddr+2] = DeathAlien[iSpriteAlien+2];
//						}
						iPixelAddr += stride;
						iSpriteAlien += 3;
					}
				}
			}

		}
}

void DrawDeathAlien(u8 *frame, u32 width, u32 height, u32 stride){

				u8 i;
				u8 newAlienArrLen;
				u32 wh;
				u32 margin;

				//screen
				u32 xcoi, ycoi;				//x,y coordinates of screen
				u32 iPixelAddr;				//running index of pixel addresses
				u32 iSpriteNewAlien;


				//init param
				newAlienArrLen = 30;
				margin = width/5;

				//checking for all aliens coordinates (in oppose for all pixels on screen)
				for(i=0;i<newAlienArrLen; i++){
					wh = 32;//alienNest[i].ybtm-alienNest[i].ytop;
					iSpriteNewAlien = 0;
					for(xcoi=0;xcoi<wh*3;xcoi+=3){
						iPixelAddr = (alienNest[i].ytop-1)*stride+alienNest[i].xLeft + xcoi;

						for(ycoi=0;ycoi<wh;ycoi++){

								if(Alien1_1[iSpriteNewAlien] != 128)
									frame[iPixelAddr] = DeathAlien[iSpriteNewAlien];
								if(Alien1_1[iSpriteNewAlien+1] != 128)
									frame[iPixelAddr+1] = DeathAlien[iSpriteNewAlien+1];
								if(Alien1_1[iSpriteNewAlien+2] != 128)
									frame[iPixelAddr+2] = DeathAlien[iSpriteNewAlien+2];

							iPixelAddr += stride;
							iSpriteNewAlien += 3;
						}
					}
				}

}

void Collision(){

	int alienArrLen = 30;
	for(int i = 0; i < alienArrLen; i++){
		for(int j = 1; j <= 32; j++)
			if(alienNest[i].ybtm+j == bullet.ytop && (alienNest[i].xLeft < bullet.xRight && alienNest[i].xRight > bullet.xLeft) && alienNest[i].status == 0){
				alienNest[i].status = 1;
				bulletHit = 1;
				moveBullet = standby;
		}
	}
}

void MoveBullet(){

	switch(moveBullet){
	case standby:
		bulletFlag = 0;
	break;

	case fire:
		bulletyMove = 0;
		moveBullet = yDecr;
	break;

	case yDecr:
		bulletyMove-=5;
		Collision();
		if(bulletyMove == -500)
			moveBullet = standby;
	break;


	}

}

void MoveHero(){

	int xRightMax = 300;
	int xLeftMax = -300;		// form origin, in which the player was defined to be.

	switch(movePlayer){
	case init:
		movePlayer = wait;
	break;
	case wait:
		if(btns == btnRight && skew < xRightMax)
			movePlayer = moveRight;
		else if (btns == btnLeft && skew > xLeftMax)
			movePlayer = moveLeft;
	break;
	case moveRight:
		skew+=2;
		movePlayer = wait;
	break;
	case moveLeft:
		skew-=2;
		movePlayer = wait;
	break;
	}
}

void SDread(){

	init_platform();

	result = f_mount(&FS_instance, Path, 1);


	sprintf(FileName, "file.txt");

	log_file = (char *) FileName;

	result = f_open(&file, log_file, FA_READ);

	char SDArr[4];

	result = f_read(&file, (void*)(char*)SDArr, sizeof(SDArr), &NumBytesRead);

	printf("read highscore data = %s\n\r", (char*)SDArr);

	result = f_close(&file);

	cleanup_platform();
}

void SDwrite(int num1,int num2,int num3,int num4){


	init_platform();

	result = f_mount(&FS_instance, Path, 1);

	sprintf(FileName, "file.txt");

	log_file = (char*)FileName;

	result = f_open(&file, log_file, FA_CREATE_ALWAYS | FA_WRITE);

	char arr[8];

	sprintf(arr,"%d%d%d%d", num1,num2,num3,num4);

	result = f_write(&file, (const void*)(char*)arr, sizeof(arr), &NumBytesRead);

	result = f_close(&file);

	cleanup_platform();
}
