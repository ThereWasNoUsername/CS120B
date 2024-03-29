/*
 * achen115_project.c
 *
 * Created: 5/8/2019 3:24:34 PM
 * Author : Alex
 */ 

#include <avr/eeprom.h>
#include <avr/io.h>
#include "nokia5110.h"
#include "nokia5110.c"
#include "timer.h"
#include "io.c"
#include "uart.c"

#define FOR_TETRA for(unsigned short i = 0; i < 4; i++)
#define WIDTH 12
#define HEIGHT 21
#define DDR_BUTTONS DDRD
#define PIN_BUTTONS PIND
#define TIMER_INTERVAL 10

char getInput() {
	/*
	if(pressed == UART_NO_DATA) {
		pressed = 0;
		} else {
		pressed = pressed & 0xFF;
	}
	char ch = (char) pressed;
	*/
	return ~PINA & 7;
}

//Custom character info: https://www.engineersgarage.com/embedded/avr-microcontroller-projects/lcd-interface-atmega16-circuit
void InitializeChar(char index, char rows[]) {
	LCD_WriteCommand(0x40 + index*8);	//Start writing custom character
	//Write each row
	for(short i = 0; i < 8; i++) {
		LCD_WriteData(rows[i]);
	}
}
#define CHAR_TETRA_T 0
#define CHAR_TETRA_L 1
#define CHAR_TETRA_Z 2
#define CHAR_TETRA_I 3
#define CHAR_TETRA_O 4
void InitializeCustom() {
	//Rows of dots
	char T[] = {
		0x00,
		0x00,
		0x18,
		0x18,
		0x1E,
		0x1E,
		0x18,
		0x18,
	};
	InitializeChar(0, T);

	char L[] = {
		0x00,
		0x00,
		0x18,
		0x18,
		0x18,
		0x18,
		0x1E,
		0x1E,
	};
	InitializeChar(1, L);

	char Z[] = {
		0x00,
		0x00,
		0x06,
		0x06,
		0x1E,
		0x1E,
		0x18,
		0x18,
	};
	InitializeChar(2, Z);

	char I[] = {
		0x18,
		0x18,
		0x18,
		0x18,
		0x18,
		0x18,
		0x18,
		0x18
	};
	InitializeChar(3, I);

	char O[] = {
		0x00,
		0x00,
		0x00,
		0x00,
		0x1E,
		0x1E,
		0x1E,
		0x1E
	};
	InitializeChar(4, O);
}
//A vector of two short integers
typedef struct Point {
	signed short x, y;
} Point;
//Returns true if the point's x component is within the fixed width of the grid
char inWidth(Point *p) {
	return p->x > -1 && p->x < WIDTH;
}
//Adds two points together componentwise
Point add(Point *p1, Point *p2) {
	Point result;
	result.x = p1->x + p2->x;
	result.y = p1->y + p2->y;
	return result;
}
// Rotates a point 90 degrees CCW around the origin
// We store the Tetromino tiles as relative positions
// So we can use a neat math trick to do this
// tx = x
// x = -y
// y = tx
// ( 1, 0)  ( 0, 1)  (-1, 0)  ( 0,-1)
//   ...      .+.      ...      ...
//   ..+      ...      +..      ...
//   ...      ...      ...      .+.
void pivotCCW(Point* p) {
	signed short x = p->x;
	p->x = -p->y;
	p->y = x;
}

// Rotates a point 90 degrees CW around the origin
// We store the Tetromino tiles as relative positions
// So we can use a neat math trick to do this
// ty = y
// y = -x
// x = ty
// ( 1, 0)  ( 0,-1)  (-1, 0)  ( 0, 1)
//   ...      ...      ...      .+.
//   ..+      ...      +..      ...
//   ...      .+.      ...      ...
void pivotCW(Point* p) {
	signed short y = p->y;
	p->y = -p->x;
	p->x = y;
}

