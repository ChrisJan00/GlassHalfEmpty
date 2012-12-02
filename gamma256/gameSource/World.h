#include <stdint.h>
typedef uint32_t Uint32;


// these can be called once at beginning and end of app execution
// since loaded graphics can be reused for multiple games
void loadWorldGraphics();
void destroyWorldGraphics();
    

// these should be called at the beginning and end of each new game
void initWorld();
void destroyWorld();
        

Uint32 sampleFromWorld( int inX, int inY, double inWeight = 1.0 );

void setPlayerPosition( int inX, int inY );
void setMirrorPosition( int inX, int inY );
void setPlayerSpriteFrame( int inFrame );
void setMirrorSpriteFrame( int inFrame );

void spouseFlee();
void resetSpouse(int playerX, int playerY);
void moveSpouseTo( int spouseX, int spouseY);
void updateSpousePosition(int playerX, int playerY);

void getSpousePosition( int *outX, int *outY );


void diePlayer();

char isPlayerDead();



// age in range 0..1
void setCharacterAges( double inAge );




// push animations forward one step
void stepAnimations();




int getTileWidth();


int getTileHeight();

