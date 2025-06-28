#include <stdio.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <vector>
#include <time.h>
#include <functional>
#include <math.h>
#include <cmath>
#include <map>
#include <unordered_set>
#include "sprites.h"
#include "pico/stdlib.h"
#include "ssd1306_i2c.h"
#include "pico/rand.h"

using namespace std;
using Callback = std::function<void()>;
 
map<string, sprite_screen_structure> allSprites{};

int lastTime = to_ms_since_boot(get_absolute_time());

struct Animation
{
    int timer = 0; // timer that counts up every loop until it passes the timeToMove
 
    int initialTimer = 0; // timer that gets sent to the current time on creation

    int delayToNext = 0; // the amount of milliseconds until the sprite STARTS going to the next position
    int timeToMove = 0;  // the amount of milliseconds that the sprite takes to move
    Vector2 speed = {0,0}; //pixels to be moved per second

    Vector2 startingPosition;
    Vector2 destination;

    string name = "";
    sprite_screen_structure *sprite; // this points to a sprite in the global sprite list, therefore this animation has to delete
    // if its ever a nullptr (need a lot of robust null checks here)

    bool started = false; //handles whether we've started moving or not

    Callback whileMoving = nullptr; // function that's always being called during movement
    Callback onFinish = nullptr;    // function to call when finished
};

struct AnimationManager
{
    // is this necessary or can we delete it after getting to the final spot?
    // no because there might be different ways
    int totalTime; // total time in Milliseconds before this gets deleted
    int animPosition = 0;
    vector<Animation> animList = {};
    bool dead = false;
};
vector<AnimationManager> anims = {};


char *ConvertString(int position, string items[], char temp[])
{

    for (int i = 0; i < items[position].length(); i++)
    {
        temp[i] = items[position][i];
    }

    return temp;
}

char *ConvertString(string temp, char test[])
{

    for (int i = 0; i < temp.length(); i++)
    {
        test[i] = temp[i];
    }

    return test;
}

void DeleteEverything(){

    allSprites.clear();

    DeleteScreen();
}

/// @brief returns true if a position within the global hex is
/// @param position
/// @param sprite
/// @return
bool IsHexWithinBounds(int x, int y, sprite_screen_structure &sprite)
{
    return (x >= sprite.pos.x && x <= sprite.pos.x + sprite.size.x &&
            y >= sprite.pos.y / 8 && y <= (sprite.pos.y + sprite.size.y) / 8);
}

/// @brief returns true if a position within the global hex is
/// @param position
/// @param sprite
/// @return
bool IsBoundsWithinPage(sprite_screen_structure &sprite, sprite_screen_structure &otherSprite)
{
    // Calculate page ranges for both sprites
    int spriteStartPage = sprite.pos.y / 8;
    int spriteEndPage = (sprite.pos.y + sprite.size.y) / 8;
    int otherSpriteStartPage = otherSprite.pos.y / 8;
    int otherSpriteEndPage = (otherSprite.pos.y + otherSprite.size.y) / 8;

    // Check if x bounds overlap and if page ranges overlap
    return (
        sprite.pos.x <= otherSprite.pos.x + otherSprite.size.x &&
        sprite.pos.x + sprite.size.x >= otherSprite.pos.x &&
        spriteStartPage <= otherSpriteEndPage &&
        spriteEndPage >= otherSpriteStartPage
    );
}

/// @brief returns true if a position within the global hex is
/// @param position
/// @param sprite
/// @return
bool IsBoundsWithinBounds(sprite_screen_structure &sprite, sprite_screen_structure &otherSprite)
{

    return (
        sprite.pos.x < otherSprite.pos.x + otherSprite.size.x &&
        sprite.pos.x + sprite.size.x > otherSprite.pos.x &&
        sprite.pos.y < otherSprite.pos.y + otherSprite.size.y &&
        sprite.pos.y + sprite.size.y > otherSprite.pos.y
    );
}

bool IsBoundsWithinBounds(Vector2 pos, Vector2 size, Vector2 pos2, Vector2 size2)
{

    return (
        pos.x < pos2.x + size2.x &&
        pos.x + size.x > pos2.x &&
        pos.y < pos2.y + size2.y &&
        pos.y + size.y > pos2.y
    );
}



