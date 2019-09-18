
/*  cnguy072_project.c
 *  Name & E-mail: Calvin Nguyen cnguy072@ucr.edu
 *  CS Login: cnguy072
 *  Partner(s) Name & E-mail: Ryan Sabik - rsabi001@ucr.edu
 *  Lab Section: 022
 *  Assignment: Project
 *
 *  I acknowledge all content contained herein, excluding template or example
 *  code, is my own original work.
 *  
 *  Some minor modifications to included keypad.h code has been made
 *  Task scheduler format taken from PES/RIBS 
 */

#include <avr/io.h>

// Following includes from Lab8: https://docs.google.com/folder/d/0B-mTsOswSyiKWWJ5clNtT1EtSVk/edit
#include "keypad.h" // modified
#include "timer.h" // implicitly includes "bit.h"

// LCD Documentation: https://www.sparkfun.com/datasheets/LCD/HD44780.pdf

/* Todo:
  More weapons: richochet, melee
  sound
  dodge/dash

*/

/* Tips for the future:
  Alias parameter values with more symbolic information
    e.g. x is 0 and y is 1, for their position in the arrays
*/


//============================================================================================================================================
// Constants:

#define bool char
#define true 1
#define false 0

// Constant Settings:
#define CLK_PER 8
#define GAME_PER CLK_PER // Beyond this point, PER stands for frames in game time
#define PL_PER 7 // So in this case, this means the player gets updated every PL_PER frames
#define LIVES 9
#define INVUL_DUR 25 // Number of player object ticks of invulnerability
#define CMD_DELAY 7 // vol: 4 6st in: 18 22st | indirectly modifies game speed | if display becomes erratic, raise this value

// Product of Spaces cannot be larger than 8, otherwise LCD will behave strangely, stat data will be offscreen
#define COL_SPACE 8 // Refrain from having less than 3
#define ROW_SPACE 1 // ROW_SPACE cannot be larger than 2

// All objects | Field is 8*(5x8), or 320 individual pixels
#define NUM_OBJ 120 // Handling too many objects will result in compiler error, game discards any objects generated at this cap, however this is extremely rare

// Weapon/Object Properties:
#define NUM_EQUIP 8 // Number of weapon types. When adding new weapons, update the ammo array

#define PISTOL_PER 3 // 2 obj
#define PISTOL_COOL 8
#define PISTOL_RANGE 22
#define RIFLE_PER 2 // 3 obj
#define RIFLE_COOL 12
#define RIFLE_AMMO 6 
#define RAIL_PER 5 // ~40 obj
#define RAIL_COOL 33
#define RAIL_AMMO 2 
#define WAVE_PER 5 // 10 obj
#define WAVE_COOL 16
#define WAVE_AMMO 4 
#define WAVE_RANGE 13
#define ROCKET_PER 2 // 4/45 obj
#define ROCKET_COOL 40
#define ROCKET_RANGE 19
#define ROCKET_AMMO 2 
#define MGUN_PER 2 // 1 obj
#define MGUN_COOL 4
#define MGUN_RANGE 15
#define MGUN_AMMO 14 
#define CLUSTER_PER 11 // 1/3/63 obj
#define CLUSTER_COOL 10
#define CLUSTER_RANGE 14
#define CLUSTER_AMMO 2    
#define HEAL_COOL 40
#define HEAL_AMMO 2 
#define HEAL_INVUL 25

#define FIRE_PER 4
#define FIRE_DUR 3

#define ITEM_PER 9
#define ITEM_DUR 150
#define EVENT_INTERVAL 750
#define MOD_CHANCE 16

//============================================================================================================================================
// Structures:

typedef struct _task {
  /*Tasks should have members that include: state, period,
      a measurement of elapsed time, and a function pointer.*/
  signed char state; //Task's current state
  unsigned long int period; //Task period
  unsigned long int elapsedTime; //Time elapsed since last task tick
  int (*TickFct)(int); //Task tick function
} task;

// Multi-purpose object
typedef struct _obj { 
  bool exist; // track if this object is in use
  bool collide; // track if object has collided or not, typically stops existing after expiry action is executed
  char type; // Object type: 0-projectile, 1-projectile, 2-explosive
  char who; // Who this object belongs to: 0-system, 1-P1, 2-P2, 3-hostile, 4-items
  signed char pos[4]; // Position of the object {char position, column(x), row(y)} negatives are invalid, but handled
  signed char next[2]; // Velocity of the object: Cartesian vector {x,y} 
  signed char jump; // y (vertical) modifier frames remaining
  unsigned char period; // Game frames to next update
  unsigned char elapsedTime; // Frames elapsed since last position update
  signed char expire; // Time till object expires
} obj;

// Player Object
typedef struct _player { 
  char who; // Who this object belongs to: 0-system, 1-P1, 2-P2
  char autofire; // Disables autofire on weapon fire/swapping
  char aimX; // Define directions: -1-left, 0-reserved, 1-right
  char aimY; // Define directions: -1-down, 0-reserved, 1-up
  signed char pos[4]; // Position of the object {char position, column(x), row(y)} negatives are invalid, but handled
  signed char next[2]; // Velocity of the object: Cartesian vector {x,y}
  unsigned char invul; // Number of invulnerability frames remaining
  signed char jump; // y (vertical) modifier frames remaining
  unsigned char cooldown; // Number of frames where player cannot shoot
  signed char life; // if <= 0, player loses
  unsigned char period; // Game frames to next update
  unsigned char elapsedTime; // Frames elapsed since last position update

  signed char equip;
  signed char inven[NUM_EQUIP];
} player;

//============================================================================================================================================
// Global Variables:

static char pause;

// Stores playing field 
char field[COL_SPACE][ROW_SPACE][8];

// Player Objects
player P1, P2;

// Object Instances
obj objs[NUM_OBJ]; 

// Ammo Array is referenced when player obtains a weapon, adds that ammo to inventory
const unsigned char ammo[] = {0,RIFLE_AMMO,RAIL_AMMO,WAVE_AMMO,ROCKET_AMMO,MGUN_AMMO,CLUSTER_AMMO,HEAL_AMMO};

//============================================================================================================================================
// Helper Functions/etc:

// Necessary delay, otherwise game would run too slowly in a SM based delay system
void delay(unsigned char a) {
  for(volatile unsigned char i = 0; i < a ; ++i) {}
}

