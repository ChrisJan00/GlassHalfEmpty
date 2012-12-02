/*
 * Modification History
 *
 * 2007-September-25   Jason Rohrer
 * Created.  
 */



#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


// for memcpy
#include <string.h>


// let SDL override our main function with SDLMain
#include <SDL/SDL_main.h>

// must do this before SDL include to prevent WinMain linker errors on win32
int mainFunction( int inArgCount, char **inArgs );

int main( int inArgCount, char **inArgs ) {
    return mainFunction( inArgCount, inArgs );
    }


#include <SDL/SDL.h>

#include "blowUp.h"
#include "World.h"
#include "map.h"
#include "score.h"
#include "common.h"
#include "musicPlayer.h"

#include "minorGems/system/Time.h"
#include "minorGems/system/Thread.h"
#include "minorGems/util/stringUtils.h"
#include "minorGems/util/SettingsManager.h"



// size of game image
int width = 100;
int height = 16;
// area above game image for score
int scoreHeight = getScoreHeight();

// size of game image plus scoreboard
int totalImageHeight = height + scoreHeight;

char paused = false;
char canPause = false;


// size of screen for fullscreen mode
int screenWidth = 640;
int screenHeight = 480;
//int screenWidth = 100;
//int screenHeight = 16;

// blow-up factor when projecting game onto screen
// Max defined by image size vs screen size.
// This is the cap for in-game user-directed blowup adjustment.
int maxBlowUpFactor;

// current blow-up setting
int blowUpFactor;


// step to take when user hits blowUp key
int blowUpStep = -1;
// flag to force update of entire screen
int blowUpChanged = true;


char fullScreen = true;


// lock down to 15 fps
int lockedFrameRate = 15;


// target length of game
int gameTime = 5 * 60;
//int gameTime = 30;


// life time that passes per frame 
double timeDelta = - ( (double)width / ( gameTime * lockedFrameRate ) );
//double timeDelta = -0.0;


//int mirrorOrigin = 4500;
int mirrorMax = 3500;
int mirrorOrigin = mirrorMax;
int mirrorMin = 150;

// the joystick to read from, or NULL if there's no available joystick
SDL_Joystick *joystick;



// catch an interrupt signal
void catch_int(int sig_num) {
	printf( "Quiting...\n" );
    SDL_Quit();
	exit( 0 );
	signal( SIGINT, catch_int );
	}



char getKeyDown( int inKeyCode ) {
    SDL_PumpEvents();
	Uint8 *keys = SDL_GetKeyState( NULL );
	return keys[ inKeyCode ] == SDL_PRESSED;
    }



char getHatDown( Uint8 inHatPosition ) {
    if( joystick == NULL ) {
        return false;
        }
    
    SDL_JoystickUpdate();

    Uint8 hatPosition = SDL_JoystickGetHat( joystick, 0 );
    
    if( hatPosition & inHatPosition ) {
        return true;
        }
    else {
        return false;
        }
    }


int joyThreshold = 25000;

char getJoyPushed( Uint8 inHatPosition ) {
        
    Sint16 x = SDL_JoystickGetAxis(joystick, 0);
    Sint16 y = SDL_JoystickGetAxis(joystick, 1);
    
    switch( inHatPosition ) {
        case SDL_HAT_DOWN:
            return  y > joyThreshold;
            break;
        case SDL_HAT_UP:
            return  y < -joyThreshold;
            break;
        case SDL_HAT_LEFT:
            return  x < - joyThreshold;
            break;
        case SDL_HAT_RIGHT:
            return  x > joyThreshold;
            break;
        default:
            return false;
        }
            
        
    }



// returns true if hit, returns false if user quits before hitting
char waitForKeyOrButton() {
    SDL_Event event;
    
    while( true ) {
        while( SDL_WaitEvent( &event ) ) {
            switch( event.type ) {
                case SDL_JOYHATMOTION:
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP:
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    return true;
                    break;
                // watch for quit event
                case SDL_QUIT:
                    return false;
                    break;
                default:
                    break;
                }
            }
        }
    
    
    return false;
    }



Uint32 *gameImage;