bool IsWithinScreen(sprite_screen_structure &sprite){
    return (
        sprite.pos.x >= 0 &&                   
        sprite.pos.x + sprite.size.x <= 127 && 
        sprite.pos.y >= 0 &&                    
        sprite.pos.y + sprite.size.y <= 63     
    );
}


/// @brief Instead of a bool return, we instead return the direction we've hit. A more complete system would be more flexible on the angle
///that you've hit but I don't know how to implement that with sprites. 
/// @param position 
/// @param sprite 
/// @return 
int DirectionScreen(Vector2 position, sprite_screen_structure &sprite){
 

    if(sprite.pos.x + position.x < 0){
        return 0;
    }else if(sprite.pos.x + position.x + sprite.size.x > 127){
        return 0;
    }else if(sprite.pos.y + position.y < 0){
        return 180;
    }else if(sprite.pos.y + position.y + sprite.size.y > 63){
        return 180;
    }
    return -1;
}

//conveniance function that doesn't worry about direction 
bool IsWithinScreen(Vector2 position, sprite_screen_structure &sprite){
    int temp = DirectionScreen(position, sprite);

    if(temp == -1){
        return true;
    }else{
        return false;
    }
    
}

 




/// @brief draws a sprite to the global buffer. Currently no way to have 'uneven' wrap arounds (so if you go over 56 you wrap to 12, but the reverse takes you to 118, for example)
/// @param sprite 
/// @param wraparoundValueUnder the value we go to when we go OVER the max, so above wraparoundValueOver
/// @param wraparoundValueOver the value we go to when we go under the minimum, so below wraparundValueUnder
void DrawToGlobalBackend(sprite_screen_structure& sprite, int drawOrErase, bool wrapAround, Vector2 wraparoundValueUnder, Vector2 wraparoundValueOver)
{
    int modulo = (int)sprite.pos.y % 8; // where we are on the current page we're starting ons
  
    int hexSprite = 0; // counter for what hex in the array we're in
    int globalBitPosition = modulo; // counter for what bit position in the global buffers target hex we are editing

    int extraTracker = 0; //This could also be worked out the loop values but its easier to just have yet another counter variable
    for (int x = sprite.pos.x; x < sprite.pos.x + sprite.size.x; x++)
    {

        int xTracker = 0; //bit tracker for sprite
        globalBitPosition = modulo; 
        hexSprite = extraTracker;

        int xTemp = x;

        if(wrapAround && x >= wraparoundValueOver.x){
            xTemp = x - wraparoundValueOver.x;
        }else if(wrapAround && x <= wraparoundValueUnder.x){
            xTemp = x + wraparoundValueOver.x;
        }

        for (int y = sprite.pos.y; y < sprite.pos.y + sprite.size.y; y++) //so loop through every bit
        {
            int yTemp = y;

            if(wrapAround && y >= wraparoundValueOver.y){
                yTemp = y - wraparoundValueOver.y;
            }else if(wrapAround && y <= wraparoundValueUnder.y){
                yTemp = y + wraparoundValueOver.y;
            }


            if(xTracker > 7){ //we've counted up 8 bits, time to go to the next vertical hex in the sprite
                hexSprite+=sprite.size.x; //the downside of doing x then y is this awkward stuff, but this takes us to the next vertical hex
                xTracker = 0; //reset the counter
            }

            int globalPosition = (((yTemp - (yTemp % 8)) / 8) * 128) + xTemp; //global position in the global hex array


            //now we know which hexes we are working with on this bit, we get the actual bits

            //we're looping through each bit, that means we edit each hex based purely off positional data
            if(y % 8 == 0){ //we're about to go over into the next page, reset the counter.
                globalBitPosition = 0;
            }


            /*
            EXPLANATION FOR HOW BITWISE OPERATORS WORK FOR LATER
            This line: 
            
            bool bitValue = (sprite.img[hexSprite] >> xTracker) & 1;

            means we take sprite.img[hexSprite], and move all the bits to the right by xTracker, replacing the left bits with 0's
            then compares the new hex with 1 (00000001), and returns true if ANY of the bits in the same position for both hexes is 1, which because 
            1 is 00000001 can only be the last bit after shifting the bits right
            */
            // if(sp->pos.y > wraparoundValueOver.y) sp->pos.y = wraparoundValueUnder.y;
            // if(sp->pos.y < wraparoundValueUnder.y) sp->pos.y = wraparoundValueOver.y;


            bool bitValue = (sprite.img[hexSprite] >> xTracker) & 1; //and the bit in the SPRITES hex we are putting in

            if (bitValue) {
                if (drawOrErase) {
                    bufferGlobal[globalPosition] |= (1 << globalBitPosition); // Set bit
                } else {
                    bufferGlobal[globalPosition] &= ~(1 << globalBitPosition); // Clear bit
                }
            }


            xTracker++;
            globalBitPosition++;

        } // vertical
        extraTracker++;
    }

}