void cmdWrite(unsigned char cmd) {
  PORTB = SetBit(PORTB,3, 0);
  PORTD = cmd;
  PORTB = SetBit(PORTB,4, 1);
  delay(CMD_DELAY);
  PORTB = SetBit(PORTB,4, 0);
  delay(CMD_DELAY);    
}

void dataWrite(unsigned char data) {
  PORTB = SetBit(PORTB,3,1);
  PORTD = data;
  PORTB = SetBit(PORTB,4, 1);
  delay(CMD_DELAY);
  PORTB = SetBit(PORTB,4, 0);
  delay(CMD_DELAY);
}

// sets the cursor position on the LCD
void setCursor(unsigned char col, unsigned char row) { 
  if(row == 0)
    cmdWrite(0x80 + col);
  else if(row == 1)
    cmdWrite(0xC0 + col);
}

// writes character data to addr on LCD
void createChar(char addr, char arr[8]) {
  if(addr >= 8 || addr < 0) return;
  else if(addr < 8) {
    cmdWrite(0x40 + (addr * 8));
    for(char i = 0; i < 8 ; ++i)
      dataWrite(arr[i]);
  }
  cmdWrite(0x80);
}

// Printing functions do not handle overflow on a row
// Print used to address by ASCII value
void write(char ascii) {
  dataWrite(ascii);
}

// Print for cstrings
void print(char str[]) {
  for(char i = 0; str[i] != '\0'; ++i)
    dataWrite(str[i]);
}

// Turns on the LCD, assumes you waited before calling this
void initLCD() {
  cmdWrite(0x38); // Fn Set
  cmdWrite(0x06); // Shift right
  cmdWrite(0x0C); // Disp on, cursor off, blink off
  cmdWrite(0x01); // Clear
}

//============================================================================================================================================

// Sets an entire byte array to 0
void clearByte( char arr[8] ) {
  for (int i = 0; i < 8; ++i)
    arr[i] = 0;
  return;
}

// Sets all objs to not exist and empties out the field storage
void initObjs() {
  for (char i = 0; i < NUM_OBJ; ++i) 
    objs[i].exist = false;
  for(char i = 0; i < COL_SPACE; ++i) 
    for(char j = 0; j < ROW_SPACE; ++j) 
      clearByte(field[i][j]);
}

// Sets initial stats of players
void initPlayers() {
  P1.who = 1;
  P1.aimX = 1;
  P1.aimY = 0;
  P1.pos[0] = 1;
  P1.pos[1] = 4;
  P1.pos[2] = 7;
  P1.pos[3] = ROW_SPACE - 1;
  P1.life = LIVES;
  P1.period = PL_PER;
  P1.elapsedTime = PL_PER;
  P1.invul = 50;
  P1.cooldown = 0;
  P1.equip = 0;

  P2.who = 2;
  P2.aimX = -1;
  P2.aimY = 0;
  P2.pos[0] = COL_SPACE-2;
  P2.pos[1] = 0;
  P2.pos[2] = 7;
  P2.pos[3] = ROW_SPACE - 1;
  P2.life = LIVES;
  P2.period = PL_PER;
  P2.elapsedTime = PL_PER;
  P2.invul = 50;
  P2.cooldown = 0;
  P2.equip = 0;

  /* Normal Ammo mode
  P1.inven[0] = -1;
  P1.inven[1] = 5;
  P1.inven[2] = 1;
  P1.inven[3] = 3;
  P1.inven[4] = 1;
  P1.inven[5] = 0;
  P1.inven[6] = 1;
  P1.inven[7] = 0;

  P2.inven[0] = -1;
  P2.inven[1] = 5;
  P2.inven[2] = 1;
  P2.inven[3] = 3;
  P2.inven[4] = 1;
  P2.inven[5] = 0;
  P2.inven[6] = 1;
  P2.inven[7] = 0;  
  */
  ///* High Ammo
  P1.life = 11;
  P2.life = 11;
  P1.inven[0] = -1;
  P1.inven[1] = 8;
  P1.inven[2] = 2;
  P1.inven[3] = 5;
  P1.inven[4] = 6;
  P1.inven[5] = 15;
  P1.inven[6] = 4;
  P1.inven[7] = 2;

  P2.inven[0] = -1;
  P2.inven[1] = 8;
  P2.inven[2] = 2;
  P2.inven[3] = 5;
  P2.inven[4] = 2;
  P2.inven[5] = 15;
  P2.inven[6] = 4;
  P2.inven[7] = 2;  
  //*/
  /* Debug Ammo Mode
  P1.life = 15;
  P2.life = 15;
  P1.inven[0] = -1;
  P1.inven[1] = -1;
  P1.inven[2] = -1;
  P1.inven[3] = -1;
  P1.inven[4] = -1;
  P1.inven[5] = -1;
  P1.inven[6] = -1;
  P1.inven[7] = -1;

  P2.inven[0] = -1;
  P2.inven[1] = -1;
  P2.inven[2] = -1;
  P2.inven[3] = -1;
  P2.inven[4] = -1;
  P2.inven[5] = -1;
  P2.inven[6] = -1;
  P2.inven[7] = -1; 
  //*/
}

// converts integer value to binary
char cBinary( char i ) {
  switch(i) {
    case 0: return 0b1;
    case 1: return 0b10;
    case 2: return 0b100;
    case 3: return 0b1000;
    case 4: return 0b10000;
    default: return 0b0; 
  }
}

// Draw obj/player on field
void setPixel( obj *item ) { 
  field[(*item).pos[0]][(*item).pos[3]][(*item).pos[2]] = field[(*item).pos[0]][(*item).pos[3]][(*item).pos[2]] | cBinary((*item).pos[1]);
}
void setPixelP( player *item ) { 
  field[(*item).pos[0]][(*item).pos[3]][(*item).pos[2]] = field[(*item).pos[0]][(*item).pos[3]][(*item).pos[2]] | cBinary((*item).pos[1]);
}

