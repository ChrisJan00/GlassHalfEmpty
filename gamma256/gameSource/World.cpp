#include "World.h"


#include "common.h"
#include "map.h"


#include "minorGems/graphics/Image.h"
#include "minorGems/util/SimpleVector.h"





class GraphicContainer {
    
    public:
        
        GraphicContainer( const char *inTGAFileName ) {
        
            Image *image = readTGA( inTGAFileName );
    
            mW = image->getWidth();
            mH = image->getHeight();
            int imagePixelCount = mW * mH;
    
            mRed =  new double[ imagePixelCount ];
            mGreen =  new double[ imagePixelCount ];
            mBlue =  new double[ imagePixelCount ];
            
            for( int i=0; i<imagePixelCount; i++ ) {
                mRed[i] = 255 * image->getChannel(0)[ i ];
                mGreen[i] = 255 * image->getChannel(1)[ i ];
                mBlue[i] = 255 * image->getChannel(2)[ i ];
                }
            delete image;
            }
        
        
        ~GraphicContainer() {
            delete [] mRed;
            delete [] mGreen;
            delete [] mBlue;
            }
        
        
        double *mRed;
        double *mGreen;
        double *mBlue;
        
        int mW;
        int mH;
        
    
    };

GraphicContainer *tileContainer;

GraphicContainer *spriteContainer;
GraphicContainer *spouseContainer;
GraphicContainer *mirrorContainer;


// dimensions of one tile.  TileImage contains 13 tiles, stacked vertically,
// with blank lines between tiles
int tileW = 8;
int tileH = 8;

int tileImageW;
int tileImageH;


int numTileSets;

// how wide the swath of a world is that uses a given tile set
int tileSetWorldSpan = 200;
// overlap during tile set transition
int tileSetWorldOverlap = 50;

// for testing
int tileSetSkip = 0;

int tileSetOrder[18] = { 0, 2, 10, 1, 8, 3, 5, 6, 4, 7, 13, 9, 15, 
                         14, 16, 12, 11, 17 };



int mapTileSet( int inSetNumber ) {
    // stay in bounds of tileSetOrder
    inSetNumber = inSetNumber % 18;
    
    int mappedSetNumber = tileSetOrder[ inSetNumber ];
    
    // stay in bounds of tile set collection
    return mappedSetNumber % numTileSets;
    }





int girlW = 8;
int girlH = 8;




class Animation {
    public:
        
        Animation( int inX, int inY, int inFrameW, int inFrameH,
                   char inAutoStep,
                   char inRemoveAtEnd, GraphicContainer *inGraphics )
                : mX( inX ), mY( inY ), 
                  mFrameW( inFrameW ), mFrameH( inFrameH ),
                  mPageNumber( 0 ),
                  mFrameNumber( 0 ),
                  mAutoStep( inAutoStep ),
                  mRemoveAtEnd( inRemoveAtEnd ),
                  mOpacity( 1.0 ),
                  mGraphics( inGraphics ) {
            
            mImageW = mGraphics->mW;
            
            mNumFrames = ( mGraphics->mH - mFrameH ) / mFrameH + 1;
            mNumPages = ( mGraphics->mW - mFrameW ) / mFrameW + 1;
            }
        
        // default constructor so that we can build a vector
        // of Animations
        Animation() {
            }
        
        
        // replaces graphics of animation
        void swapGraphics( GraphicContainer *inNewGraphics ) {
        
            mGraphics = inNewGraphics;
            
            mImageW = mGraphics->mW;
            
            mNumFrames = ( mGraphics->mH - mFrameH ) / mFrameH + 1;
            mNumPages = ( mGraphics->mW - mFrameW ) / mFrameW + 1;
            
            if( mPageNumber >= mNumPages ) {
                mPageNumber = mNumPages - 1;
                }
            if( mFrameNumber >= mNumFrames ) {
                mFrameNumber = mNumFrames - 1;
                }
            }
        
        