void DrawToGlobal(sprite_screen_structure& sprite, int drawOrErase = 1, bool wrapAround = true, Vector2 wraparoundValueUnder = {-1,-1}, Vector2 wraparoundValueOver = {128,64}){
    DrawToGlobalBackend(sprite, drawOrErase, wrapAround, wraparoundValueUnder, wraparoundValueOver);
}


/// @brief This finds every sprite thats touching every other sprite from the original sprite, recursively. 
/// @param sprite 
/// @param overlaps 
void FindAllOverlap(sprite_screen_structure& sprite, unordered_set<sprite_screen_structure*>& overlaps){

    //loop every sprite on the screen, and if they're in bounds add them to the list
 
    for(auto& sp : allSprites){
        bool next = false;
        if(IsBoundsWithinBounds(sprite, sp.second)){
            if (overlaps.insert(&sp.second).second) {
                FindAllOverlap(sp.second, overlaps);
            }
        }
    }
}

/// @brief figure out if the sprite being sent is hitting anything else, nuke anything being touched, and redraw them
void RemoveSpriteFromGlobalLoop(string name, bool wrapAround = false, Vector2 wraparoundValueUnder = {0,0}, Vector2 wraparoundValueOver = {128,64})
{
    if (!allSprites.count(name)) return;
 
    unordered_set<sprite_screen_structure*> overlaps = {&allSprites.at(name)};

    // //we know we've got the sprite somewhere in there
    sprite_screen_structure* sprite = &allSprites[name];

    FindAllOverlap(*sprite, overlaps); 


    //now nuke every sprite thats overlapped at all
    for(sprite_screen_structure* spr : overlaps){
        DrawToGlobal(*spr, 0, wrapAround, wraparoundValueUnder, wraparoundValueOver);
    }

    for(sprite_screen_structure* spr : overlaps){ //do we need another loop or can it be in 1?
        if(spr != sprite){
            DrawToGlobal(*spr, 1, wrapAround, wraparoundValueUnder, wraparoundValueOver);
        }
    }
}

void DrawToGlobalMove(string name, bool wrapAround = false, Vector2 wraparoundValueUnder = {0,0}, Vector2 wraparoundValueOver = {128,64});

void RemoveSpriteFromGlobal(string name, bool wrapAround = false, Vector2 wraparoundValueUnder = {0,0}, Vector2 wraparoundValueOver = {128,64}){
    RemoveSpriteFromGlobalLoop(name, wrapAround, wraparoundValueUnder, wraparoundValueOver);
}

void RedrawSprite(string name){
    RemoveSpriteFromGlobal(name);
    DrawToGlobalMove(name);
}


void RemoveSpriteFromListAndGlobal(string name){
    RemoveSpriteFromGlobal(name);
    allSprites.erase(name);
}


/// @brief currently you can't specify size here, it just goes of longest string. Thats a fix for later
/// @param x
/// @param y
/// @param text
/// @param size
/// @param gap
void DisplayTextMultipleLines(int x, int y, const char **text, int size, int gap, string name, int positionToInvert = -1)
{
    int longest = GetLongestString(text, size);

    sprite_screen_structure textSprite{
        {x,
         y},
        {longest * 8,
         8 * 7},
        {0}
    };
    int counterForThingy = 0;

    for (int i = 0; i < size; i++)
    {   
        WriteStringBlock(textSprite.img, 0, y, text[i], longest, &counterForThingy, (i == positionToInvert));
        y = y + gap;
    }

    allSprites[name] = textSprite;
    DrawToGlobal(textSprite, 1);
}