// Recalculates obj's position based on velocity, also handles wall collision and var jump
void calcNext( obj *item ) { 
  if((*item).jump == -1) {}
  else if((*item).jump > 0) {
    (*item).next[1] = 1;
    (*item).jump--;
  } else if((*item).jump == 0)
    (*item).next[1] = -1;

  signed char newX = (*item).pos[1] - (*item).next[0];
  if(newX > 4) { // if X greater than character limit, set prev(left) character space
    (*item).pos[0]--;
    if((*item).pos[0] <= -1) { // Collided with left wall
      if(((*item).type == 3 || (*item).type == 4) && (*item).who != 4) { // Cluster bomb bounce
        (*item).collide = false;
        (*item).next[0] = 1;
      } else
      (*item).collide = true;
      (*item).pos[0] = 0;
      (*item).pos[1] = 4;
    } else
      (*item).pos[1] = newX - 5; // Assumes you will never cross two characters at once
  } else if(newX < 0) { // if X less than character limit, go to next(right) character space
    (*item).pos[0]++;
    if((*item).pos[0] >= COL_SPACE) { // Collided with right wall
      if(((*item).type == 3 || (*item).type == 4) && (*item).who != 4) { // Cluster bomb bounce
        (*item).collide = false;
        (*item).next[0] = -1;
      } else
        (*item).collide = true;
      (*item).pos[0] = COL_SPACE - 1;
      (*item).pos[1] = 0;
    } else
      (*item).pos[1] = newX + 5; // Assumes you will never cross two characters at once
  } else // else still in same character space
    (*item).pos[1] = newX;
  
  signed char newY = (*item).pos[2] - (*item).next[1]; // Rows count up when moving down
  if(newY < 0) { // if Y greater than character limit, move up a character
    (*item).pos[3]--;
    if((*item).pos[3] == -1) { // ceiling collide
      if(((*item).type == 3 || (*item).type == 4) && (*item).who != 4) { // Cluster bomb bounce
        (*item).collide = false;
        (*item).jump = 0;
      } else
        (*item).collide = true;
      (*item).pos[2] = 0;
      (*item).pos[3] = 0;
    } else
      (*item).pos[2] = newY + 8;
  } else if(newY > 7) { // if Y greater than character limit, collided with floor wall
    (*item).pos[3]++;
    if((*item).pos[3] == ROW_SPACE) { //floor collide
      if(((*item).type == 3 || (*item).type == 4) && (*item).who != 4) { // Cluster bomb bounce
        (*item).collide = false;
        (*item).jump = 3;
      } else
        (*item).collide = true;
      (*item).pos[2] = 7;
      (*item).pos[3] = ROW_SPACE-1;
    } else
      (*item).pos[2] = newY - 8;
  } else 
    (*item).pos[2] = newY;
}

// Recalculates players's position based on velocity, also handles wall collision
void calcNextP( player *item ) { 
  signed char newX = (*item).pos[1] - (*item).next[0];
  if(newX > 4) { // if X greater than character limit, set prev(left) character space
    (*item).pos[0]--;
    if((*item).pos[0] <= -1) { // Collided with left wall
      (*item).pos[0] = 0;
      (*item).pos[1] = 4;
    } else
      (*item).pos[1] = newX - 5; // Assumes you will never cross two characters at once
  } else if(newX < 0) { // if X less than character limit, go to next(right) character space
    (*item).pos[0]++;
    if((*item).pos[0] >= COL_SPACE) { // Collided with right wall
      (*item).pos[0] = COL_SPACE - 1;
      (*item).pos[1] = 0;
    } else
      (*item).pos[1] = newX + 5; // Assumes you will never cross two characters at once
  } else // else still in same character space
    (*item).pos[1] = newX;
  
  signed char newY = (*item).pos[2] - (*item).next[1]; // Rows count up when moving down
  if(newY < 0) { // if Y greater than character limit, move up a character
    (*item).pos[3]--;
    if((*item).pos[3] == -1) { 
      (*item).pos[2] = 0;
      (*item).pos[3] = 0;
    } else
      (*item).pos[2] = newY + 8;
  } else if(newY > 7) { // if Y greater than character limit, collided with floor wall
    (*item).pos[3]++;
    if((*item).pos[3] == ROW_SPACE) { 
      (*item).pos[2] = 7;
      (*item).pos[3] = ROW_SPACE-1;
    } else
      (*item).pos[2] = newY - 8;
  } else 
    (*item).pos[2] = newY;
}

// gets index of unallocated obj
char getNewObj() {
  char i;
  for( i = 0; i < NUM_OBJ && objs[i].exist == true; ++i) {} // Find first obj not in use
  if( i == NUM_OBJ ) return -1; // Ran out of allocable objs, don't do anything
  return i;
}

// generate explosion
void explosion( obj *item, char radius ) {
  for(signed char y = -1 * radius; y < radius + 1; ++y) {
    for(signed char x = -1 * radius; x < radius + 1; ++x) {
      char n = getNewObj();
      if( n == -1 ) return;
      if(y * x == (radius * radius) || x * y == -1 * ( radius * radius )) objs[n].exist = false;
      else objs[n].exist = true;
      objs[n].collide = false;
      objs[n].who = (*item).who;
      objs[n].type = 0;
      objs[n].jump = -1;
      objs[n].period = FIRE_PER;
      objs[n].elapsedTime = objs[n].period;
      objs[n].expire = FIRE_DUR;
      objs[n].pos[0] = (*item).pos[0];
      objs[n].pos[1] = (*item).pos[1] + x;
      objs[n].pos[2] = (*item).pos[2] + y;
      objs[n].pos[3] = (*item).pos[3];
      objs[n].next[0] = (*item).next[0];
      objs[n].next[1] = (*item).next[1];
    }
  }   
}

