#ifndef FUNCTIONS
#define FUNCTIONS

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
#include "functions.hpp"
#include "pico/stdlib.h"
#include "ssd1306_i2c.h"
#include "pico/rand.h"


map<string, sprite_screen_structure> allSprites{};


void Update();
void CreateNewTextSprite(int posX, int posY, std::string stringToConvert, std::string nameOfSprite, bool invert = false);
int ConvertCenterToSide(int position, int size);
void DeleteEverything();
void RemoveSpriteFromGlobal(string name, bool wrapAround = false, Vector2 wraparoundValueUnder = {0,0}, Vector2 wraparoundValueOver = {128,64});
void CreateNewSprite(int x, int y, sprite_structure spriteStructure, string name);

#endif