#ifndef SPRITESHPP
#define SPRITESHPP

#include <stdio.h>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;



struct Vector2
{
    float x = 0;
    float y = 0;

    void Print(){
        printf("x: %f  y: %f \n", x, y);
    }

    ///this is an unbelievably useful feature that I had no idea about, thanks chatGPT. You can just use the listed equations to add/subtract/multiply vectors, 
    //like you can at base in unity
    Vector2 operator-(const Vector2& other) const {
        return {x - other.x, y - other.y};
    }

    Vector2 operator+(const Vector2& other) const {
        return {x + other.x, y + other.y};
    }

    Vector2 operator*(float scalar) const {
        return {x * scalar, y * scalar};
    }

    Vector2 normalized() const {
        float length = std::sqrt(x * x + y * y);
        if (length == 0) return {0, 0};
        return {x / length, y / length};
    }
};

//constant 
const vector<string> tags = {


};

struct sprite_structure
{
    Vector2 size;
    uint8_t img[1024]; // max size is 1024 but just in case
};

struct sprite_screen_structure
{
    Vector2 pos;
    Vector2 size;
    float rotation;

    pair<const string, sprite_structure>* sprite; //this is safe as 'sprites' is const and never getting altered in-game
    
    //current hex saved, including rotation. This is inneficient on memory but more efficient on not having to recalculate rotation a lot
    uint8_t img[1024] = {0x00}; 
};

const unordered_map<string, sprite_structure> sprites = {

    {"16x16Square", {{16,16}, {0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}}},
    {"Paddle", {{16,8}, {0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03}}},
    {"8x8Square", {{8,8}, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}},

    {"Line", {{2,64}, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}},
    {"SnakeHead", {{5, 3}, {0x04, 0x06, 0x05, 0x06, 0x06}}},
    {"SnakeBody", {{4, 3}, {0x02, 0x03, 0x03, 0x01}}},
    {"SnakePellet", {{3, 3}, {0x02, 0x05, 0x02}}},
    
    {"TetrisBlock", {6, 6, {0x3f, 0x39, 0x3d, 0x3f, 0x3f, 0x3f}} },
 };

 #endif