void handleCollision( obj *item ) {
  if((*item).who != 0) { // if item is not neutral and assumed to exist (handled before called)
    if(P1.who != (*item).who) 
      if(P1.pos[0] == (*item).pos[0] && P1.pos[1] == (*item).pos[1] && P1.pos[2] == (*item).pos[2] && P1.pos[3] == (*item).pos[3]) {
        if((*item).who == 4) { // item object collision case
          (*item).exist = false;
          P1.inven[(*item).type] += ammo[(*item).type]; 
        }
        else if(P1.invul == 0) {
          P1.life--;
          P1.invul = INVUL_DUR;
        }
        (*item).collide = true;
      }
    if(P2.who != (*item).who) 
      if(P2.pos[0] == (*item).pos[0] && P2.pos[1] == (*item).pos[1] && P2.pos[2] == (*item).pos[2] && P2.pos[3] == (*item).pos[3]) {
        if((*item).who == 4) { // item object collision case
          (*item).exist = false;
          P2.inven[(*item).type] += ammo[(*item).type]; 
        }
        else if(P2.invul == 0) {
          P2.life--;
          P2.invul = INVUL_DUR;
        } 
        (*item).collide = true;
      }
  }

  // Item logic
  if((*item).who == 4) { 
    if((*item).expire == 0)
      (*item).exist = false;
    return;
  }

  //Collision action logic follows:
  if((*item).expire == 0)
    (*item).collide = true;

  // Particle effect
  if((*item).type == 1 && (*item).collide) { 
    char n = getNewObj();
    if( n == -1 ) return;
    objs[n].exist = true;
    objs[n].collide = false;
    objs[n].who = 0;
    objs[n].type = 0;
    objs[n].jump = -1;
    objs[n].period = PISTOL_PER;
    objs[n].elapsedTime = objs[n].period;
    objs[n].expire = (rand() % 2) + 1;
    objs[n].pos[0] = (*item).pos[0];
    objs[n].pos[1] = (*item).pos[1];
    objs[n].pos[2] = (*item).pos[2];
    objs[n].pos[3] = (*item).pos[3];
    objs[n].next[0] = (rand() % 3) - 1;
    objs[n].next[1] = (rand() % 3) - 1;
  }

  // Rocket logic
  if((*item).type == 2 && (*item).collide) { 
    (*item).who = 3;
    explosion(item, 3);
  }

  // Cluster bomb logic - bounce logic is in position check logic
  if((*item).type == 3 && (*item).expire == 0) {
    for(signed char x = -1; x < 2; ++x) {
      char n = getNewObj();
      if( n == -1 ) return;
      objs[n].exist = true;
      objs[n].collide = false;
      objs[n].who = 0;
      objs[n].type = 4;
      objs[n].jump = 3;
      objs[n].period = CLUSTER_PER - 3;
      objs[n].elapsedTime = objs[n].period;
      objs[n].expire = 11;
      objs[n].pos[0] = (*item).pos[0];
      objs[n].pos[1] = (*item).pos[1];
      objs[n].pos[2] = (*item).pos[2];
      objs[n].pos[3] = (*item).pos[3];
      objs[n].next[0] = x;
      objs[n].next[1] = 0;
    }
    (*item).exist = false;
  }
  if((*item).type == 4 && (*item).expire == 0) {
    (*item).who = 3;
    (*item).next[0] = 0;
    (*item).next[1] = 0;
    explosion(item, 2);
    (*item).exist = false;
  } // End of Cluster bomb logic

  if((*item).collide) 
    (*item).exist = false;
}

// Draws field data onto LCD
void drawField() {
  for(char i = 0; i < COL_SPACE; ++i)
    for(char j = 0; j < ROW_SPACE; ++j) {
      createChar(i+4*j, field[i][j]);
      setCursor(i,j);
      write(i+4*j);
    }
}

// Ticks all objects, calculates their new positions, handles collisions, then draws the screen
void evalField() {
  // Clear screen
  for(char i = 0; i < COL_SPACE; ++i) 
    for(char j = 0; j < ROW_SPACE; ++j) 
      clearByte(field[i][j]);
  // Update players
  if(P1.elapsedTime == P1.period) {
    calcNextP(&P1);
    P1.elapsedTime = 0;
    if(P1.invul != 0) P1.invul--;
    if(P1.cooldown != 0) P1.cooldown--;
  } else
    P1.elapsedTime++;
  if(!(P1.invul % 2))
    setPixelP(&P1);
  if(P2.elapsedTime == P2.period) {
    calcNextP(&P2);
    P2.elapsedTime = 0;
    if(P2.invul != 0) P2.invul--;
    if(P2.cooldown != 0) P2.cooldown--;
  } else    
    P2.elapsedTime++;
  if(!(P2.invul % 2))
    setPixelP(&P2);
  // Update all non-player objects
  for (char i = 0; i < NUM_OBJ; ++i) {
    if (objs[i].exist) {
      if(objs[i].elapsedTime == objs[i].period) {
        if(objs[i].expire != -1) 
          objs[i].expire--;
        if (objs[i].exist) {
          calcNext(&(objs[i]));
          objs[i].elapsedTime = 0;
        }
      } else
        objs[i].elapsedTime++;
      setPixel(&(objs[i]));
      handleCollision (&(objs[i]));
    }
  }
  drawField();
}

void genEvent( char eventNo ) {
  switch(eventNo) {
    case 0: { // spawn a cluster bomb
      char c = (rand() % (COL_SPACE / 2)) + 2;
      char x = rand() % 5;
      for(signed char x = -1; x < 2; ++x) {
      char n = getNewObj();
      if( n == -1 ) return;
      objs[n].exist = true;
      objs[n].collide = false;
      objs[n].who = 0;
      objs[n].type = 4;
      objs[n].jump = 3;
      objs[n].period = CLUSTER_PER;
      objs[n].elapsedTime = objs[n].period;
      objs[n].expire = 25;
      objs[n].pos[0] = c;
      objs[n].pos[1] = x;
      objs[n].pos[2] = 3;
      objs[n].pos[3] = 0;
      objs[n].next[0] = x;
      objs[n].next[1] = 0;
      }
    }
    break;
  case 1: { // generate an instance of rain
      char n = getNewObj();
      if( n == -1 ) return;
      objs[n].exist = true;
      objs[n].collide = false;
      objs[n].who = 0;
      objs[n].type = 0;
      objs[n].jump = 0;
      objs[n].period = 2;
      objs[n].elapsedTime = objs[n].period;
      objs[n].expire = 0;
      objs[n].pos[0] = (rand() % COL_SPACE);
      objs[n].pos[1] = rand() % 5;
      objs[n].pos[2] = -1;
      objs[n].pos[3] = 0;
      objs[n].next[0] = rand() % 2;
      objs[n].next[1] = 0;
    }   
    break;
  case 2:
    for(signed char c = 0; c < COL_SPACE; ++c) {
      for(char x = 0; x != 5; ++x) {
        char n = getNewObj();
        if( n == -1 ) return;
        objs[n].exist = true;
        objs[n].collide = false;
        objs[n].who = 3;
        objs[n].type = 1;
        objs[n].jump = -1;
        objs[n].period = 3;
        objs[n].elapsedTime = objs[n].period;
        objs[n].expire = 5;
        objs[n].pos[0] = c;
        objs[n].pos[1] = x;
        objs[n].pos[2] = 7;
        objs[n].pos[3] = 0;
        objs[n].next[0] = 0;
        objs[n].next[1] = 0;
      }
    }
    break;
  default:
    break;
  } 
}