        int mX, mY;
        
        
        int mFrameW, mFrameH;
        int mImageW;
        
        // can blend between pages
        double mPageNumber;

        int mNumPages;
        
        int mFrameNumber;
        int mNumFrames;
        char mAutoStep;
        char mRemoveAtEnd;
        
        double mOpacity;
        
        GraphicContainer *mGraphics;
        
    };



SimpleVector<Animation> animationList;




// pointers into vectors are unsafe

int spriteAnimationIndex;
int spouseAnimationIndex;
int mirrorAnimationIndex;


char playerDead = false;
char spouseEscaping = false;



void resetSampleHashTable();



void initWorld() {
    resetMap();
    resetSampleHashTable();
    
    playerDead = false;

    animationList.deleteAll();
    
    tileImageW = tileContainer->mW;
    tileImageH = tileContainer->mH;
    
    numTileSets = (tileImageW + 1) / (tileW + 1);
    
    Animation character(  0, 0, 8, 8, false, false, spriteContainer );
    
    
    animationList.push_back( character );
    
    // unsafe
    // get pointer to animation in vector
    //spriteAnimation = animationList.getElement( animationList.size() - 1 );
    spriteAnimationIndex = animationList.size() - 1;

    Animation spouse(  100, 7, 8, 8, false, false, spouseContainer );
    spouse.mFrameNumber = 7;
    
    
    animationList.push_back( spouse );
    
    // unsafe!
    // get pointer to animation in vector
    //spouseAnimation = animationList.getElement( animationList.size() - 1 );
    spouseAnimationIndex = animationList.size() - 1;


    spouseEscaping = false;

    Animation mirror( 1000, 0, 8, 8, false, false, mirrorContainer );
    mirror.mFrameNumber = 0;

    animationList.push_back( mirror );
    mirrorAnimationIndex = animationList.size() - 1;
    }


char isSpriteTransparent( GraphicContainer *inContainer, int inSpriteIndex ) {
    
    // take transparent color from corner
    return 
        inContainer->mRed[ inSpriteIndex ] == 
        inContainer->mRed[ 0 ]
        &&
        inContainer->mGreen[ inSpriteIndex ] == 
        inContainer->mGreen[ 0 ]
        &&
        inContainer->mBlue[ inSpriteIndex ] == 
        inContainer->mBlue[ 0 ];
    }



struct rgbColorStruct {
    double r;
    double g;
    double b;
    };
typedef struct rgbColorStruct rgbColor;


 
// outTransient set to true if sample returned is part of a transient
// world feature (character sprite, girl, etc.)
rgbColor sampleFromWorldNoWeight( int inX, int inY, char *outTransient );

// same, but wrapped in a hash table to store non-transient results
rgbColor sampleFromWorldNoWeightHash( int inX, int inY );



Uint32 sampleFromWorld( int inX, int inY, double inWeight ) {
    rgbColor c = sampleFromWorldNoWeightHash( inX, inY );
    
    unsigned char r = (unsigned char)( inWeight * c.r );
    unsigned char g = (unsigned char)( inWeight * c.g );
    unsigned char b = (unsigned char)( inWeight * c.b );
    
#ifdef PIXEL_FORMAT_BGRA
    return (b << 16 | g << 8 | r) << 8;
#else // PIXEL_FORMAT_ARGB
    return r << 16 | g << 8 | b;
#endif
}



char isSpritePresent( int inX, int inY ) {
    // look at animations
    for( int i=0; i<animationList.size(); i++ ) {
        Animation a = *( animationList.getElement( i ) );
        
        int animW = a.mFrameW;
        int animH = a.mFrameH;
        
        
        // pixel position relative to animation center
        // player position centered on sprint left-to-right
        int animX = (int)( inX - a.mX + a.mFrameW / 2 );
        int animY = (int)( inY - a.mY + a.mFrameH / 2 );
        
        if( animX >= 0 && animX < animW
            && 
            animY >= 0 && animY < animH ) {
            return true;
            }
        
        }
    

    return false;
    }