//A tetromino, a set of four adjacent points around a position
typedef struct Tetra {
	Point pos;			//Absolute position
	Point tiles[4];		//Relative to pos
} Tetra;
//	nn   n    ne   e    se   s    sw   w    nw   c
//  .+.  ...  ...  ...  ...  ...  ...  ...  ...  ...
//  ...  .+.  ..+  ...  ...  ...  ...  ...  +..  ...
//  ...  ...  ...  ..+  ...  ...  ...  +..  ...  .+.
//  ...  ...  ...  ...  ..+  .+.  +..  ...  ...  ...
//Tetromino tile templates
#define nn (Point) { .x = 0, .y = 2 }	//North North
#define n (Point) { .x = 0, .y = 1 }	//North
#define ne (Point) { .x = 1, .y = 1 }	//Northeast
#define e (Point) { .x = 1, .y = 0 }	//East
#define se (Point) { .x = 1, .y = -1 }	//Southeast
#define s (Point) { .x = 0, .y = -1 }	//South
#define sw (Point) { .x = -1, .y = -1 }	//Southwest
#define w (Point) { .x = -1, .y = 0 }	//West
#define nw (Point) { .x = -1, .y = 1 }	//Northwest
#define c (Point) { .x = 0, .y = 0 }	//Center
// Tetromino templates
//  ...  ...  ...  .+.  ...
//  ...  .+.  .++  .+.  .++
//  +++  .+.  ++.  .+.  .++
//  .+.  .++  ...  .+.  ...
// Points are copied on assignment
Tetra T(Point pos) { return (Tetra) {.pos = pos, .tiles = {w, c, e, s}}; }
Tetra L(Point pos) { return (Tetra) {.pos = pos, .tiles = {n, c, s, se}}; }
Tetra Z(Point pos) { return (Tetra) {.pos = pos, .tiles = {w, c, n, ne}}; }
Tetra I(Point pos) { return (Tetra) {.pos = pos, .tiles = {s, c, n, nn}}; }
Tetra O(Point pos) { return (Tetra) {.pos = pos, .tiles = {s, c, e, se}}; }
//Returns a copy of an absolute point on the tetromino
Point getTile(Tetra *t, unsigned short index) {
	return add(&t->pos, &t->tiles[index]);
}
//Decrements the tetromino position without bounds checking
void down(Tetra *t) {
	t->pos.y--;
}
//Movs the tetromino position right without bounds checking
void right(Tetra *t) {
	t->pos.x++;
}
//Movs the tetromino position left without bounds checking
void left(Tetra *t) {
	t->pos.x--;
}
//Flips the tetromino tiles across X without bounds checking
void mirror(Tetra* t) {
	for(unsigned short i = 0; i < 4; i++) {
		t->tiles[i].x = -t->tiles[i].x;
	}
}
//Rotates the tetromino tiles around the center without bounds checking
void turnCCW(Tetra *t) {
	FOR_TETRA {
		pivotCCW(&t->tiles[i]);
	}
}
//Rotates the tetromino tiles around the center without bounds checking
void turnCW(Tetra *t) {
	FOR_TETRA {
		pivotCW(&t->tiles[i]);
	}
}
//Creates a random tetromino
Tetra CreateTetra() {
	char x = WIDTH/2 + rand()%8 - 4;
	Point pos = {.x = x, .y = HEIGHT + 2};
	Tetra t;
	switch(rand()%5) {
		case 0: t = T(pos); break;
		case 1: t = L(pos); break;
		case 2: t = Z(pos); break;
		case 3: t = I(pos); break;
		case 4: t = O(pos); break;
	}
	switch(rand()%3) {
		case 0:
		turnCCW(&t);
		break;
		case 1:
		turnCW(&t);
		break;
		case 2:
		default:
		break;
	}
	if(rand()%2) {
		mirror(&t);
	}
	return t;
}
//A wrapper for a 2D array of fixed width and height
typedef struct Grid {
	unsigned char tiles[WIDTH][HEIGHT];
} Grid;
//Returns the value of the point on the grid.
//Returns 1 if filled and 0 if empty
char getPoint(Grid *g, Point *p) {
	return g->tiles[p->x][p->y];
}
//Clears all tiles on the given row, no bounds checking
void clearRow(Grid *g, short y) {
	for(short x = 0; x < WIDTH; x++) {
		g->tiles[x][y] = 0;
	}
}
//Clears all the grid spaces
void clear(Grid *g) {
	for(short x = 0; x < WIDTH; x++) {
		for(short y = 0; y < HEIGHT; y++) {
			g->tiles[x][y] = 0;
		}
	}
}
//Fills all the grid spaces
void fill(Grid *g) {
	for(short x = 0; x < WIDTH; x++) {
		for(short y = 0; y < HEIGHT; y++) {
			g->tiles[x][y] = 1;
		}
	}
}
//Fills all tiles on the given row, no bounds checking
void fillRow(Grid *g, short y) {
	for(short x = 0; x < WIDTH; x++) {
		g->tiles[x][y] = 1;
	}
}
//Starting at this row and going up, overwrites the current row with the one above it
void descendRow(Grid *g, short start) {
	for(short y = start; y + 1 < HEIGHT; y++) {
		for(short x = 0; x < WIDTH; x++) {
			g->tiles[x][y] = g->tiles[x][y+1];
		}
	}
	clearRow(g, HEIGHT - 1);
}
//Returns whether the row is completely filled, no bounds checking
char rowFull(Grid *g, short y) {
	for(short x = 0; x < WIDTH; x++) {
		if(!g->tiles[x][y]) {
			return 0;
		}
	}
	return 1;
}
//Returns whether the row is completely empty, no bounds checking
char rowEmpty(Grid *g, short y) {
	for(short x = 0; x < WIDTH; x++) {
		if(g->tiles[x][y]) {
			return 0;
		}
	}
	return 1;
}
//Returns whether the point is in bounds of the grid
char inBoundsPoint(Grid *g, Point *p) {
	return p->x > -1 && p->x < WIDTH && p->y > -1 && p->y < HEIGHT;
}
//Returns whether the tetromino and all of its tiles are in bounds of the grid AND the grid is empty for all the points
char inBoundsOpen(Grid *g, Tetra *t) {
	FOR_TETRA {
		Point p = getTile(t, i);
		if(!inBoundsPoint(g, &p) || getPoint(g, &p)) {
			return 0;
		}
	}
	return 1;
}
//Returns whether the tetromino has reached the bottom or has a filled tile beneath it on the grid.
char land(Grid *g, Tetra *t) {
	FOR_TETRA {
		//Copy the world point from the tetromino
		Point p = getTile(t, i);
		//Land if we reach the bottom
		if(p.y < 1) {
			return 1;
		} else {
			//Land if there's a tile right below us
			p.y--;
			if(inBoundsPoint(g, &p) && getPoint(g, &p)) {
				return 1;
			}
		}
	}
	return 0;
}
//Mirrors the tetromino, reverting the operation if it goes out of bounds
void tryMirror(Grid *g, Tetra *t) {
	//Try it
	mirror(t);
	//If it works, we're done
	if(inBoundsOpen(g, t)) {
		return;
	}
	//Otherwise, revert
	mirror(t);
}
//Turns the Tetromino 90 degrees CCW, reverting the operation if it goes out of bounds
void tryTurnCCW(Grid *g, Tetra *t) {
	//Try it
	turnCCW(t);
	//If it works, we're done
	if(inBoundsOpen(g, t)) {
		return;
	}
	//Otherwise, revert
	turnCW(t);
}
//Turns the Tetromino 90 degrees CW, reverting the operation if it goes out of bounds
void tryTurnCW(Grid *g, Tetra *t) {
	//Try it
	turnCW(t);
	//If it works, we're done
	if(inBoundsOpen(g, t)) {
		return;
	}
	//Otherwise, revert
	turnCCW(t);
}
//Shifts the Tetromino right, reverting the operation if it goes out of bounds
void tryShiftRight(Grid *g, Tetra *t) {
	right(t); //Try it
	//See if it worked
	if(inBoundsOpen(g, t)) {
		return;
	}
	left(t);	//Otherwise go back
}
//Shifts the Tetromino right, reverting the operation if it goes out of bounds
void tryShiftLeft(Grid *g, Tetra *t) {
	left(t); //Try it
	//See if it worked
	if(inBoundsOpen(g, t)) {
		return;
	}
	right(t);	//Otherwise go back
}
//Sets the Tetromino's tiles in the grid to solid
void place(Grid *g, Tetra *t) {
	FOR_TETRA {
		Point p = getTile(t, i);

		if(inBoundsPoint(g, &p)) {
			g->tiles[p.x][p.y] = 1;
		}
	}
}
/*
//Sets the Tetromino's tiles in the grid to empty
void remove(Grid *g, Tetra *t) {
	FOR_TETRA {
		Point p = getTile(t, i);
		if(inBoundsPoint(g, &p)) {
			g->tiles[p.x][p.y] = 0;
		}
	}
}
*/
//The current state of the screen
typedef enum ScreenState { Title = 0, Game = 1, FinalScore = 2 } ScreenState;
ScreenState screenState;