// Creates an item to pickup
void genItem( char iType ) {
  char n = getNewObj();
  if( n == -1 ) return;
  objs[n].exist = true;
  objs[n].collide = false;
  objs[n].who = 0;
  objs[n].type = iType;
  objs[n].jump = 4;
  objs[n].period = ITEM_PER;
  objs[n].elapsedTime = objs[n].period;
  objs[n].expire = ITEM_DUR;
  objs[n].pos[0] = (rand() % (COL_SPACE / 2)) + 2;
  objs[n].pos[1] = rand() % 5;
  objs[n].pos[2] = 2;
  objs[n].pos[3] = 0;
  objs[n].next[0] = 0;
  objs[n].next[1] = 0;
  explosion(&objs[n], 2);
  objs[n].who = 4;
}

// Creates projectiles for the player based on what's equiped
void genProjectile( player *PL ) {
  if( getNewObj() == -1 ) return;
  switch((*PL).equip) {
    case 0: // PISTOL, uses 2 objects
      for(char x = 0; x < 2; ++x) {
        char n = getNewObj();
        if( n == -1 ) return;
        objs[n].exist = true;
        objs[n].collide = false;
        objs[n].who = (*PL).who;
        objs[n].type = 1;
        objs[n].jump = -1;
        objs[n].period = PISTOL_PER;
        objs[n].elapsedTime = objs[n].period;
        objs[n].expire = PISTOL_RANGE;
        objs[n].pos[0] = (*PL).pos[0];
        objs[n].pos[1] = (*PL).pos[1] + x * (*PL).aimX;
        objs[n].pos[2] = (*PL).pos[2];
        objs[n].pos[3] = (*PL).pos[3];
        objs[n].next[0] = (*PL).aimX;
        objs[n].next[1] = 0;
      }
      (*PL).cooldown = PISTOL_COOL;
      return;
    case 1: // RIFLE, uses 3 objects
      for(char x = 0; x < 3; ++x) {
        char n = getNewObj();
        if( n == -1 ) return;
        objs[n].exist = true;
        objs[n].collide = false;
        objs[n].who = (*PL).who;
        objs[n].type = 1;
        objs[n].jump = -1;
        objs[n].period = RIFLE_PER;
        objs[n].elapsedTime = objs[n].period;
        objs[n].expire = -1;
        objs[n].pos[0] = (*PL).pos[0];
        objs[n].pos[1] = (*PL).pos[1] + x * (*PL).aimX;
        objs[n].pos[2] = (*PL).pos[2];
        objs[n].pos[3] = (*PL).pos[3];
        objs[n].next[0] = (*PL).aimX;
        objs[n].next[1] = 0;
      }
      (*PL).cooldown = RIFLE_COOL;
      return;
    case 2: // RAIL/ION uses -40 objects
      for(char x = (*PL).pos[1]; x >= 0 && x <= 4; x -= (*PL).aimX) {
        char n = getNewObj();
        if( n == -1 ) return;
        objs[n].exist = true;
        objs[n].collide = false;
        objs[n].who = (*PL).who;
        objs[n].type = 1;
        objs[n].jump = -1;
        objs[n].period = RAIL_PER;
        objs[n].elapsedTime = objs[n].period;
        objs[n].expire = 2;
        objs[n].pos[0] = (*PL).pos[0];
        objs[n].pos[1] = x;
        objs[n].pos[2] = (*PL).pos[2];
        objs[n].pos[3] = (*PL).pos[3];
        objs[n].next[0] = 0;
        objs[n].next[1] = 0;
      }
      for(signed char c = (*PL).pos[0] + (*PL).aimX; c >= 0 && c <= COL_SPACE; c += (*PL).aimX) {
        for(char x = 0; x != 5; ++x) {
          char n = getNewObj();
          if( n == -1 ) return;
          objs[n].exist = true;
          objs[n].collide = false;
          objs[n].who = (*PL).who;
          objs[n].type = 1;
          objs[n].jump = -1;
          objs[n].period = RAIL_PER;
          objs[n].elapsedTime = objs[n].period;
          objs[n].expire = 2;
          objs[n].pos[0] = c;
          objs[n].pos[1] = x;
          objs[n].pos[2] = (*PL).pos[2];
          objs[n].pos[3] = (*PL).pos[3];
          objs[n].next[0] = 0;
          objs[n].next[1] = 0;
        }
      }
      (*PL).cooldown = RAIL_COOL;
      return;
    case 3: // WAVE, uses 10 obj
      for(signed char x = 0; x < 2; ++x) {  
        for(signed char y = -2; y < 3; ++y) {
          char n = getNewObj();
          if( n == -1 ) return;
          objs[n].exist = true;
          objs[n].collide = false;
          objs[n].who = (*PL).who;
          objs[n].type = 0;
          objs[n].jump = -1;
          objs[n].period = WAVE_PER;
          objs[n].elapsedTime = objs[n].period;
          if(y == 2 || y == -2)  objs[n].expire = WAVE_RANGE - 10;
          else if(y == 1 || y == -1)  objs[n].expire = WAVE_RANGE - 1;
          else objs[n].expire = WAVE_RANGE;
          objs[n].pos[0] = (*PL).pos[0];
          objs[n].pos[1] = (*PL).pos[1] + x;
          objs[n].pos[2] = (*PL).pos[2] + y;
          objs[n].pos[3] = (*PL).pos[3];
          objs[n].next[0] = (*PL).aimX;
          objs[n].next[1] = 0;
        }
      }
      (*PL).cooldown = WAVE_COOL;
      return;
    case 4: // Rocket uses 4/ 45 objs
      for(char x = 0; x < 4; ++x) {
        char n = getNewObj();
        if( n == -1 ) return;
        objs[n].exist = true;
        objs[n].collide = false;
        objs[n].who = (*PL).who;
        if(x == 0) objs[n].type = 2;
        else objs[n].type = 0;
        objs[n].jump = -1;
        objs[n].period = ROCKET_PER;
        objs[n].elapsedTime = objs[n].period;
        objs[n].expire = ROCKET_RANGE;
        objs[n].pos[0] = (*PL).pos[0];
        objs[n].pos[1] = (*PL).pos[1] + x * (*PL).aimX;
        objs[n].pos[2] = (*PL).pos[2];
        objs[n].pos[3] = (*PL).pos[3];
        objs[n].next[0] = (*PL).aimX;
        objs[n].next[1] = 0;
      }
      (*PL).cooldown = ROCKET_COOL;        
      return;
    case 5: // MGUN, uses 1 objects
    {
      char n;
      if((n = getNewObj()) == -1 ) return;
      objs[n].exist = true;
      objs[n].collide = false;
      objs[n].who = (*PL).who;
      objs[n].type = 1;
      objs[n].jump = -1;
      objs[n].period = MGUN_PER;
      objs[n].elapsedTime = objs[n].period;
      objs[n].expire = MGUN_RANGE;
      objs[n].pos[0] = (*PL).pos[0];
      objs[n].pos[1] = (*PL).pos[1] + (*PL).aimX;
      objs[n].pos[2] = (*PL).pos[2];
      objs[n].pos[3] = (*PL).pos[3];
      objs[n].next[0] = (*PL).aimX;
      objs[n].next[1] = 0;
      (*PL).cooldown = MGUN_COOL;
    }   
      return;
    case 6: // CLUSTER GRENADE
      {
        char n;
        if((n = getNewObj()) == -1 ) return;
        objs[n].exist = true;
        objs[n].collide = false;
        objs[n].who = 0;
        objs[n].type = 3;
        objs[n].jump = 5;
        objs[n].period = CLUSTER_PER;
        objs[n].elapsedTime = objs[n].period;
        objs[n].expire = CLUSTER_RANGE;
        objs[n].pos[0] = (*PL).pos[0];
        objs[n].pos[1] = (*PL).pos[1];
        objs[n].pos[2] = (*PL).pos[2];
        objs[n].pos[3] = (*PL).pos[3];
        objs[n].next[0] = (*PL).aimX;
        objs[n].next[1] = 0;
        (*PL).cooldown = CLUSTER_COOL;
      }
      return;
    case 7: // HEAL
      {
        for(signed char x = -5; x < 6; ++x) {
          char n = getNewObj();
          if( n == -1 ) return;
          objs[n].exist = true;
          objs[n].collide = false;
          objs[n].who = (*PL).who;
          objs[n].type = 0;
          objs[n].jump = -1;
          objs[n].period = 4;
          objs[n].elapsedTime = objs[n].period;
          objs[n].expire = 5;
          objs[n].pos[0] = (*PL).pos[0];
          objs[n].pos[1] = (*PL).pos[1] + x;
          objs[n].pos[2] = (*PL).pos[2];
          objs[n].pos[3] = (*PL).pos[3];
          objs[n].next[0] = 0;
          objs[n].next[1] = 0;
        }
        for(signed char y = -5; y < 6; ++y) {
          char n = getNewObj();
          if( n == -1 ) return;
          objs[n].exist = true;
          objs[n].collide = false;
          objs[n].who = (*PL).who;
          objs[n].type = 0;
          objs[n].jump = -1;
          objs[n].period = 4;
          objs[n].elapsedTime = objs[n].period;
          objs[n].expire = 5;
          objs[n].pos[0] = (*PL).pos[0];
          objs[n].pos[1] = (*PL).pos[1];
          objs[n].pos[2] = (*PL).pos[2] + y;
          objs[n].pos[3] = (*PL).pos[3];
          objs[n].next[0] = 0;
          objs[n].next[1] = 0;
        }
        (*PL).invul = HEAL_INVUL;
        (*PL).life++;
        char n;
        if((n = getNewObj()) == -1 ) return;
        objs[n].exist = true;
        objs[n].who = 3;
        objs[n].pos[0] = (*PL).pos[0];
        objs[n].pos[1] = (*PL).pos[1];
        objs[n].pos[2] = (*PL).pos[2];
        objs[n].pos[3] = (*PL).pos[3];
        objs[n].next[0] = 0;
        objs[n].next[1] = 0;
        explosion(&(objs[n]),2);
        objs[n].exist = true;
        (*PL).cooldown = HEAL_COOL;  
      }  
      return;
    default:
      break;
  }
}