#include "HashTable.h"

HashTable<rgbColor> worldSampleHashTable( 30000 );


void resetSampleHashTable() {
    worldSampleHashTable.clear();
    }



rgbColor sampleFromWorldNoWeightHash( int inX, int inY ) {
    
    char found;
    
    rgbColor sample = worldSampleHashTable.lookup( inX, inY, &found );
    
    if( isSpritePresent( inX, inY ) ) {
        // don't consider cached result if a sprite is present
        found = false;
        }
            

    if( found ) {
        return sample;
        }
    
    // else not found
    
    // call real function to get result
    char transient;
    sample = sampleFromWorldNoWeight( inX, inY, &transient );
    
    // insert, but only if not transient
    if( !transient ) {
        worldSampleHashTable.insert( inX, inY, sample );
        }
    
    return sample;
    }



rgbColor sampleFromWorldNoWeight( int inX, int inY, char *outTransient ) {
    *outTransient = false;

    rgbColor returnColor;
    
    char isSampleAnim = false;
    double tileOpacity = 1.0;
    
    // consider sampling from an animation
    // draw them in reverse order so that oldest sprites are drawn on top
    for( int i=animationList.size() - 1; i>=0; i-- ) {

        Animation a = *( animationList.getElement( i ) );
        
        int animW = a.mFrameW;
        int animH = a.mFrameH;

        
        
        // pixel position relative to animation center
        // player position centered on sprint left-to-right
        int animX = (int)( inX - a.mX + a.mFrameW / 2 );
        int animY = (int)( inY - a.mY + a.mFrameH / 2 );
        
        if( animX >= 0 && animX < animW
            && 
            animY >= 0 && animY < animH ) {
                        
            // pixel is in animation frame

            int animIndex = animY * a.mImageW + animX;
        
            // skip to appropriate anim page
            animIndex += (int)a.mPageNumber * (animW + 1);
            
            
            // skip to appropriate anim frame
            animIndex += 
                a.mFrameNumber * 
                a.mImageW * 
                ( animH + 1 );
            
            // page to blend with
            int animBlendIndex = animIndex + animW + 1;
            
            double blendWeight = a.mPageNumber - (int)a.mPageNumber;
            
        
            if( !isSpriteTransparent( a.mGraphics, animIndex ) ) {
            
                tileOpacity = 1-a.mOpacity;

                // in non-transparent part

                if( blendWeight != 0 ) {
                    returnColor.r = 
                        ( 1 - blendWeight ) * a.mGraphics->mRed[ animIndex ] 
                        +
                        blendWeight * a.mGraphics->mRed[ animBlendIndex ];
                    returnColor.g = 
                        ( 1 - blendWeight ) * a.mGraphics->mGreen[ animIndex ] 
                        +
                        blendWeight * a.mGraphics->mGreen[ animBlendIndex ];
                    returnColor.b = 
                        ( 1 - blendWeight ) * a.mGraphics->mBlue[ animIndex ] 
                        +
                        blendWeight * a.mGraphics->mBlue[ animBlendIndex ];
                    }
                else {
                    // no blend
                    returnColor.r = a.mGraphics->mRed[ animIndex ];
                    returnColor.g = a.mGraphics->mGreen[ animIndex ];
                    returnColor.b = a.mGraphics->mBlue[ animIndex ];
                    }
                
                *outTransient = true;
                // don't return here, because there might be other
                // animations and sprites that are on top of
                // this one
                isSampleAnim = true;
                }
            }
        }

    if( isSampleAnim && tileOpacity == 1 ) {
        // we have already found an animation here that is on top
        // of whatever map graphics we might sample below
        
        // thus, we can return now
        return returnColor;
        }
    


    int tileIndex;
    
    char blocked = isBlocked( inX, inY );
    
    if( !blocked ) {
        // empty tile
        tileIndex = 0;
        }
    else {
        
        int neighborsBlockedBinary = 0;
        
        if( isBlocked( inX, inY - tileH ) ) {
            // top
            neighborsBlockedBinary = neighborsBlockedBinary | 1;
            }
        if( isBlocked( inX + tileW, inY ) ) {
            // right
            neighborsBlockedBinary = neighborsBlockedBinary | 1 << 1;
            }
        if( isBlocked( inX, inY + tileH ) ) {
            // bottom
            neighborsBlockedBinary = neighborsBlockedBinary | 1 << 2;
            }
        if( isBlocked( inX - tileW, inY ) ) {
            // left
            neighborsBlockedBinary = neighborsBlockedBinary | 1 << 3;
            }
    
        // skip empty tile, treat as tile index
        neighborsBlockedBinary += 1;
        
        tileIndex = neighborsBlockedBinary;
        }
    
    

    // pick a tile set
    int netWorldSpan = tileSetWorldSpan + tileSetWorldOverlap;

    int tileSet = inX / netWorldSpan + tileSetSkip;
    int overhang = inX % netWorldSpan;
    if( inX < 0 ) {
        // fix to a constant tile set below 0
        overhang = 0;
        tileSet = 0;
        }
    
    // is there blending with next tile set?
    int blendTileSet = tileSet + 1;
    double blendWeight = 0;
    
    if( overhang > tileSetWorldSpan ) {
        blendWeight = ( overhang - tileSetWorldSpan ) / 
            (double) tileSetWorldOverlap;
        }
    // else 100% blend of our first tile set


    tileSet = tileSet % numTileSets;
    blendTileSet = blendTileSet % numTileSets;
    
    // make sure not negative
    if( tileSet < 0 ) {
        tileSet += numTileSets;
        }
    if( blendTileSet < 0 ) {
        blendTileSet += numTileSets;
        }
    
    
    // apply mapping
    tileSet = mapTileSet( tileSet );
    blendTileSet = mapTileSet( blendTileSet );


    // sample from tile image
    int imageY = inY % tileH;
    int imageX = inX % tileW;
    

    if( imageX < 0 ) {
        imageX += tileW;
        }
    if( imageY < 0 ) {
        imageY += tileH;
        }
    
    // offset to top left corner of tile
    int tileImageOffset = tileIndex * ( tileH + 1 ) * tileImageW
        + tileSet * (tileW + 1);
    int blendTileImageOffset = tileIndex * ( tileH + 1 ) * tileImageW
        + blendTileSet * (tileW + 1);
    
    
    int imageIndex =  tileImageOffset + imageY * tileImageW + imageX;
    int blendImageIndex =  blendTileImageOffset + imageY * tileImageW + imageX;

    returnColor.r =
        (1-tileOpacity) * returnColor.r + tileOpacity * (
        (1-blendWeight) * tileContainer->mRed[ imageIndex ] +
        blendWeight * tileContainer->mRed[ blendImageIndex ] );
    
    returnColor.g =
        (1-tileOpacity) * returnColor.g + tileOpacity * (
        (1-blendWeight) * tileContainer->mGreen[ imageIndex ] +
        blendWeight * tileContainer->mGreen[ blendImageIndex ] );
    
    returnColor.b =
        (1-tileOpacity) * returnColor.b + tileOpacity * (
        (1-blendWeight) * tileContainer->mBlue[ imageIndex ] +
        blendWeight * tileContainer->mBlue[ blendImageIndex ] );
    
    
    // only consider drawing girls in empty spots
    if( !blocked && isGirl( inX, inY ) ) {
        // draw girl here

        // draw spouse sprite instead of girl
        Animation a = *( animationList.getElement( spouseAnimationIndex ) );

        int animW = a.mFrameW;
        int animH = a.mFrameH;


        // pixel position relative to animation center
        // player position centered on sprint left-to-right
        int animX = inX % a.mFrameW;
        int animY = inY % a.mFrameH;

        if( animX >= 0 && animX < animW
            &&
            animY >= 0 && animY < animH ) {

            // pixel is in animation frame

            int animIndex = animY * a.mImageW + animX;


            // skip to appropriate anim frame
            animIndex +=
                7 *
                a.mImageW *
                ( animH + 1 );

            // page to blend with
            int animBlendIndex = animIndex + animW + 1;

            double blendWeight = a.mPageNumber - (int)a.mPageNumber;


            if( !isSpriteTransparent( a.mGraphics, animIndex ) ) {

                // in non-transparent part

                if( blendWeight != 0 ) {
                    returnColor.r =
                        ( 1 - blendWeight ) * a.mGraphics->mRed[ animIndex ]
                        +
                        blendWeight * a.mGraphics->mRed[ animBlendIndex ];
                    returnColor.g =
                        ( 1 - blendWeight ) * a.mGraphics->mGreen[ animIndex ]
                        +
                        blendWeight * a.mGraphics->mGreen[ animBlendIndex ];
                    returnColor.b =
                        ( 1 - blendWeight ) * a.mGraphics->mBlue[ animIndex ]
                        +
                        blendWeight * a.mGraphics->mBlue[ animBlendIndex ];
                    }
                else {
                    // no blend
                    returnColor.r = a.mGraphics->mRed[ animIndex ];
                    returnColor.g = a.mGraphics->mGreen[ animIndex ];
                    returnColor.b = a.mGraphics->mBlue[ animIndex ];
                    }

                *outTransient = true;
                // don't return here, because there might be other
                // animations and sprites that are on top of
                // this one
                isSampleAnim = true;
                }
            }
                
        }
        
        
    return returnColor;
    }