/// @brief Taxes a C++ string, writes it to the screen as a sprite and saves it in the list like any other sprite
/// @param posX 
/// @param posY 
/// @param stringToConvert 
/// @param nameOfSprite 
void CreateNewTextSprite(int posX, int posY, string stringToConvert, string nameOfSprite, bool invert = false)
{
    sprite_screen_structure text{
        {posX,
         posY},
        {stringToConvert.length() * (size_t)8,
         8},
        {0}
    }; 

    WriteString(text.img, posX, posY, stringToConvert.c_str(), invert);

    allSprites[nameOfSprite] = text;
    
    DrawToGlobal(text, 1);
}

/// @brief convert a center area to the side area
/// @param name 
/// @param text 
/// @return 
int ConvertCenterToSide(int position, int size){
    return position - size / 2; 
}

int ConvertCenterToLeftSideText(int position, string str){
    return position - (str.length() * 8) / 2;
}

int ConvertCenterToRightSideText(int position, string str){
    return position - (str.length() * 8);
}

/// @brief Takes a text sprite and edits it with new text
void ChangeTextSprite(string name, string text){
    if(!allSprites.count(name)) return;


    Vector2 vec = {allSprites[name].pos.x, allSprites[name].pos.y};
    RemoveSpriteFromGlobal(name);
    CreateNewTextSprite(vec.x, vec.y, text, name);
}


//
Vector2 CalculateDirectionPos(Animation ma)
{
    Vector2 pos = {0, 0};

    if (ma.destination.x > ma.startingPosition.x)
    {
        pos.x = 1;
    }
    else if (ma.destination.x < ma.startingPosition.x)
    {
        pos.x = -1;
    }

    if (ma.destination.y > ma.startingPosition.y)
    {
        pos.y = 1;
    }
    else if (ma.destination.y < ma.startingPosition.y)
    {
        pos.y = -1;
    }

    return pos;
}


Vector2 CalculateVelocity(const Vector2 start, const Vector2 end, float timeSeconds) {
    if (timeSeconds == 0) return {0.0f, 0.0f}; // avoid division by zero

 
    Vector2 delta;
    delta.x = (end.x - start.x) / timeSeconds;
    delta.y = (end.y - start.y) / timeSeconds;

    return delta; // pixels per second
}




//Version that takes a starting position instead of just going off the sprite position
//for use when chaining multiple animations together
Animation MoveListAddition(int posX, int posY, int destX, int destY, int timeToMove, string name, int delayToMove = 0)
{
    Animation ma;

    //get a reference to the sprite that exists in the list and 
    ma.name = name;
    ma.sprite = &allSprites[name];
    ma.timeToMove = timeToMove; //max movement time
    ma.destination = Vector2{destX, destY};
    ma.startingPosition = Vector2{posX, posY};
    ma.delayToNext = delayToMove;

    ma.speed = CalculateVelocity({posX,posY},{destX,destY}, (timeToMove / 1000.0f));

    printf("x %f   y %f\n", ma.speed.x, ma.speed.y);

    return ma;
}


///helper function to make a new animationmanager with a single animation loaded into it. Only supports basics, no function calls or anything
AnimationManager SingleAnimation(int posX, int posY, int destX, int destY, int timeToMove, string name, int delayToMove = 0){
    AnimationManager animMan = {};

    Animation anim = MoveListAddition(posX, posY, destX, destY, timeToMove, name);
    animMan.animList.push_back(anim);

    anims.push_back(animMan);

    return animMan;
}