void swapItem( player *PL ) {
  (*PL).equip++;
  if((*PL).equip == NUM_EQUIP)
      (*PL).equip = 0;
  for( ; (*PL).inven[(*PL).equip] == 0; ) { // if consecutive items are empty, keep swapping
    (*PL).equip++;
    if((*PL).equip == NUM_EQUIP)
      (*PL).equip = 0;
  }
}

void swapItemR( player *PL ) {
  (*PL).equip--;
  if((*PL).equip == -1)
      (*PL).equip = NUM_EQUIP - 1;
  for( ; (*PL).inven[(*PL).equip] == 0; ) { // if consecutive items are empty, keep swapping
    (*PL).equip--;
    if((*PL).equip == -1)
      (*PL).equip = NUM_EQUIP - 1;
  }
}

void handlePLInput( player *PL, char input ) {
  if((*PL).jump) {
    (*PL).next[1] = 1;
    (*PL).jump--;
  } else 
    (*PL).next[1] = -1;
  switch(input) { 
    case '\0':
      (*PL).autofire = 0; 
      (*PL).next[0] = 0;
      break;
    case 'D':
      (*PL).autofire = 0; 
      (*PL).next[0] = 1;
      (*PL).aimX = 1;
      break;
    case '0':
      (*PL).autofire = 0; 
      (*PL).next[0] = -1;
      (*PL).aimX = -1;
      break;
    case '9':
      (*PL).autofire = 0; 
      (*PL).jump = 2;
      break;
    case '8':
      if((*PL).jump)
        (*PL).jump = 2;
      if((*PL).autofire != 2) 
        swapItemR(PL);
      (*PL).autofire = 2; 
      break;
    case 'C':
      if((*PL).jump)
        (*PL).jump = 2;
      if((*PL).autofire != 3) 
        swapItem(PL);
      (*PL).autofire = 3; 
      break;
    case '4':
    case '7':
    case '*':
      if((*PL).jump)
        (*PL).jump = 2;
      if((*PL).cooldown == 0 && (*PL).autofire != 1) {
        if((*PL).inven[(*PL).equip] > 0 )
          (*PL).inven[(*PL).equip]--;
        genProjectile(PL); // Allocates a projectile, updates on next turn
        if((*PL).inven[(*PL).equip] == 0)
          swapItem(PL);
        (*PL).autofire = 1; // Prevent autofiring
        if((*PL).equip == 5) // MGUN bypasses any autofiring limitations
          (*PL).autofire = 0;
      }
      break;
    default:
      break;
  }
}