int getTileWidth() {
    return tileW;
    }



int getTileHeight() {
    return tileH;
    }



void destroyWorld() {
    }



void stepAnimations() {
    
    
    for( int i=0; i<animationList.size(); i++ ) {
        Animation *a = animationList.getElement( i );
        if( a->mAutoStep ) {
            if( a->mFrameNumber < a->mNumFrames - 1 ) {
                a->mFrameNumber ++;
                }
            else if( a->mRemoveAtEnd ) {
                // remove it
                animationList.deleteElement( i );
                // back up in list for next loop iteration
                i--;
                }
            }
        
        }
    }


#include <math.h>


void setPlayerPosition( int inX, int inY ) {

    Animation *spriteAnimation = 
        animationList.getElement( spriteAnimationIndex );

    char moving = false;

    if( inX != spriteAnimation->mX ) {
        moving = true;
        }

    
    spriteAnimation->mX = inX;
    // player position centered at sprite's feet
    int newSpriteY = inY - spriteAnimation->mFrameH / 2 + 1;
    

    if( newSpriteY != spriteAnimation->mY ) {
        moving = true;
        }
    
    spriteAnimation->mY = newSpriteY;

    }

void spouseFlee() {
    spouseEscaping = true;
}

void resetSpouse(int playerX, int playerY) {
    Animation *spouseAnimation = animationList.getElement(spouseAnimationIndex);

    spouseEscaping = false;
    spouseAnimation->mX = playerX + 150;
    spouseAnimation->mY = playerY;
}