// flips back buffer onto screen (or updates rect)
void flipScreen( SDL_Surface *inScreen ) {

    // unlock the screen if necessary
    if( SDL_MUSTLOCK( inScreen ) ) {
        SDL_UnlockSurface( inScreen );
        }
    
    if( ( inScreen->flags & SDL_DOUBLEBUF ) == SDL_DOUBLEBUF ) {
        // need to flip buffer
        SDL_Flip( inScreen );	
        }
    else if( !blowUpChanged ) {
        // just update center

        // small area in center that we actually draw in, black around it
        int yOffset = ( inScreen->h - totalImageHeight * blowUpFactor ) / 2;
        int xOffset = ( inScreen->w - width * blowUpFactor ) / 2;
        
        SDL_UpdateRect( inScreen, xOffset, yOffset, 
                        width * blowUpFactor, 
                        totalImageHeight * blowUpFactor );	
        }
    else {
        // update the whole thing
        SDL_UpdateRect( inScreen, 0, 0, inScreen->w, inScreen->h );
        
        // reset flag
        blowUpChanged = false;
        }
    
    }



void lockScreen( SDL_Surface * inScreen ) {
    // check if we need to lock the screen
    if( SDL_MUSTLOCK( inScreen ) ) {
        if( SDL_LockSurface( inScreen ) < 0 ) {
            printf( "Couldn't lock screen: %s\n", SDL_GetError() );
            }
        }
    }



void flipGameImageOntoScreen( SDL_Surface *inScreen ) {
        
    if( blowUpChanged 
	&&
	( inScreen->flags & SDL_DOUBLEBUF ) == SDL_DOUBLEBUF ) {
   
        // blow up size has changed, and we are double-buffered
        // flip onto screen an additional time.
        // This will cause us to black-out the background in both buffers.

        lockScreen( inScreen );
	
	// when blow-up factor changes:
	// clear screen to prepare for next draw, 
	// which will be bigger or smaller
	SDL_FillRect( inScreen, NULL, 0x00000000 );
        
	blowupOntoScreen( gameImage, 
			  width, totalImageHeight, blowUpFactor, inScreen );
    
	flipScreen( inScreen );
        }

    lockScreen( inScreen );
    if( blowUpChanged ) {
        SDL_FillRect( inScreen, NULL, 0x00000000 );
        }
    blowupOntoScreen( gameImage, 
		      width, totalImageHeight, blowUpFactor, inScreen );
    
    flipScreen( inScreen );

    // we've handled any blow up change
    blowUpChanged = false;
    }







SDL_Surface *screen = NULL;


// play a complete game, from title screen to end, on screen
// returns false if player quits
char playGame();



void createScreen() {
    if( screen != NULL ) {
        // destroy old one first
        SDL_FreeSurface( screen );
        }

    int displayW = width * blowUpFactor;
    int displayH = totalImageHeight * blowUpFactor;

    
    Uint32 flags = SDL_HWSURFACE | SDL_DOUBLEBUF;
    if( fullScreen ) {
        flags = flags | SDL_FULLSCREEN;
    
        // use letterbox mode in full screen
        displayW = screenWidth;
        displayH = screenHeight;
        }
    
    

    screen = SDL_SetVideoMode( displayW, displayH, 
                               32, 
                               flags );
    
    if ( screen == NULL ) {
        printf( "Couldn't set %dx%dx32 video mode: %s\n", displayW, 
                displayH,
                SDL_GetError() );
        }

    SDL_WM_SetCaption("Glass Half Empty", 0);
    }



