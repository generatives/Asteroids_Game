/*******************************************************************************************
This is free, public domain software and there is NO WARRANTY.
No restriction on use. You can use, modify and redistribute it for
personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.

Sheldon Patterson
********************************************************************************************/

#include "includes.h"
#include "bsp.h"
#include "gpio.h"
#include "sysTick.h"
#include "timer.h"
#include "wdt.h"
#include "beeper.h"
#include "lcd.h"
#include "version.h"
#include "led.h"
#include "button.h"
#include "rtc.h"
#include "demo.h"

#define LETTERS_PER_PLAN_BLOCK 8



/**************************************************************************
 *                                  Constants
 **************************************************************************/
static const char topPlans[9][LETTERS_PER_PLAN_BLOCK] =    {{"rbbbbbrb"}, {"bbabbrbr"}, {"rabbrbrr"}, {"babbrrbr"}, {"brrbbbrb"}, {"bbrbbbrb"}, {"bbarbrbb"}, {"rbbbbbrb"}, {"rrbbbrbb"}};
static const char bottomPlans[9][LETTERS_PER_PLAN_BLOCK] = {{"bbbrbbbb"}, {"bbrbbrbr"}, {"bbrbbrbb"}, {"babbrrbb"}, {"bbbrbrbb"}, {"bbbbrbbb"}, {"rbbrbbrb"}, {"bbbrbbbb"}, {"bbbbrbbb"}};


/**************************************************************************
 *                                  Types
 **************************************************************************/
typedef struct
{
  s32 shields;
  s32 missles;
  s32 position;
  bool damaged;
  bool destroyed;
} PLAYER;

typedef struct
{
  s32 advance;
  s32 missleAdvance;
  s32 time;
} TIMERS;

typedef struct
{
  s32 lastTime;
  s32 timeDifference;
  s32 index;
  s32 randN;
} GENSUPPORT;

typedef struct
{
  s32 lastButton;
  char topObjectPosition[LCD_NUM_CHARS_PER_LINE+1];
  char bottomObjectPosition[LCD_NUM_CHARS_PER_LINE+1];
  char nextTopObject;
  char nextBottomObject;
  char topMissilePosition[LCD_NUM_CHARS_PER_LINE];
  char bottomMissilePosition[LCD_NUM_CHARS_PER_LINE];
  char collideWith;
} GAMESTATE;


typedef struct
{
  char topLine[LCD_NUM_CHARS_PER_LINE];
  char bottomLine[LCD_NUM_CHARS_PER_LINE];
} DISPLAYSTATE;

/**************************************************************************
 *                                  Variables
 **************************************************************************/
PLAYER player;
TIMERS timers;
GAMESTATE gameState;
DISPLAYSTATE displayState;
LEDS leds;
GENSUPPORT genSupport;

/**************************************************************************
 *                                  Prototypes
 **************************************************************************/
static void MainInit(void);
void GameInit(void);
void GameUpdate(void);
s32 CheckButtons(void);
void WaitForResponse(void);
void DisplayUpdate(void);
void PlayerUpdate(void);
void CheckCollisions(void);
static void GenerateAsteroids(void);
void LedDisplayUpdate(void);

/**************************************************************************
 *                                  Global Functions
 **************************************************************************/
void main(void)
{
   MainInit();
   //DemoInit();
   GameInit();


   for (;;)
   {
      // Drivers
      LedUpdate();
      ButtonUpdate();
      LcdUpdate();

      // Application

      gameState.lastButton = CheckButtons();
      if (!player.destroyed) {
        CheckCollisions();
        GameUpdate();
        PlayerUpdate();
        LedDisplayUpdate();
      }
      DisplayUpdate();

      //Timed Events
      timers.advance--;
      timers.missleAdvance--;
      timers.time++;

      //Restart Event
      if (player.destroyed && gameState.lastButton == 0) {
        GameInit();
      }

      GpioSet(GPIO_PIN_HEARTBEAT);   // active low
//    TimerWaitXMs(SYS_TICK_RATE_MS);
      SysTickWait();
      GpioClear(GPIO_PIN_HEARTBEAT);
   }
}


/**************************************************************************
 *                                 Private Functions
 **************************************************************************/
static void MainInit(void) {
   BspInit();
   SysTickInitMs(SYS_TICK_RATE_MS);
   TimerInit();
   RtcInit();
   BeeperInit();
   ButtonInit();
   LedInit();
   LcdInit();
   WdtInit();
}

void GameInit(void) {
  for(s32 j = 0; j < NELEMENTS(gameState.topObjectPosition); j++)
  {
    gameState.topObjectPosition[j] = 'b';
    gameState.bottomObjectPosition[j] = 'b';
  }
  for(s32 j = 0; j < NELEMENTS(gameState.topMissilePosition); j++)
  {
    gameState.topMissilePosition[j] = 'b';
    gameState.bottomMissilePosition[j] = 'b';
  }
  gameState.lastButton = -1;

  player.shields = 3;
  player.missles = 3;
  player.destroyed = false;
  player.position = 0;
  player.damaged = false;

  timers.advance = 0;
  timers.missleAdvance = 0;

  genSupport.lastTime = 0;
  genSupport.timeDifference = 0;
  genSupport.index = 0;
  genSupport.randN = 0;

  LedRgbSet(COLOR_GhostWhite);
}