/// @brief takes a sprite and returns a hex array thats a rotated version, always a 90 degree. 
//I cant work out how to make this dynamic so right now it always goes by 90 degrees its using an if/else statement
/// @param spr 
void RotateSpriteClockwiseby90(sprite_screen_structure* spr, float rotation, string str){

    RemoveSpriteFromGlobal(str);

    sprite_screen_structure temp; 

    //get the bits of the sprite at 0 rotation and save it into a giant grid 
 
    //its easiest to assign everyhing to temp then copy it over
    temp.size.x = spr->size.y; 
    temp.size.y = spr->size.x;

    temp.pos.x = spr->pos.x;
    temp.pos.y = spr->pos.y;


    int hexBitPosition = 0; //the bit position of the hex that we're 'moving'
    int targetBitPosition = 0;//the bit position of the hex that we're editing
 

    for(int y = 0; y < spr->size.y; y++){

        if(hexBitPosition > 7){
            hexBitPosition = 0;
        }
        targetBitPosition = 0;


        for(int x = 0; x < spr->size.x; x++){
            //we need to count down the bits, right to left and put them top to bottom on the right f

            if(targetBitPosition > 7){
                targetBitPosition = 0;
            }

            int currentHexPosition = ((int)(y / 8) * spr->size.x) + x;//the hex in the sprite we're rotating
            int targetHexPosition = (temp.size.x - 1) + ((x / 8) * temp.size.x) - y; //the hex that we're editing

            bool bitValue = (spr->img[currentHexPosition] >> hexBitPosition) & 1; //and the bit in the SPRITES hex we are putting in

            if(bitValue){
                temp.img[targetHexPosition] |= 1  << targetBitPosition;
            }


            targetBitPosition++;
        }   
       // sleep_ms(5000);
        
        hexBitPosition++;

    }

     spr->size.x = temp.size.x;
     spr->size.y = temp.size.y;


    for(int i = 0 ; i < 1023; i++){
        spr->img[i] = 0x00;
        spr->img[i] = temp.img[i];
    }
 
 
    DrawToGlobal(*spr);

    UpdateFromGlobal();
 }


/// @brief creates the sprite_structure_??? and adds it to the list, then calls RenderGoBetween
void CreateNewSprite(int x, int y, sprite_structure spriteStructure, string name)
{
    sprite_screen_structure sprite = {{x, y}, {spriteStructure.size.x, spriteStructure.size.y}};

    int size = spriteStructure.size.x * (spriteStructure.size.y > 8 ? spriteStructure.size.y / 8 : 1);

    
    for (int i = 0; i < size; i++)
    {
        sprite.img[i] = spriteStructure.img[i]; // adding the array to the sprite structure
    }

    allSprites.insert({name, sprite});
    DrawToGlobal(sprite, 1);
}


int getRandomInRange(int min, int max)
{ 
    uint32_t r = get_rand_32();
    return min + (r % (max - min + 1));
}


/// @brief Go-between function that takes a string and calls DrawToGlobal
void DrawToGlobalMove(string name, bool wrapAround, Vector2 wraparoundValueUnder, Vector2 wraparoundValueOver){
    if (!allSprites.count(name)) return;
    
    //we know we've got the sprite somewhere in there
    sprite_screen_structure* sprite = &allSprites[name];

    DrawToGlobalBackend(*sprite, 1, wrapAround, wraparoundValueUnder, wraparoundValueOver);
}
  

/// @brief returns whether we've make a full pixels movement or not
/// @param name name to search sprite list 
/// @param movement amount we're moving this cycle
/// @param stepAmount //amount of extra pixels we want to move at a time over 1. Note that this multiplies the speed by whatever specified as well by moving it X pixels each time it reaches the 1 pixel threshhold
Vector2 FullPixelMove(sprite_screen_structure &sp, Vector2 movement, int extraStepAmount = 0, float rotation = 0){

    Vector2 move = {};

    //easy way of 'rounding up' a float instead of the default round down
    //this is optional but it prevents us redrawing the sprite every single cycle if we haven't moved a full pixel yet
    if(sp.pos.y + movement.y < (int)sp.pos.y){
        move.y = -1;
    }else if(sp.pos.y + movement.y > ceil(sp.pos.y)){
        move.y = 1;
    }

    if(sp.pos.x + movement.x > ceil(sp.pos.x)){
        move.x = 1;   
    }else if(sp.pos.x + movement.x < (int)sp.pos.x){
        move.x = -1;
    }

    return move;

}