//The current state of the game
typedef enum GameState { Init, Load, Play, PlayInterval, RowClear, GameOver, GameOverFlash } GameState;
GameState gameState;

unsigned char score = 0; //The current score for the game
unsigned char highScore = 0; //The running high score
unsigned char ADDR_STATE = 0x20;	//Unused: the location of the screen state
unsigned char ADDR_GAME = 0xA0;		//Unused: the location of the game state
unsigned char ADDR_SCORE = 0xFF;	//The location of the high score
unsigned char rnd = 0;	//The Random Seed value
//Counts the digits in the number
short getLength(short sh) {
	unsigned char length = 1;
	//Count the number in case it is 0
	if(sh == 0) {
		length++;
	} else {
		while(sh > 0) {
			length++;
			sh /= 10;
		}
	}
	return length;
}
//Converts the number to a string
void getString(short sh, char* str, short length) {
	length--;
	str[length] = 0;
	//Print the number in case it is 0
	if(sh == 0) {
		length--;
		str[length] = '0';
	} else {
		while(sh > 0) {
			length--;
			str[length] = '0' + (sh%10);
			sh /= 10;
		}
	}
	return;
}
//The states of the music
typedef enum SoundState { MusicTitle = 1, MusicTypeA = 2, MusicGameOver = 3, MusicHighScore = 4 } SoundState;
//Sets the screen state and resets the music if transitioning to another state
void SetScreenState(ScreenState next) {
	screenState = next;
	switch(next) {
		case Title:
			uart_putc(MusicTitle);
			break;
		case Game:
			uart_putc(MusicTypeA);
			break;
		case FinalScore:
			uart_putc(MusicHighScore);
			break;
	}
}
//Updates the title screen
void UpdateTitle() {
	//Write the title
	nokia_lcd_clear();
	nokia_lcd_write_string("Tetris!", 1);

	//Write the high score
	nokia_lcd_set_cursor(0, 10);
	nokia_lcd_write_string("High score: ", 1);
	nokia_lcd_set_cursor(0, 20);
	//Get the high score as a string
	unsigned short highScoreLength = getLength(highScore);
	char highScoreString[highScoreLength];
	getString(highScore, highScoreString, highScoreLength);
	nokia_lcd_write_string(highScoreString, 1);
	//Write the Random Seed
	nokia_lcd_set_cursor(0, 30);
	nokia_lcd_write_string("Random seed:", 1);
	//Get the Random Seed as a string
	unsigned short rndLength = getLength(rnd);
	char rndString[rndLength];
	getString(rnd, rndString, rndLength);

	nokia_lcd_set_cursor(0, 40);
	nokia_lcd_write_string(rndString, 1);

	nokia_lcd_render();

	//Write the Tetra sequence to the LCD
	srand(rnd);
	for(short i = 0; i < 32; i++) {
		LCD_Cursor(i+1);
		LCD_WriteData(rand()%5);
	}

	//char pressed = ((~PIN_BUTTONS) >> 2) & 7;
	char pressed = getInput();
	static char pressed_prev = 0;
	char justPressed = pressed & ~pressed_prev;
	//Check for input
	switch(pressed) {
		case 1:	//Right
			//Increment the seed
			rnd++;
			break;
		case 2:	//Middle
			//Start the game
			if(justPressed == 2) {
				//screenState = Game;
				SetScreenState(Game);
				gameState = Init;
			}
			break;
		case 3:	//Middle + Right
			break;
		case 4:	//Left
			//Decrement the seed
			rnd--;
			break;
		case 5:	//Left + Right
			//Randomize the seed
			srand(rnd);
			rnd = rand();
			break;
		case 6:	//Left + Middle
			
			break;
		case 7:	//Left + Middle + Right
			//Erase the high score
			highScore = 0;
			eeprom_write_byte(ADDR_SCORE, highScore);
			break;
	}
	pressed_prev = pressed;
}
void UpdateFinalScore() {

	static short time = 500;
	if((time -= TIMER_INTERVAL) > 0) {
		return;
	}
	time = 500;

	//Print the final score
	nokia_lcd_clear();
	nokia_lcd_write_string("Your score: ", 1);
	nokia_lcd_set_cursor(0, 10);
	//Get the final score as a string
	unsigned short scoreLength = getLength(score);
	char scoreString[scoreLength];
	getString(score, scoreString, scoreLength);
	nokia_lcd_write_string(scoreString, 1);
	//Print the high score
	nokia_lcd_set_cursor(0, 20);
	nokia_lcd_write_string("High score: ", 1);
	nokia_lcd_set_cursor(0, 30);
	//Get the high score as a string
	unsigned short highScoreLength = getLength(highScore);
	char highScoreString[highScoreLength];
	getString(highScore, highScoreString, highScoreLength);
	nokia_lcd_write_string(highScoreString, 1);

	nokia_lcd_set_cursor(0, 40);

	//The player got a new high score this game
	if(score > highScore) {
		static char flash = 1;
		flash = !flash;
		if(flash)
			nokia_lcd_write_string("New high score!", 1);	
	}

	nokia_lcd_render();


	//char pressed = ((~PIN_BUTTONS) >> 2) & 7;
	char pressed = getInput();
	static char pressed_prev = 7;
	char justPressed = pressed & ~pressed_prev;
	//Check for input
	switch(justPressed) {
		case 0: break;
		default:
			//Press any button

			//Save the high score
			highScore = highScore > score ? highScore : score;
			eeprom_write_byte(ADDR_SCORE, highScore);
			//Go back to title screen
			//screenState = Title;
			SetScreenState(Title);
			break;
	}
	pressed_prev = pressed;
}
//Draws a world tile as a 4x4 block on a vertical screen
//x: the x position on the grid
//y: the y position on the grid
//fill: whether the tile is filled or empty
void drawTile(short x, short y, short fill) {
	for(short xi = 0; xi < 4; xi++) {
		for(short yi = 0; yi < 4; yi++) {
			nokia_lcd_set_pixel(y*4 + yi, x*4 + xi, fill);
		}
	}
}
#define STANDARD_INTERVAL 100;
//Unused: loads the saved state from EEPROM
void LoadState() {
	//Load the high score
	highScore = eeprom_read_byte(ADDR_SCORE);

	//Load the screen state
	screenState = eeprom_read_byte(ADDR_STATE);

	//In case we're loading a game in progress
	if(screenState == Game)
		gameState = Load;
}
//Unused: loads the saved game from EEPROM
void LoadGame(Grid *g, Tetra *t) {
	gameState = Play;
	unsigned char address = ADDR_GAME;
	//Load the score
	score = eeprom_read_byte(address);
	address++;
	//Load the rng
	rnd = eeprom_read_byte(address);
	srand(rnd);
	address++;
	//Load sections of the grid as chars
	unsigned char block = eeprom_read_byte(address);
	address++;
	unsigned char blockIndex = 0;
	for(short x = 0; x < WIDTH; x++) {
		for(short y = 0; y < HEIGHT; y++) {
			g->tiles[x][y] = (block >> blockIndex) & 1;
			if(++blockIndex == 8) {
				block = eeprom_read_byte(address);
				address++;
				blockIndex = 0;
			}
		}
	}

	//Store the tetra coordinates each as bytes
	t->pos.x = eeprom_read_byte(address);
	address++;
	t->pos.y = eeprom_read_byte(address);
	address++;
	for(short i = 0; i < 4; i++) {
		t->tiles[i].x = eeprom_read_byte(address);
		address++;
		t->tiles[i].y = eeprom_read_byte(address);
		address++;
	}
}
//Unused: saves the current state in EEPROM
void SaveState() {
	//Save the high score
	eeprom_write_byte(ADDR_SCORE, highScore);
	eeprom_write_byte(ADDR_STATE, screenState);
}
//Unused: saves the current game in EEPROM
void SaveGame(Grid *g, Tetra *t) {
	//We're saving a game in progress
	unsigned char address = ADDR_GAME;

	//Save the score
	eeprom_write_byte(address, score);
	address++;

	//Save the RNG
	eeprom_write_byte(address, rnd);
	address++;

	//Store sections of the grid as chars
	unsigned char block = 0;
	unsigned char blockIndex = 0;
	for(short x = 0; x < WIDTH; x++) {
		for(short y = 0; y < HEIGHT; y++) {
			//Store in block
			block |= (g->tiles[x][y]) << blockIndex;
			if(++blockIndex == 8) {
				//Save the block and create a new one
				eeprom_write_byte(address, block);
				block = 0;
				blockIndex = 0;
				address++;
			}
		}
	}
	//Write the last block
	eeprom_write_byte(address, block);
	address++;

	//Store the tetra coordinates each as bytes
	eeprom_write_byte(address, t->pos.x);
	address++;
	eeprom_write_byte(address, t->pos.y);
	address++;
	for(short i = 0; i < 4; i++) {
		eeprom_write_byte(address, t->tiles[i].x);
		address++;
		eeprom_write_byte(address, t->tiles[i].y);
		address++;
	}
}
//Updates the game
void UpdateGame() {
	static short standardInterval = STANDARD_INTERVAL;

	static Grid g;					//The current state of the playing field 
	static Tetra t;					//The current Tetra we are dropping

	static char pressed_prev = 0;
	static char hard_drop = 0;		//How many Tetras we hard dropped in a row

	static short rowCleared = 0;	//The row that we are clearing (only for state RowClear)
	static char rowState = 0;		//The number of times the cleared row is flashing
	static short placed = 0;		//Debug: How many Tetras have been placed
	static short fall = 0;			//Debug: How far the Tetra fell

	static short time = 0;	//Time until next update

	if((time -= TIMER_INTERVAL) > 0) {
		return;
	}
	switch(gameState) {
	case Init: {
		clear(&g);
		t = CreateTetra();

		pressed_prev = 0;
		hard_drop = 0;

		rowCleared = 0;
		rowState = 0;
		placed = 0;
		fall = 0;

		standardInterval = STANDARD_INTERVAL;
		time = standardInterval;
		//Reset the RNG
		srand(rnd);
		//Reset the score
		score = 0;
		gameState = Play;
		//Reset all static variables so that they don't carry over from previous games
		break;

	} case Load:{
		//Initialize before loading
		fill(&g);
		t = CreateTetra();

		pressed_prev = 0;
		hard_drop = 0;

		rowCleared = 0;
		rowState = 0;
		placed = 0;
		fall = 0;

		standardInterval = STANDARD_INTERVAL;
		time = standardInterval;
		//Reset the RNG
		srand(rnd);
		//Reset the score
		score = 0;
		gameState = Play;
		LoadGame(&g, &t);
		break;
	} case Play: {
		//remove(&g, &t);			//Remove so that we can move

		//char pressed = ((~PIN_BUTTONS) >> 2) & 7;
		char pressed = getInput();
		//char justPressed = pressed & ~pressed_prev;
		switch(pressed) {
		case 1:	//Right
			tryShiftRight(&g, &t);
			break;
		case 2:	//Middle
			tryMirror(&g, &t);
			break;
		case 3:	//Middle + Right
			tryTurnCW(&g, &t);
			break;
		case 4:	//Left
			tryShiftLeft(&g, &t);
			break;
		case 5:	//Left + Right
			while(!land(&g, &t)) {
				down(&t);
			}
			break;
		case 6:	//Left + Middle
			tryTurnCCW(&g, &t);
			break;
		case 7:	//Left + Middle + Right
			gameState = GameOver;
			uart_putc(MusicGameOver);
			return;
		}
		pressed_prev = pressed;
		if(pressed_prev == pressed && pressed == 5) {
			hard_drop++;
		} else {
			hard_drop = 0;
		}


		if(land(&g, &t)) {		//See if we stop falling here
			if(inBoundsOpen(&g, &t)) {	//We landed in the screen
				place(&g, &t);	//Place in grid
				gameState = PlayInterval;
				//Check for rows to clear
				for(unsigned short y = HEIGHT - 1; y < HEIGHT; y--) {
					//We decrement because we know when underflow happens
					if(rowFull(&g, y)) {
						rowCleared = y;
						rowState = 6;
						gameState = RowClear;

						for(unsigned short yBelow = y - 1; yBelow < HEIGHT; yBelow--) {
							//We decrement because we know when underflow happens
							if(rowFull(&g, yBelow)) {
								score++;
							}
						}
						break;
					}
				}
				placed++;
				fall = 0;
				t = CreateTetra();
				if(gameState != RowClear) {
					if(hard_drop > 12) {
						time = 0;
					}
					else if(hard_drop > 4) {
						time = standardInterval / 2;
					} else {
						time = standardInterval * 5;
					}
					//Save the game
					SaveState();
					SaveGame(&g, &t);
				} else {
					time = standardInterval;
				}
			} else {
				//We landed above the top of the screen
				//Game over
				gameState = GameOver;
				uart_putc(MusicGameOver);
				rowCleared = 0;
				//Restore the standard interval
				standardInterval = STANDARD_INTERVAL;
				time = standardInterval;
			}
		} else {
			down(&t);
			//place(&g, &t);
			fall++;
			time = standardInterval;
			gameState = PlayInterval;
		}
		break;
	} case PlayInterval:
		time = standardInterval;
		gameState = Play;
		break;
	case RowClear:
		//Flash the row being cleared
		if(--rowState%2 == 1) {
			clearRow(&g, rowCleared);
		} else {
			fillRow(&g, rowCleared);
		}
		//Keep flashing or clear the row and continue playing
		if(rowState > 0) {
			time = standardInterval;
		} else {
			descendRow(&g, rowCleared);
			//Add points based on the row cleared
			score += 1 + rowCleared;

			//See if we should return to gameplay
			gameState = Play;
			//Check if we have more rows to clear
			for(unsigned short y = HEIGHT - 1; y < HEIGHT; y--) {
				//We decrement because we know when underflow happens
				if(rowFull(&g, y)) {
					rowCleared = y;
					rowState = 6;
					gameState = RowClear;

					break;
				}
			}
			if(gameState != RowClear) {
				//Return to gameplay
				time = standardInterval * 3;
				//Make the game faster depending on score
				standardInterval = STANDARD_INTERVAL - (score / 5);
				
				//Save the game
				SaveState();
				SaveGame(&g, &t);
			} else {
				//Clear another row
				time = standardInterval;
			}
		}
		break;
	case GameOver:
		//Flash dark and descend the rows until the grid is empty, then go to score screen
		if(!rowEmpty(&g, 0)) {
			//Descend the next row
			clearRow(&g, 0);
			descendRow(&g, 0);
			time = standardInterval;
			gameState = GameOverFlash;
		} else {
			//Done clearing the rows, so go to score screen
			time = standardInterval;
			//screenState = FinalScore;
			SetScreenState(FinalScore);
			gameState = Init;	//Prepare the state for the next game
		}
		break;
	case GameOverFlash:
		//Flash dark
		time = standardInterval;
		gameState = GameOver;
		break;
	}
	nokia_lcd_clear();
	if(gameState == GameOverFlash) {
		//Flash dark
		for(short y = 0; y < HEIGHT; y++) {
			for(short x = 0; x < WIDTH; x++) {
				drawTile(x, y, 1);
			}
		}
	} else if(gameState == Play) {
		//Draw the grid
		for(short x = 0; x < WIDTH; x++) {
			for(short y = 0; y < HEIGHT; y++) {
				drawTile(x, y, g.tiles[x][y]);
			}
		}
		//Draw the piece
		for(short i = 0; i < 4; i++) {
			Point p = getTile(&t, i);
			if(p.y < HEIGHT) {
				drawTile(p.x, p.y, 1);
			}
		}
		//Create the ghost piece
		Tetra landed = t;
		//Descend it to the bottom
		while(!land(&g, &landed)) {
			down(&landed);
		}
		//Draw the ghost piece
		for(short i = 0; i < 4; i++) {
			Point p = getTile(&landed, i);
			if(p.y < HEIGHT) {
				drawTile(p.x, p.y, 1);
			}
		}
	} else if(gameState == PlayInterval) {
		//Draw the grid
		for(short x = 0; x < WIDTH; x++) {
			for(short y = 0; y < HEIGHT; y++) {
				drawTile(x, y, g.tiles[x][y]);
			}
		}
		//Draw the ghost piece
		for(short i = 0; i < 4; i++) {
			Point p = getTile(&t, i);
			if(p.y < HEIGHT) {
				drawTile(p.x, p.y, 1);
			}
		}
	} else {
		//Otherwise just draw the grid
		for(short x = 0; x < WIDTH; x++) {
			for(short y = 0; y < HEIGHT; y++) {
				drawTile(x, y, g.tiles[x][y]);
			}
		}
	}
	nokia_lcd_render();
}
void UpdateState() {
	switch(screenState) {
	case Title: UpdateTitle();
		PORTB = 0;
		break;
	case Game: UpdateGame();
		PORTB = score;					//Display score on the LEDs
		break;
	case FinalScore: UpdateFinalScore();
		PORTB = 0;
		break;
	default:
		//screenState = Title;
		SetScreenState(Title);
		UpdateState();
		break;
	}
}
int main(void)
{
	/*
	Grid g;
	Tetra t = L((Point) {.x = 2, .y = 18});
	
	signed short i;
	Add: i = 0;
	t = L((Point) {.x = 2, .y = 18});
	while(!land(&g, &t)) {
		down(&t);
		if(canShiftRight(&t))
			right(&t);
		i++;
	}
	//If we're inbounds, then we can place
	if(inBounds(&g, &t)) {
		place(&g, &t);
		goto Add;
	} else {
	//Otherwise, we lost
		int done = 1;
		return;
	}
	*/
	//while(1) UpdateGame();
	/*
    nokia_lcd_init();
    nokia_lcd_clear();
    nokia_lcd_write_string("IT'S WORKING!",1);
    nokia_lcd_set_cursor(0, 10);
    nokia_lcd_write_string("Nice!", 3);
    nokia_lcd_render();

	TimerSet(1000);
	TimerOn();
	*/
	DDRD = 0xF2;
	PIND = 0x01;
	PORTD = 0;

	DDRC = 0xFF;
	PORTC = 0;

	DDRA = ~7;
	PORTA = 0;
	PINA = 7;

	DDRB = -1;
	PORTB = 0;
	nokia_lcd_init();
	nokia_lcd_clear();
	TimerSet(TIMER_INTERVAL);
	TimerOn();

	LCD_init();

	//screenState = Title;
	SetScreenState(Title);
	highScore = eeprom_read_byte(ADDR_SCORE);

	uart_init(25); //https://circuitdigest.com/microcontroller-projects/uart-communication-between-two-atmega8-microcontrollers

	while(1) {
		UpdateState();
		while(!TimerFlag);
		TimerFlag = 0;
	}

	return 0;
}