void GameUpdate(void) {
  if(timers.advance == 0) {
    GenerateAsteroids();

    for(s32 i = 1; i < NELEMENTS(gameState.topObjectPosition); i++) {
      gameState.topObjectPosition[i-1] = gameState.topObjectPosition[i];
      gameState.bottomObjectPosition[i-1] = gameState.bottomObjectPosition[i];
    }

    gameState.topObjectPosition[LCD_NUM_CHARS_PER_LINE] = gameState.nextTopObject;
    gameState.bottomObjectPosition[LCD_NUM_CHARS_PER_LINE] = gameState.nextBottomObject;

    timers.advance = 500 - ((timers.time / 1000) * 2);
  }

  if (timers.missleAdvance == 0) {
    gameState.bottomMissilePosition[19] = 'b';
    gameState.topMissilePosition[19] = 'b';

    for(s32 i = 18; i > 0; i--) {
      if (gameState.topMissilePosition[i] == 'm') {
        gameState.topMissilePosition[i+1] = 'm';
        gameState.topMissilePosition[i] = 'b';
      }
      if (gameState.bottomMissilePosition[i] == 'm') {
        gameState.bottomMissilePosition[i+1] = 'm';
        gameState.bottomMissilePosition[i] = 'b';
      }
    }
    timers.missleAdvance = 100;
  }
}

void DisplayUpdate(void) {

  if (!player.destroyed) {

    for (s32 i = 0; i < LCD_NUM_CHARS_PER_LINE; i++) {
      if (gameState.topObjectPosition[i] == 'b')
        displayState.topLine[i] = ' ';

      if (gameState.bottomObjectPosition[i] == 'b')
        displayState.bottomLine[i] = ' ';

      if (gameState.topObjectPosition[i] == 'r')
        displayState.topLine[i] = '0';

      if (gameState.bottomObjectPosition[i] == 'r')
        displayState.bottomLine[i] = '0';

      if (gameState.topObjectPosition[i] == 'a')
        displayState.topLine[i] = '+';

      if (gameState.bottomObjectPosition[i] == 'a')
        displayState.bottomLine[i] = '+';

      if (gameState.topMissilePosition[i] == 'm')
        displayState.topLine[i] = '-';

      if (gameState.bottomMissilePosition[i] == 'm')
        displayState.bottomLine[i] = '-';
    }
    if (player.position == 0) {
      displayState.topLine[0] = '>';
    }

    if (player.position == 1) {
      displayState.bottomLine[0] = '>';
    }
    LcdSetPos(0, 0);
    LcdPuts(displayState.topLine);

    LcdSetPos(1, 0);
    LcdPuts(displayState.bottomLine);
  }

  else {
    LcdClear();
    LcdSetPos(0, 0);
    LcdPuts("Game Over");
  }
}

void PlayerUpdate(void) {
  if(gameState.lastButton==0) {
    player.position = 1 - player.position;
  }

  if (gameState.lastButton == 1 && player.missles > 0) {
    player.missles--;

    if (player.position == 0)
      gameState.topMissilePosition[1] = 'm';

    if (player.position == 1)
      gameState.bottomMissilePosition[1] = 'm';
  }

  if(gameState.collideWith == 'r')
    player.shields--;

  if(gameState.collideWith == 'a') {
    if (player.missles <= 6) {
      player.missles = player.missles + 2;
      if (player.position == 0) {
        gameState.topObjectPosition[0] = 'b';
      }
      if (player.position == 1) {
        gameState.bottomObjectPosition[0] = 'b';
      }
    }
  }
  if(player.shields <= 0)
    player.destroyed = true;
}

s32 CheckButtons(void) {
  s32 buttonPressed;

  if (ButtonWasPressed(BTN_L) && (gameState.lastButton != 0)) {
    buttonPressed = 0;
    ButtonPressAck(BTN_L);
  }

  else if (ButtonWasPressed(BTN_LM) && (gameState.lastButton != 1)) {
    buttonPressed = 1;
    ButtonPressAck(BTN_LM);
  }

  else if (ButtonWasPressed(BTN_RM) && (gameState.lastButton != 2)) {
    buttonPressed = 2;
    ButtonPressAck(BTN_RM);
  }

  else if (ButtonWasPressed(BTN_R) && (gameState.lastButton != 3)) {
    buttonPressed = 3;
    ButtonPressAck(BTN_R);
  }

  else
    buttonPressed = -1;

  if (buttonPressed != -1) {
    genSupport.timeDifference = timers.time - genSupport.lastTime;
    genSupport.lastTime = timers.time;
  }

  return buttonPressed;
}

void CheckCollisions(void) {
  if(player.position == 0)
    gameState.collideWith = gameState.topObjectPosition[0];

  if(player.position == 1)
   gameState.collideWith = gameState.bottomObjectPosition[0];

   for (s32 i =0; i < LCD_NUM_CHARS_PER_LINE; i++) {
     if(gameState.bottomMissilePosition[i] == 'm' && gameState.bottomObjectPosition[i] == 'r') {
       gameState.bottomMissilePosition[i] = 'b';
       gameState.bottomObjectPosition[i] = 'b';
     }
     if(gameState.topMissilePosition[i] == 'm' && gameState.topObjectPosition[i] == 'r') {
       gameState.topMissilePosition [i]= 'b';
       gameState.topObjectPosition[i] = 'b';
     }
   }

}

static void GenerateAsteroids(void) {

  if (genSupport.index == LETTERS_PER_PLAN_BLOCK - 1) {
    genSupport.randN = (1 + genSupport.timeDifference) * timers.time;
    while (genSupport.randN >= 10)
      genSupport.randN = genSupport.randN / 10;

    genSupport.index = 0;
  }

  gameState.nextTopObject = topPlans[genSupport.randN][genSupport.index];
  gameState.nextBottomObject = bottomPlans[genSupport.randN][genSupport.index];
  genSupport.index++;

}

void LedDisplayUpdate(void) {
  s32 i = 0;
  for (LEDS led = (LEDS)0; led <= LED_RED; led++) {
    if (i < player.missles) {
      LedSetPwm(led, 6);
    }
    else
      LedOff(led);
    i++;
  }

}