Vector2 MoveSprite(string name, Vector2 movement, bool wrapAround = true, Vector2 wraparoundValueUnder = { 1,1}, Vector2 wraparoundValueOver = {128,64}){
    sprite_screen_structure *sp = &allSprites.at(name);

    Vector2 pixel = FullPixelMove(*sp, movement);

    RemoveSpriteFromGlobal(name, wrapAround, wraparoundValueUnder, wraparoundValueOver);
 

    sp->pos.x += movement.x;
    sp->pos.y += movement.y;

    if(wrapAround){
        if(sp->pos.y >= wraparoundValueOver.y) sp->pos.y = wraparoundValueUnder.y;
        else if(sp->pos.y <= wraparoundValueUnder.y) sp->pos.y = wraparoundValueOver.y;

        if(sp->pos.x >= wraparoundValueOver.x) sp->pos.x = wraparoundValueUnder.x;
        else if(sp->pos.x <= wraparoundValueUnder.x) sp->pos.x =  wraparoundValueOver.x;
    }

    DrawToGlobalMove(name, true, wraparoundValueUnder, wraparoundValueOver);
    //CreateNewSprite(sp->pos.x, sp->pos.y, sp, "Snake");

    return pixel; //return the firection if either is used. We don't use this much.
}


void MoveToPosition(string name, Vector2 newPosition){
    sprite_screen_structure *sp = &allSprites.at(name);
 
    RemoveSpriteFromGlobal(name);
    //DeleteScreen();
    sp->pos.x = (int)newPosition.x;
    sp->pos.y = (int)newPosition.y;

    DrawToGlobalMove(name);
    //CreateNewSprite(sp->pos.x, sp->pos.y, sp, "Snake");
 
}




float DegToRads(float deg){
    return deg * (M_PI  / 180);
}

void ChangeDegree(float& degrees, float change){
    degrees += change;

    if(degrees > 360){
        degrees = degrees - 360;
    }else if(degrees < 0){
        degrees = 360 + degrees;
    }
}
void ChangeToSpecifiedDegree(float& degrees, float change){
    degrees = change;

    if(degrees > 360){
        degrees = degrees - 360;
    }else if(degrees < 0){
        degrees = 360 + degrees;
    }
}

Vector2 MoveSpriteCalculations(string name, float direction, float speed){

    ChangeDegree(direction, -90.0);
    
    int time = to_ms_since_boot(get_absolute_time()) - lastTime;//the amount of time thats passed
    float percent = (float)time / 1000; //percentage of a second thats passed

    //convert the float to a vector2

    float rads = DegToRads(direction);

    Vector2 directionVec = {
        cos(rads),
        sin(rads)
    };

    Vector2 movement = {
        directionVec.x * speed * percent,
        directionVec.y * speed * percent
    };


    return movement;
}



Vector2 MoveSpriteCalculations(string name, Vector2 direction, float speed){

    int time = to_ms_since_boot(get_absolute_time()) - lastTime;//the amount of time thats passed
    float percent = (float)time / 1000; //percentage of a second thats passed

    //

    Vector2 movement = {
        direction.x * speed * percent,
        direction.y * speed * percent
    };
    return movement;
}


Vector2 MoveSpriteCalculations(string name, Vector2 directionWithSpeed){

    int time = to_ms_since_boot(get_absolute_time()) - lastTime;//the amount of time thats passed
    float percent = (float)time / 1000; //percentage of a second thats passed

    //

    Vector2 movement = {
        directionWithSpeed.x * percent,
        directionWithSpeed.y * percent
    };
    return movement;
}


void AnimationExecuter(){



    // loop all animations
    for (int i = 0; i < anims.size(); i++)
    {
        Animation &ma = anims[i].animList[anims[i].animPosition]; //
        Vector2 pos = {};                                         // normalized position
        int32_t current_time = to_ms_since_boot(get_absolute_time());

        MoveSprite(ma.name, MoveSpriteCalculations(ma.name, ma.speed));



        ma.timer = current_time - ma.initialTimer;

        if (ma.timer > ma.timeToMove)
        {
            if (anims[i].animList[anims[i].animPosition].onFinish)
            {
                anims[i].animList[anims[i].animPosition].onFinish();
            }

            if (anims[i].animPosition + 1 <= anims[i].animList.size() - 1)
            { 
                anims[i].animPosition += 1;
            }
            else
            {
                anims[i].dead = true;
            }
        }

    }; // i wonder if there's a cleaner way to do this

    //I dont like that we do a second loop but its much easier to get wrking
    for (auto it = anims.begin(); it != anims.end(); ) {
        if (it->dead)
            it = anims.erase(it);  // erase returns the next valid iterator
        else
            ++it;
    }
}