int mainFunction( int inArgCount, char **inArgs ) {

    // let catch_int handle interrupt (^c)
    signal( SIGINT, catch_int );


    Uint32 initFlags = 
        SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_NOPARACHUTE;
    
    #ifdef __mac__
        // SDL_Init is dreadfully slow if we try to init JOYSTICK on MacOSX
        // not sure why---maybe it's searching all devices or something like 
        // that.
        // Couldn't find anything online about this.
        initFlags = 
	    SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE;
    #endif
    
    if( SDL_Init( initFlags ) < 0 ) {
        printf( "Couldn't initialize SDL: %s\n", SDL_GetError() );
        return -1;
        }
    
    SDL_ShowCursor( SDL_DISABLE );


    // read screen size from settings
    char widthFound = false;
    int readWidth = SettingsManager::getIntSetting( "screenWidth", 
                                                    &widthFound );
    char heightFound = false;
    int readHeight = SettingsManager::getIntSetting( "screenHeight", 
                                                    &heightFound );
    
    if( widthFound && heightFound ) {
        // override hard-coded defaults
        screenWidth = readWidth;
        screenHeight = readHeight;
        }
    
    // set here, since screenWidth may have changed from default
    maxBlowUpFactor = screenWidth / width;
    
    // default to max
    blowUpFactor = maxBlowUpFactor;


    char fullscreenFound = false;
    int readFullscreen = SettingsManager::getIntSetting( "fullscreen",
                                                         &fullscreenFound );
    if( fullscreenFound ) {
        fullScreen = readFullscreen;
        }
    


    createScreen();
    

    // try to open joystick
    int numJoysticks = SDL_NumJoysticks();
    
    if( numJoysticks > 0 ) {
        // open first one by default
        joystick = SDL_JoystickOpen( 0 );
        int numHats = SDL_JoystickNumHats( joystick );
        
        if( numHats <= 0 ) {
            SDL_JoystickClose( joystick );
            joystick = NULL;
            }
        }
    else {
        joystick = NULL;
        }


    
    #ifdef __mac__
        // make sure working directory is the same as the directory
        // that the app resides in
        // this is especially important on the mac platform, which
        // doesn't set a proper working directory for double-clicked
        // app bundles
    
        // arg 0 is the path to the app executable
        char *appDirectoryPath = stringDuplicate( inArgs[ 0 ] );

        printf( "Mac:  app path %s\n", appDirectoryPath );

        char *appNamePointer = strstr( appDirectoryPath,
                                       "GlassHalfEmpty.app" );

        if( appNamePointer != NULL ) {
            // terminate full app path to get parent directory
            appNamePointer[0] = '\0';
            
            printf( "Mac: changing working dir to %s\n", appDirectoryPath );

            chdir( appDirectoryPath );
            chdir( "GlassHalfEmpty.app/Contents/Resources" );
            }
        
        delete [] appDirectoryPath;
    #endif





    // load graphics only once
    loadWorldGraphics();
    

    setMusicLoudness( 0 );
    startMusic( "music.tga" );
    
    // keep playing until player quits
    while( playGame() ) {
        }
    
    stopMusic();
    

    destroyWorldGraphics();
    

    if( joystick != NULL ) {
        SDL_JoystickClose( joystick );
        }
    
    SDL_Quit();

    printf("\n\nGlass Half Empty\nby Christiaan Janssen\na remix of 'Passage' by Jason Rohrer\nBerlin Mini Game Jam, December 2012\n\n");
    fflush(0);

    return 0;
    }


/*
// TEMP
#include "minorGems/graphics/converters/TGAImageConverter.h"

#include "minorGems/io/file/File.h"

#include "minorGems/io/file/FileInputStream.h"
// END TEMP
*/