void evalStats( player *PL ) {
  setCursor(8, ((*PL).who - 1));
  write(255);

  write((*PL).who + '0');
  write((*PL).pos[0] + (*PL).pos[3] * 4);

  write('L');
  if((*PL).life > 9) write(43); // plus sign
  else write((*PL).life + '0');

  if ((*PL).cooldown / 2 > 9) write(43); // plus sign
  else if((*PL).cooldown == 0) write('!');
  else write((*PL).cooldown / 2  + '0');

  switch((*PL).equip) {
    case 0:
      write('P');
      break;
    case 1:
      write('F');
      break;
    case 2:
      write('I');
      break;
    case 3:
      write('W');
      break;
    case 4:
      write('R');
      break;
    case 5:
      write('M');
      break;
    case 6:
      write('C');
      break;
    case 7:
      write('H');
      break;
    default:
      write('?');
      break;
  }
  if((*PL).inven[(*PL).equip] == -1) write(243); // infinity sign
  else if((*PL).inven[(*PL).equip] > 9) write(43); // plus sign
  else write((*PL).inven[(*PL).equip] + '0');
}

void map() {
  if(ROW_SPACE == 1) {
    setCursor(0, 1);
    print("        ");
    setCursor(P1.pos[0], 1);
    write('1');
    setCursor(P2.pos[0], 1);
    write('2');
    for(char n = 0; n < NUM_OBJ; ++n)
      if(objs[n].exist && objs[n].who == 4) {
        setCursor(objs[n].pos[0], 1);
        switch(objs[n].type) {
          case 0:
            write('P');
            break;
          case 1:
            write('F');
            break;
          case 2:
            write('I');
            break;
          case 3:
            write('W');
            break;
          case 4:
            write('R');
            break;
          case 5:
            write('M');
            break;
          case 6:
            write('C');
            break;
          case 7:
            write('H');
            break;
          default:
            write('?');
            break;
        }
      }

  } else if(ROW_SPACE == 2) {
    setCursor(COL_SPACE, 0);
    print("    ");
    setCursor(COL_SPACE, 1);
    print("    ");
    setCursor(P1.pos[0] + COL_SPACE, P1.pos[3] );
    write('1');
    setCursor(P2.pos[0] + COL_SPACE, P2.pos[3]);
    write('2');  
  }
}

void victory(char who) {
  switch(who) {
    case 0:
      setCursor(0, 1);
      print("DRAW    ");
      break;
    case 1:
      setCursor(0, 1);
      print("P1 WINS!");
      break;
    case 2:
      setCursor(0, 1);
      print("P2 WINS!");
      break;
  }
}

//============================================================================================================================================
// State Machines:

// System SM, Also sets LCD_State on player win
enum Sys_States { Sys_init, Sys_boot, Sys_seed, Sys_play, Sys_reset, Sys_reset2, Sys_P1win, Sys_P2win, Sys_tie } Sys_State;
int Sys_Tick(int Sys_State) {
  static bool seeded;
  static unsigned char seed_val;
  switch(Sys_State) { // Transitions
    case -1:
      Sys_State = Sys_boot;
      pause = 0;
      seeded = false;
      seed_val = 0;
      break;
    case Sys_init:
      if(pause == 0) {
        Sys_State = Sys_boot;
        pause = 1;
      } else 
        Sys_State = Sys_init;
      break;
    case Sys_boot:
      if(seeded == false) {
        pause = 1;
        Sys_State = Sys_seed;
      }
      else {
        Sys_State = Sys_play;
        pause = 0;
      }
      break;
    case Sys_seed:
      if(GetBit(~PINB,0)) {
        Sys_State = Sys_play;
        srand(seed_val);
        pause = 0;
        seeded = true;
        setCursor(0, 0);
        print("Seeded");
      }
      else {
        Sys_State = Sys_seed;
        setCursor(0, 0);
        print("Press Reset");
        setCursor(0, 1);
        print("to Begin");
      }
      break;
    case Sys_play:
      if(P1.life == 0 && P2.life == 0)
        Sys_State = Sys_tie;  
      else if(P1.life == 0)
        Sys_State = Sys_P2win;  
      else if(P2.life == 0)
        Sys_State = Sys_P1win;  
      else if(GetBit(~PINB,0)) // if B0, start reset
        Sys_State = Sys_reset;
      else
        Sys_State = Sys_play;
      break;
    case Sys_reset:
      if(!GetBit(~PINB,0)) // if !B0, confirm reset
        Sys_State = Sys_reset2;
      else 
        Sys_State = Sys_reset;
      break;
    case Sys_reset2:
      Sys_State = Sys_init;
      break;
    case Sys_P1win:
      pause = 1;
      if(GetBit(~PINB,0)) // if B0, start reset
        Sys_State = Sys_reset;
      break;
    case Sys_P2win:
      pause = 1;
      if(GetBit(~PINB,0)) // if B0, start reset
        Sys_State = Sys_reset;
      break;
    case Sys_tie:
      pause = 1;
      if(GetBit(~PINB,0)) // if B0, start reset
        Sys_State = Sys_reset;
      break;
    default:
      Sys_State = Sys_reset2;
      break;
  } // Transitions end
  switch(Sys_State) { // State actions
    case Sys_init:
      break;
    case Sys_boot:
      initObjs();
      initPlayers();
      setCursor(0, 0); 
      pause = 0;
      break;
    case Sys_seed:
      ++seed_val;
      break;
    case Sys_play:
      pause = 0;
      break;
    case Sys_reset:
      pause = 1;
      break;
    case Sys_reset2:
      pause = 0;
      break;
    case Sys_P1win:
      victory(1);
      break;
    case Sys_P2win:
      victory(2);
      break;
    case Sys_tie:
      victory(0);
      break;
    default:
      break;
  } // State actions end
  return Sys_State;
} 

