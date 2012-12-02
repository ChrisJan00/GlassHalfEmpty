// checks if position is blocked by wall
char isBlocked( int inX, int inY );


// checks if girl is present
// assumes position is not blocked
#define GIRL_NONE 0
#define GIRL_PRESENT 1
char isGirl( int inX, int inY );

void getGirlCenter( int inX, int inY, int *outCenterX, int *outCenterY );

void contactGirl( int inX, int inY );


// resets map to a fresh state
void resetMap();