char playGame() {
    int currentSpriteIndex = 2;
    double playerX, playerY;
    double maxPlayerX = 0;
    bool spouseWaiting = true;
    
    initWorld();
    initScore();
    
    // room at top for score
    int totalGameImagePixels = width * totalImageHeight;
    
    gameImage = new Uint32[ totalGameImagePixels ];
    
    int i;
    
    // fill with black
    for( i=0; i<totalGameImagePixels; i++ ) {
        gameImage[i] = 0;
        }
    

    // first, fill the whole thing with black
    // SDL_FillRect( screen, NULL, 0x00000000 );

    double dX = 0;
    double dY = 0;

    
    char done = false;
    
    double maxWorldX = 0;
    double minWorldX = 0;


    // start player position
    playerX = getTileWidth();
    maxPlayerX = playerX;
    
    dX = playerX;
    playerY = 2 + height/2;
    dY = 0;

    setPlayerPosition( (int)playerX, (int)playerY );
    setPlayerSpriteFrame( currentSpriteIndex );

    int mirrorX = mirrorOrigin - (int)playerX;
    setMirrorPosition( mirrorX, (int)playerY );
    setMirrorSpriteFrame( currentSpriteIndex );

    double lastFrameTimeStamp = Time::getCurrentTime();
    
    // use to slow player motion after spouse has died
    char movingThisFrame = true;
    

    // first, flip title onto screen
    Image *titleImage = readTGA( "title.tga" );
    int numTitlePixels = titleImage->getWidth() * titleImage->getHeight();
    
    double *titleRed = titleImage->getChannel( 0 );
    double *titleGreen = titleImage->getChannel( 1 );
    double *titleBlue = titleImage->getChannel( 2 );
    
    Uint32 *titlePixels = new Uint32[ numTitlePixels ];
    
    for( int i=0; i<numTitlePixels; i++ ) {
#ifdef PIXEL_FORMAT_BGRA
        titlePixels[ i ] = (
            (unsigned char )( titleBlue[i] * 255 ) << 16
            |
            (unsigned char )( titleGreen[i] * 255 ) << 8
            |
            (unsigned char )( titleRed[i] * 255 )
            ) << 8;

#else
        titlePixels[ i ] =
                    (unsigned char )( titleRed[i] * 255 ) << 16
                    |
                    (unsigned char )( titleGreen[i] * 255 ) << 8
                    |
                    (unsigned char )( titleBlue[i] * 255 );
#endif
        }
    
    // fill screen with title
    memcpy( gameImage, titlePixels, 4 * numTitlePixels );
    

    flipGameImageOntoScreen( screen );
    

    char hit = waitForKeyOrButton();
    
    if( !hit ) {
        // no key pressed...
        // maybe they closed the window
        // count as a "quit"
        done = true;
        }
    else {
        // they pressed a key to start the game
        
        // return to start
        restartMusic();
        
        // turn up music
        setMusicLoudness( 1.0 );
        }
    
        
    

    // fill with black to cover up title
    for( i=0; i<totalGameImagePixels; i++ ) {
        gameImage[i] = 0;
        }



    int frameCount = 0;
    unsigned long startTime = time( NULL );

    char quit = false;
    
    int titleFadeFrame = 0;
    int numTitleFadeFrames = 200;

    char stepDX = true;

    while( !done ) {
        
        if( getKeyDown( SDLK_s ) ) {
            stepDX = false;
            }
        else {
            stepDX = true;
            }
        
        // trick:
        // dX advances linearly, which results in smooth motion
        // instead of single-pixel tick-tick-tick
        // However, this results in smudgy sampling of sprites, which
        // makes them hard to see
        // If we map our sloping dX, which looks like this:  /
        // to a constant between integers with a steep slope up to the next
        // integer, like this:  _/
        // Then we get to look at unsmudged sprites between steps, but
        // still see a smoothed-out tick

        double drawDX = floor( dX );
        double floorWidth = 0.5;
        
        if( dX - drawDX > floorWidth ) {
            // slope
            double slopeValue = 
                ( dX - drawDX - floorWidth ) / ( 1 - floorWidth );
            
            // us trig function to avoid jagged transition from floor to 
            // slope to next floor
            
            drawDX += -0.5 * cos( slopeValue * M_PI ) + 0.5;
            }
        
        if( !stepDX ) {
            // disable smoothed stepping
            drawDX = dX;
            }
        

        for( int y=0; y<height; y++ ) {
            for( int x=0; x<width; x++ ) {
                
                //int worldX = (int)( pow( x / 2.0, 1.4 ) );
                
                // compression based on distance of pixel column from player
                double trueDistanceFromPlayer = drawDX + x - playerX;
                double maxDistance = width - 1;
                
                // cap it so that we don't force tan into infinity below
                double cappedDistanceFromPlayer = trueDistanceFromPlayer;
                if( cappedDistanceFromPlayer > maxDistance ) {
                    cappedDistanceFromPlayer = maxDistance;
                    }
                if( cappedDistanceFromPlayer < -maxDistance ) {
                    cappedDistanceFromPlayer = -maxDistance;
                    }
                
                // the world position we will sample from
                double worldX = x;

                // zone around player where no x compression happens
                int noCompressZone = 10;
                

                if( trueDistanceFromPlayer > noCompressZone ) {
                    
                    worldX =
                        x + 
                        //(width/8) *
                        // use true distance as a factor so that compression
                        // continues at constant rate after we pass capped 
                        // distance
                        // otherwise, compression stops after capped distance
                        // Still... constant rate looks weird, so avoid
                        // it by not letting it pass capped distance
                        trueDistanceFromPlayer / 2 *
                        ( pow( tan( ( ( cappedDistanceFromPlayer - 
                                        noCompressZone ) / 
                                      (double)( width - noCompressZone ) ) * 
                                    M_PI / 2 ), 2) );
                    /*
                        trueDistanceFromPlayer / 2 *
                        ( tan( ( cappedDistanceFromPlayer / 
                                 (double)( width - 0.5 ) ) * M_PI / 2 ) );
                    */
                        // simpler formula
                    // actually, this does not approach 0 as 
                    // cappedDistanceFromPlayer approaches 0, so use tan 
                    // instead
                    //    ( trueDistanceFromPlayer / 2 ) * 100
                    //  / pow( ( width - cappedDistanceFromPlayer ), 1.6 );
                    }
                else if( trueDistanceFromPlayer < - noCompressZone ) {
                    worldX =
                        x + 
                        //(width/8) *
                        trueDistanceFromPlayer / 2 *
                        ( pow( tan( ( ( - cappedDistanceFromPlayer - 
                                        noCompressZone ) / 
                                      (double)( width - noCompressZone ) ) * 
                                    M_PI / 2 ), 2) );
                        /*
                        trueDistanceFromPlayer / 2 * 
                        ( tan( ( - cappedDistanceFromPlayer / 
                                 (double)( width - 0.5 ) ) * M_PI / 2 ) );
                        */
                        //( trueDistanceFromPlayer / 2 ) * 50
                        /// ( width + cappedDistanceFromPlayer );
                    }
                else {
                    // inside no-compresison zone
                    worldX = x;
                    }
                
                
                //int worldX = x;
                
                worldX += drawDX;
                
                if( worldX > maxWorldX ) {
                    maxWorldX = worldX;
                    }
                if( worldX < minWorldX ) {
                    minWorldX = worldX;
                    }
                

                int worldY = (int)floor( y + dY );
                

                // linear interpolation of two samples for worldX
                int intWorldX = (int)floor( worldX );
                
                double bWeight = worldX - intWorldX;
                double aWeight = 1 - bWeight;
                
                                
                Uint32 sampleA = 
                    sampleFromWorld( intWorldX, worldY, aWeight );
                Uint32 sampleB = 
                    sampleFromWorld( intWorldX + 1, worldY, bWeight );
                
                
                
                Uint32 combined = sampleB + sampleA;
                                

                gameImage[ ( y + scoreHeight ) * width + x ] = combined;
                
                }
            }        

        if( isPlayerDead() ) {
            // fade to title screen
            double titleWeight = 
                titleFadeFrame / (double)( numTitleFadeFrames - 1 );
            double gameWeight = 1 - titleWeight;
            
            // wipe from left to right during fade
            int wipePosition = (int)( titleWeight * width );
            
            // fade out music while we do it
            setMusicLoudness( 1.0 - titleWeight );
            

            
            for( i=0; i<totalGameImagePixels; i++ ) {
                

                Uint32 gamePixel = gameImage[i];
#ifdef PIXEL_FORMAT_BGRA
                unsigned char gameRed = gamePixel >> 8 & 0xFF;
                unsigned char gameGreen = gamePixel >> 16 & 0xFF;
                unsigned char gameBlue = gamePixel >> 24 & 0xFF;
#else
                unsigned char gameRed = gamePixel >> 16 & 0xFF;
                unsigned char gameGreen = gamePixel >> 8 & 0xFF;
                unsigned char gameBlue = gamePixel & 0xFF;
#endif
                
                Uint32 titlePixel = titlePixels[i];
#ifdef PIXEL_FORMAT_BGRA
                unsigned char titleRed = titlePixel >> 8 & 0xFF;
                unsigned char titleGreen = titlePixel >> 16 & 0xFF;
                unsigned char titleBlue = titlePixel >> 24 & 0xFF;

#else
                unsigned char titleRed = titlePixel >> 16 & 0xFF;
                unsigned char titleGreen = titlePixel >> 8 & 0xFF;
                unsigned char titleBlue = titlePixel & 0xFF;
#endif
            
                unsigned char red = 
                    (unsigned char)( 
                        gameWeight * gameRed + titleWeight * titleRed );
                unsigned char green = 
                    (unsigned char)( 
                        gameWeight * gameGreen + titleWeight * titleGreen );
                unsigned char blue = 
                    (unsigned char)( 
                        gameWeight * gameBlue + titleWeight * titleBlue );
                

                int x = i % width;
                if( x <= wipePosition ) {
#ifdef PIXEL_FORMAT_BGRA
                    gameImage[i] = (blue << 16 | green << 8 | red) << 8;
#else
                    gameImage[i] = red << 16 | green << 8 | blue;
#endif
                    }
                
                }
            
            if( !paused ) {
                
                titleFadeFrame ++;
                }
            

            if( titleFadeFrame == numTitleFadeFrames ) {
                done = true;
                }
            }
            
        
        flipGameImageOntoScreen( screen );
        

        // done with frame
        double newTimestamp = Time::getCurrentTime();
        
        double frameTime = newTimestamp - lastFrameTimeStamp;
        
        double extraTime = 1.0 / lockedFrameRate - frameTime;
        
        if( extraTime > 0 ) {
            Thread::staticSleep( (int)( extraTime * 1000 ) );
            }

        // start timing next frame
        lastFrameTimeStamp = Time::getCurrentTime();
        


        int spouseX, spouseY;
        getSpousePosition( &spouseX, &spouseY );

        
        int moveDelta = 1;
        
        if( isPlayerDead() ) {
            // stop moving
            moveDelta = 0;
            movingThisFrame = false;
            }
        
        if( getKeyDown( SDLK_p ) && canPause ) {
            paused = true;
            }
        if( getKeyDown( SDLK_o ) ) {
            paused = false;
            }
        if( paused && getKeyDown( SDLK_i ) ) {
            stepAnimations();
            }

        
        if( getKeyDown( SDLK_b ) ) {
            // adjust blowup factor
            blowUpFactor += blowUpStep;
            
            if( blowUpFactor > maxBlowUpFactor ) {
                blowUpStep *= -1;
                blowUpFactor = maxBlowUpFactor - 1;
                }
            
            if( blowUpFactor < 1 ) {
                blowUpStep *= -1;
                blowUpFactor = 2;
                }

            if( fullScreen ) {                
                // force redraw of whole screen
                blowUpChanged = true;
                }
            else {
                // create a new screen using the new size
                createScreen();
                }

            }

        if( getKeyDown( SDLK_f ) ) {
            // toggle fullscreen mode
            fullScreen = ! fullScreen;
            
            // create a new screen surface (and destroy old one)
            createScreen();
            }
        
            

        if( getKeyDown( SDLK_LEFT ) || getJoyPushed( SDL_HAT_LEFT ) ) {
            char notBlocked = 
                !isBlocked( (int)( playerX - moveDelta ), (int)playerY );
                    
            if( movingThisFrame && notBlocked ) {
                
                playerX -= moveDelta;
                
                if( playerX < 0 ) {
                    // undo
                    playerX += moveDelta;
                    }
                else {
                    // update screen position
                    dX -= moveDelta;
                    
                    // pick sprite frame based on position in world
                    if( ( (int)playerX / 2 ) % 2 == 0 ) {
                        currentSpriteIndex = 6;
                        }
                    else {
                        currentSpriteIndex = 7;
                        }                    
                    }
                }
            }
        else if( getKeyDown( SDLK_RIGHT ) || getJoyPushed( SDL_HAT_RIGHT )) {
            char notBlocked = 
                !isBlocked( (int)( playerX + moveDelta ), (int)playerY );

            if( movingThisFrame && notBlocked ) {
                
                dX += moveDelta;
                
                playerX += moveDelta;

                // pick sprite frame based on position in world
                if( ( (int)playerX / 2 ) % 2 == 0 ) {
                    currentSpriteIndex = 3;
                    }
                else {
                    currentSpriteIndex = 2;
                    }  
                }
            }
        else if( getKeyDown( SDLK_UP ) || getJoyPushed( SDL_HAT_UP ) ) {
            char notBlocked =
                !isBlocked( (int)playerX, (int)( playerY - moveDelta ) );         
            
            if( movingThisFrame && notBlocked ) {
                
                playerY -= moveDelta;
                
                if( playerY < 0 ) {
                    // undo
                    playerY += moveDelta;
                    }
                else {
                    // update screen position
                    dY -= moveDelta;

                    // pick sprite frame based on position in world
                    if( ( (int)playerY / 2 ) % 2 == 0 ) {
                        currentSpriteIndex = 0;
                        }
                    else {
                        currentSpriteIndex = 1;
                        }  
                    }
                }
            }
        else if( getKeyDown( SDLK_DOWN )  || getJoyPushed( SDL_HAT_DOWN )) {
            char notBlocked = 
                !isBlocked( (int)playerX, (int)( playerY + moveDelta ) );
            
            
            if( movingThisFrame && notBlocked ) {
                
                dY += moveDelta;
                
                playerY += moveDelta;

                // pick sprite frame based on position in world
                if( ( (int)playerY / 2 ) % 2 == 0 ) {
                    currentSpriteIndex = 5;
                    }
                else {
                    currentSpriteIndex = 4;
                    }  
                }
            }

        setPlayerPosition( (int)playerX, (int)playerY );
        setPlayerSpriteFrame( currentSpriteIndex );


        updateSpousePosition((int)playerX, (int)playerY);

        // may change after we set player position
        getSpousePosition( &spouseX, &spouseY );


        // check for events to quit
        SDL_Event event;
        while( SDL_PollEvent(&event) ) {
            switch( event.type ) {
                case SDL_KEYDOWN:
                    switch( event.key.keysym.sym ) {
                        case SDLK_q:
                        case SDLK_ESCAPE:
                            done = true;
                            quit = true;
                            break;
                        default:
                            break;
                        }
                    break;
                case SDL_QUIT:
                    done = true;
                    quit = true;
                    break;
                default:
                    break;
                }
            }

        //t +=0.25;
        frameCount ++;
        
        // other animations run independent of whether player is moving
        if( !paused && frameCount % 6 == 0 ) {
            stepAnimations();
            }
                
        if( ! isPlayerDead() && ! paused ) {
            // player position on screen inches forward
            dX += timeDelta;
            }

        // not let him "age" behind 0.85
        if (playerX - dX > width * 0.85)
            dX = playerX - width * 0.85;
        
        double age = ( playerX - dX ) / width;
        // adjust mirrorOrigin if too far
        if (mirrorOrigin - playerX > playerX + 150) {
            int newMirrorOrigin = mirrorMax * (1-2*age) + 2 * playerX * 2 * age;
            if (newMirrorOrigin > mirrorMax)
                newMirrorOrigin = mirrorMax;
            if (newMirrorOrigin < mirrorMin)
                newMirrorOrigin = mirrorMin;

            // change it only if it does not warp the mirror to the other side
            if (newMirrorOrigin - (int)playerX > (int) playerX)
                mirrorOrigin = newMirrorOrigin;
        }
        
        int mirrorX = mirrorOrigin - (int)playerX;
        setMirrorPosition( mirrorX, (int)playerY );
        setMirrorSpriteFrame( currentSpriteIndex );
        setCharacterAges( age * 1.5 );


        if ( (int) playerX >= (int) mirrorX ) {
            // when you meet your reflection, the game ends
            diePlayer();
        }

        
        if( isGirl( (int)playerX, (int)playerY ) == GIRL_PRESENT ) {
            int girlX, girlY;
            contactGirl( (int)playerX, (int)playerY );
            getGirlCenter( (int)playerX, (int)playerY, &girlX, &girlY );
            moveSpouseTo( (int)girlX, (int)girlY);
            getSpousePosition(&spouseX, &spouseY);
            spouseFlee();
            spouseWaiting = false;       
        } else {

            int distanceFromSpouse = (int) sqrt( pow( spouseX - playerX, 2 ) +
                                                 pow( spouseY - playerY, 2 ) );

            if (distanceFromSpouse < 10 && spouseWaiting) {
                spouseWaiting = false;
                spouseFlee();
            } else if (distanceFromSpouse > 150) {
                spouseWaiting = true;
                resetSpouse((int)playerX, (int)playerY);
                getSpousePosition(&spouseX, &spouseY);
            }
        }

        // stop after player has gone off right end of screen
        if( playerX - dX > width ) {
            dX = playerX - width;
            }
        if( playerX > maxPlayerX ) {
            maxPlayerX = playerX;
            }

        }

    
    
    delete titleImage;

    delete [] gameImage;
    delete [] titlePixels;
    
    
    destroyWorld();
    destroyScore();
    

    if( quit ) {
        return false;
        }
    else {
        return true;
        }
        
    }