void moveSpouseTo( int spouseX, int spouseY) {
    Animation *spouseAnimation = animationList.getElement(spouseAnimationIndex);

    spouseAnimation->mX = spouseX;
    spouseAnimation->mY = spouseY;
}

void updateSpousePosition(int playerX, int playerY) {
    if (!spouseEscaping)
        return;

    int spouseSpeed = 3;
    int currentSpriteIndex = 3;

    Animation *spouseAnimation = animationList.getElement(spouseAnimationIndex);
    if (spouseAnimation->mX < playerX) {
        spouseAnimation->mX -= spouseSpeed;
        if( ( spouseAnimation->mX / 2 ) % 2 == 0 ) {
            currentSpriteIndex = 6;
        }
        else {
            currentSpriteIndex = 7;
        }
    } else {
        spouseAnimation->mX += spouseSpeed;
        if( ( spouseAnimation->mX / 2 ) % 2 == 0 ) {
            currentSpriteIndex = 2;
        }
        else {
            currentSpriteIndex = 3;
        }
    }

    if (spouseAnimation->mX < 0) {
        spouseAnimation->mX += 4500;
        spouseEscaping = false;
    }

    if (spouseAnimation->mX > 4500) {
        spouseAnimation->mX -= 1;
        spouseEscaping = false;
    }


    spouseAnimation->mFrameNumber = currentSpriteIndex;

}