// LCD SM
enum LCD_States { LCD_init, LCD_update, LCD_pause } LCD_State;
int LCD_Tick(int LCD_State) {
  static unsigned char i;
  switch(LCD_State) { // Transitions
    case -1:
      LCD_State = LCD_init;
      cmdWrite(0x01);
      i = 0;
      break;
    case LCD_init:
      if (i<100)
        LCD_State = LCD_init;
      else {
        LCD_State = LCD_update;
        initLCD();
      }
      break;
    case LCD_update:
      LCD_State = LCD_update;
      break;
    default:
      break;
  } // Transitions end
  switch(LCD_State) { // State actions
    case LCD_init:
      i++;
      break;
    case LCD_update:
      if(pause == 0) {
        evalField();
        evalStats(&P1);
        evalStats(&P2);
        map();
      }
      break;
    default:
      break;
  } // State actions end
  return LCD_State;
} 

// P1 SM
enum P1_States { P1_stand } P1_State;
int P1_Tick(int P1_State) {
  switch(P1_State) { // Transitions
    case -1:
      P1_State = P1_stand;
      break;
    case P1_stand:
      P1_State = P1_stand;
      break;
    default:
      P1_State = -1;
  } // Transitions end
  switch(P1_State) { // State actions
    case P1_stand:
      handlePLInput(&P1,GetKeypadKeyP1());
      break;
    default:
      break;
  } // State actions end
  return P1_State;
}

// P2 SM
enum P2_States { P2_stand } P2_State;
int P2_Tick(int P2_State) {
  switch(P2_State) { // Transitions
    case -1:
      P2_State = P2_stand;
      break;
    case P2_stand:
      P2_State = P2_stand;
      break;
    default:
      P2_State = -1;
  } // Transitions end
  switch(P2_State) { // State actions
    case -1:
      break;
    case P2_stand:
      handlePLInput(&P2,GetKeypadKeyP2());
      break;
    default:
      break;
  } // State actions end
  return P2_State;
}

// Item SM
enum Item_States { Item_wait, Item_gen, Item_event } Item_State;
int Item_Tick(int Item_State) {
  static unsigned short c;
  static signed char event;
  switch(Item_State) { // Transitions
    case -1:
      Item_State = Item_wait;
      c = 0;
      event = -1;
      break;
    case Item_wait:
      if(c > EVENT_INTERVAL) {
        Item_State = Item_gen;  
        genItem((rand() % (NUM_EQUIP - 1)) + 1);
      } else 
        Item_State = Item_wait;
      break;
    case Item_gen:
      Item_State = Item_wait;
      break;
    case Item_event:
      if(c == 0) {
        Item_State = Item_wait;
        event = -1;
      } else
        Item_State = Item_event;
      break;
    default:
      Item_State = -1;
  } // Transitions end
  switch(Item_State) { // State actions
    case Item_wait:
      switch(event) {
        case 1:
          if((c % 3) == 0)
            genEvent(1);
          break;
        case 2:
          if(c < 200) {
            setCursor(0, 1);
            print("LAVA INC");
          } else {
            if((c % 30) == 0)
              genEvent(2);
          }
          break;
        case 3:
          if(c < 200) {
            setCursor(0, 1);
            print("LAV&RAIN");
          } else {
            if((c % 30) == 0)
              genEvent(2);
            if((c % 3) == 0)
              genEvent(1);
          }
          break;    
        default:
          break;
      }
      if(pause == 0) 
        ++c;
      else if(pause == 1) {
        c = 0;
        event = -1;
      }
      break;
    case Item_gen:
      switch(rand() % MOD_CHANCE) {
        case 0:
          genEvent(0);
          event = -1;
          break;
        case 1:
          event = 1;
          break;
        case 2:
          event = 2;
          break;
        case 3:
          event = 3;
          break;  
        default:
          event = -1;
          break;
      }
      c = 0;
      break;
    default:
      break;
  } // State actions end
  return Item_State;
}

//============================================================================================================================================

int main() {
  DDRA = 0xF0; PORTA = 0x0F; // input from Keypad1
  DDRB = 0xFE; PORTB = 0x01; // input B0-reset, out-B1/B2-speaker(?)
  DDRC = 0xF0; PORTC = 0x0F; // input from Keypad2
  DDRD = 0xFF; PORTD = 0x00; // output to LCD
  
  static task task_System, task_LCD, task_Player1, task_Player2, task_Item;
  player *player[] = {&P1, &P2};
  task *tasks[] = {&task_System, &task_Player1, &task_Player2, &task_LCD, &task_Item};
  int numTasks = sizeof(tasks)/sizeof(task*);

  task_System.state = -1;
  task_System.period = GAME_PER;
  task_System.elapsedTime = task_System.period;
  task_System.TickFct = &Sys_Tick;

  task_LCD.state = -1;
  task_LCD.period = GAME_PER;
  task_LCD.elapsedTime = task_LCD.period;
  task_LCD.TickFct = &LCD_Tick;

  task_Player1.state = -1;
  task_Player1.period = GAME_PER;
  task_Player1.elapsedTime = task_Player1.period;
  task_Player1.TickFct = &P1_Tick;

  task_Player2.state = -1;
  task_Player2.period = GAME_PER;
  task_Player2.elapsedTime = task_Player2.period;
  task_Player2.TickFct = &P2_Tick;

  task_Item.state = -1;
  task_Item.period = GAME_PER;
  task_Item.elapsedTime = task_Item.period;
  task_Item.TickFct = &Item_Tick;

  //============================================================================================================================================

  TimerSet(CLK_PER);
  TimerOn();
  
  unsigned short i; // Scheduler for-loop iterator
  while(1) {
    // Scheduler code
    for ( i = 0; i < numTasks; i++ ) {
      // Task is ready to tick
      if ( tasks[i]->elapsedTime == tasks[i]->period ) {
        // Setting next state for task
        tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
        // Reset the elapsed time for next tick.
        tasks[i]->elapsedTime = 0;
      }
      tasks[i]->elapsedTime += CLK_PER;
    }
    while(!TimerFlag);
    TimerFlag = 0;
  }
}