void setMirrorPosition( int inX, int inY ) {

    Animation *mirrorAnimation =
        animationList.getElement( mirrorAnimationIndex );

    mirrorAnimation->mX = inX;
    // player position centered at sprite's feet
    int newSpriteY = inY - mirrorAnimation->mFrameH / 2 + 1;

    mirrorAnimation->mY = newSpriteY;

    }


void setPlayerSpriteFrame( int inFrame ) {
    Animation *spriteAnimation = 
        animationList.getElement( spriteAnimationIndex );

    spriteAnimation->mFrameNumber = inFrame;

    // update opacity if it's "dying"
    if (!playerDead) {
        spriteAnimation->mOpacity = 1.0;
    } else {
        spriteAnimation->mFrameNumber = 8;
        spriteAnimation->mOpacity *= 0.9;
    }

    }

void setMirrorSpriteFrame( int inFrame ) {
    Animation *mirrorAnimation =
            animationList.getElement( mirrorAnimationIndex );

    if (!isPlayerDead())
        mirrorAnimation->mFrameNumber = inFrame;
    else
        mirrorAnimation->mFrameNumber = 8;
}


void setCharacterAges( double inAge ) {
    Animation *spriteAnimation = 
        animationList.getElement( spriteAnimationIndex );

    // 0 -> 0.25, constant page 0
    if( inAge <= 0.25 ) {
        spriteAnimation->mPageNumber = 0;
        }
    // 0.75 - 1.0, constant last page
    else if( inAge >= 0.75 ) {
        spriteAnimation->mPageNumber = spriteAnimation->mNumPages - 1;
        }
    else {
        // blend of pages in between
        double blendingAge = ( inAge - 0.25 ) / 0.5;
        
        spriteAnimation->mPageNumber = 
            blendingAge * ( spriteAnimation->mNumPages - 1 );
        }
    }



void getSpousePosition( int *outX, int *outY ) {
    Animation *spouseAnimation = 
        animationList.getElement( spouseAnimationIndex );
    
    *outX = spouseAnimation->mX;
    *outY = spouseAnimation->mY + spouseAnimation->mFrameH / 2 - 1;
    }


void diePlayer() {
    Animation *spriteAnimation = 
        animationList.getElement( spriteAnimationIndex );

    playerDead = true;
    
    // tombstone (not with the mirror, only animation freezes and fade to black)
    spriteAnimation->mFrameNumber = 8;
    }

char isPlayerDead() {
    return playerDead;
}


void loadWorldGraphics() {
    tileContainer = new GraphicContainer( "tileSet.tga" );
      
    spriteContainer = new GraphicContainer( "characterSprite.tga" );
    spouseContainer = new GraphicContainer( "spouseSprite.tga" );
    mirrorContainer = new GraphicContainer( "mirrorSprite.tga");
    }



void destroyWorldGraphics() {
    delete tileContainer;
      
    delete spriteContainer;
    delete spouseContainer;
    delete mirrorContainer;
    }

