/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <ctype.h>
 #include <math.h>
 #include "pico/stdlib.h"
 #include "pico/binary_info.h"
 #include "hardware/i2c.h"
 #include "ssd1306_font.h"
 
 
 /* Example code to talk to an SSD1306-based OLED display
 
    The SSD1306 is an OLED/PLED driver chip, capable of driving displays up to
    128x64 pixels.
 
    NOTE: Ensure the device is capable of being driven at 3.3v NOT 5v. The Pico
    GPIO (and therefore I2C) cannot be used at 5v.
 
    You will need to use a level shifter on the I2C lines if you want to run the
    board at 5v.i2c_default
 
    Connections on Raspberry Pi Pico board, other boards may vary.
 
    GPIO PICO_DEFAULT_I2C_SDA_PIN (on Pico this is GP4 (pin 6)) -> SDA on display
    board
    GPIO PICO_DEFAULT_I2C_SCL_PIN (on Pico this is GP5 (pin 7)) -> SCL on
    display board
    3.3v (pin 36) -> VCC on display board
    GND (pin 38)  -> GND on display board
 */
 
 // Define the size of the display we have attached. This can vary, make sure you
 // have the right size defined or the output will look rather odd!
 // Code has been tested on 128x32 and 128x64 OLED displays
 #define SSD1306_HEIGHT              64
 #define SSD1306_WIDTH               128
 
 #define SSD1306_I2C_ADDR            _u(0x3C)
 
 // 400 is usual, but often these can be overclocked to improve display response.
 // Tested at 1000 on both 32 and 84 pixel height devices and it worked.
 #define SSD1306_I2C_CLK             400
 //#define SSD1306_I2C_CLK             1000
 
 
 // commands (see datasheet)
 #define SSD1306_SET_MEM_MODE        _u(0x20)
 #define SSD1306_SET_COL_ADDR        _u(0x21)
 #define SSD1306_SET_PAGE_ADDR       _u(0x22)
 #define SSD1306_SET_HORIZ_SCROLL    _u(0x26)
 #define SSD1306_SET_SCROLL          _u(0x2E)
 
 #define SSD1306_SET_DISP_START_LINE _u(0x40)
 
 #define SSD1306_SET_CONTRAST        _u(0x81)
 #define SSD1306_SET_CHARGE_PUMP     _u(0x8D)
 
 #define SSD1306_SET_SEG_REMAP       _u(0xA0)
 #define SSD1306_SET_ENTIRE_ON       _u(0xA4)
 #define SSD1306_SET_ALL_ON          _u(0xA5)
 #define SSD1306_SET_NORM_DISP       _u(0xA6)
 #define SSD1306_SET_INV_DISP        _u(0xA7)
 #define SSD1306_SET_MUX_RATIO       _u(0xA8)
 #define SSD1306_SET_DISP            _u(0xAE)
 #define SSD1306_SET_COM_OUT_DIR     _u(0xC0)
 #define SSD1306_SET_COM_OUT_DIR_FLIP _u(0xC0)
 
 #define SSD1306_SET_DISP_OFFSET     _u(0xD3)
 #define SSD1306_SET_DISP_CLK_DIV    _u(0xD5)
 #define SSD1306_SET_PRECHARGE       _u(0xD9)
 #define SSD1306_SET_COM_PIN_CFG     _u(0xDA)
 #define SSD1306_SET_VCOM_DESEL      _u(0xDB)
 
 #define SSD1306_PAGE_HEIGHT         _u(8)
 #define SSD1306_NUM_PAGES           (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
 #define SSD1306_BUF_LEN             (SSD1306_NUM_PAGES * SSD1306_WIDTH)
 
 #define SSD1306_WRITE_MODE         _u(0xFE)
 #define SSD1306_READ_MODE          _u(0xFF)
 
 
 struct render_area {
     uint8_t start_col;
     uint8_t end_col;
     uint8_t start_page;
     uint8_t end_page;
 
     int buflen;
 };

 uint8_t bufferGlobal[1024] = { (uint8_t)0 };

 void calc_render_area_buflen(struct render_area *area) {
     // calculate how long the flattened buffer will be for a render area
     area->buflen = (area->end_col - area->start_col + 1) * (area->end_page - area->start_page + 1);
 }
 
 #ifdef i2c_default
 
 void SSD1306_send_cmd(uint8_t cmd) {
     // I2C write process expects a control byte followed by data
     // this "data" can be a command or data to follow up a command
     // Co = 1, D/C = 0 => the driver expects a command
     uint8_t buf[2] = {0x80, cmd};
     i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, buf, 2, true);
 }
 
 void SSD1306_send_cmd_list(uint8_t *buf, int num) {
    for (int i=0;i<num;i++){
        SSD1306_send_cmd(buf[i]);
    }

 }
 
 void SSD1306_send_buf(uint8_t buf[], int buflen) {
     // in horizontal addressing mode, the column address pointer auto-increments
     // and then wraps around to the next page, so we can send the entire frame
     // buffer in one gooooooo!
 
     // copy our frame buffer into a new buffer because we need to add the control byte
     // to the beginning


 
     uint8_t *temp_buf = malloc(buflen + 1);
 
     temp_buf[0] = 0x40;

     memcpy(temp_buf+1, buf, buflen);


     i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, temp_buf, buflen + 1, true);

     free(temp_buf);
 }
 
 void SSD1306_init() {
     // Some of these commands are not strictly necessary as the reset
     // process defaults to some of these but they are shown here
     // to demonstrate what the initialization sequence looks like
     // Some configuration values are recommended by the board manufacturer
 
     uint8_t cmds[] = {
         SSD1306_SET_DISP,               // set display off
         /* memory mapping */
         SSD1306_SET_MEM_MODE,           // set memory address mode 0 = horizontal, 1 = vertical, 2 = page
         0x00,                           // horizontal addressing mode
         /* resolution and layout */
         SSD1306_SET_DISP_START_LINE,    // set display start line to 0
         SSD1306_SET_SEG_REMAP | 0x01,   // set segment re-map, column address 127 is mapped to SEG0
         SSD1306_SET_MUX_RATIO,          // set multiplex ratio
         SSD1306_HEIGHT - 1,             // Display height - 1
         SSD1306_SET_COM_OUT_DIR | 0x08, // set COM (common) output scan direction. Scan from bottom up, COM[N-1] to COM0
         SSD1306_SET_DISP_OFFSET,        // set display offset
         0x00,                           // no offset
         SSD1306_SET_COM_PIN_CFG,        // set COM (common) pins hardware configuration. Board specific magic number.
                                         // 0x02 Works for 128x32, 0x12 Possibly works for 128x64. Other options 0x22, 0x32
 #if ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 32))
         0x02,
 #elif ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 64))
         0x12,
 #else
         0x02,
 #endif
         /* timing and driving scheme */
         SSD1306_SET_DISP_CLK_DIV,       // set display clock divide ratio
         0x80,                           // div ratio of 1, standard freq
         SSD1306_SET_PRECHARGE,          // set pre-charge period
         0xF1,                           // Vcc internally generated on our board
         SSD1306_SET_VCOM_DESEL,         // set VCOMH deselect level
         0x30,                           // 0.83xVcc
         /* display */
         SSD1306_SET_CONTRAST,           // set contrast control
         0xFF,
         SSD1306_SET_ENTIRE_ON,          // set entire display on to follow RAM content
         SSD1306_SET_NORM_DISP,           // set normal (not inverted) display
         SSD1306_SET_CHARGE_PUMP,        // set charge pump
         0x14,                           // Vcc internally generated on our board
         SSD1306_SET_SCROLL | 0x00,      // deactivate horizontal scrolling if set. This is necessary as memory writes will corrupt if scrolling was enabled
         SSD1306_SET_DISP | 0x01, // turn display on
     };

     SSD1306_send_cmd_list(cmds, count_of(cmds));
 }
 
 void SSD1306_scroll(bool on) {
     // configure horizontal scrolling
     uint8_t cmds[] = {
         SSD1306_SET_HORIZ_SCROLL | 0x00,
         0x00, // dummy byte
         0x00, // start page 0
         0x00, // time interval
         0x03, // end page 3 SSD1306_NUM_PAGES ??
         0x00, // dummy byte
         0xFF, // dummy byte
         SSD1306_SET_SCROLL | (on ? 0x01 : 0) // Start/stop scrolling
     };

    
     SSD1306_send_cmd_list(cmds, count_of(cmds));
 }
 
 void render(uint8_t *buf, struct render_area *area, bool overRide) {
     // update a portion of the display with a render area
    uint8_t cmds[] = {
        SSD1306_SET_COL_ADDR,
        area->start_col,
        area->end_col,
        SSD1306_SET_PAGE_ADDR,
        area->start_page,
        area->end_page
    };

    /*
    I dont think its possible just from this to go further than this. This means you cna overlapping render areas which
    makes it easier, but if actual page areas are overlapping theres no way without saving more buffers (which we could do)
    to stop it getting weird when you're overlapping multiple sprites 
    
    whatever, this was a distraction anywya
    
    */
 {
    // if(overRide){

    //     for(int i = 0; i < area->buflen;i++){

    //         int posGlobal = xGlobal + (yGlobal * 128);


    //         if(buf[i] != bufferGlobal[posGlobal]){
    //             if(buf[i] + bufferGlobal[posGlobal] > 255){

    //                 buf[i] = 255;
    //                 bufferGlobal[posGlobal] = buf[i];
    
    //             }else{

    //                 buf[i] = buf[i] + bufferGlobal[posGlobal];
    //                 bufferGlobal[posGlobal] = buf[i];
                
    //             }   
    //         }



    //         xGlobal++;
    //         if(xGlobal >= area->start_col + width){
    //             xGlobal = area->start_col;
    //             yGlobal++;
    //         }
    //         //there might have to be a safety thing to shut it off if yGlobal gets too big or whatever.
    //     }    
    // }
 }

    //which I think is done somewhere already, surely,
    //but in any case, we'll have to run through the buffer, left to right, until you hit the end col, 
    //then go down, you know the drill

    //if its blank, leave it be, if its not, add them together or replace, depending on a bool passed in I guess

    //this probably remains the same, we're adjusting the buffer passed here and then adjusting the main buffer
    //after
    SSD1306_send_cmd_list(cmds, count_of(cmds));

    SSD1306_send_buf(buf, area->buflen);
 }

 void UpdateFromGlobal(){
 
    struct render_area frame_area = {
        start_col: 0,
        end_col : SSD1306_WIDTH - 1,
        start_page : 0,
        end_page : SSD1306_NUM_PAGES - 1
    };
    
    calc_render_area_buflen(&frame_area);


    render(bufferGlobal, &frame_area, false);


 }
 
 void SetPixel(uint8_t *buf, int x,int y, bool on) {
     assert(x >= 0 && x < SSD1306_WIDTH && y >=0 && y < SSD1306_HEIGHT);
 
     // The calculation to determine the correct bit to set depends on which address
     // mode we are in. This code assumes horizontal
 
     // The video ram on the SSD1306 is split up in to 8 rows, one bit per pixel.
     // Each row is 128 long by 8 pixels high, each byte vertically arranged, so byte 0 is x=0, y=0->7,
     // byte 1 is x = 1, y=0->7 etc
 
     // This code could be optimised, but is like this for clarity. The compiler
     // should do a half decent job optimising it anyway.
 
     const int BytesPerRow = SSD1306_WIDTH ; // x pixels, 1bpp, but each row is 8 pixel high, so (x / 8) * 8
 
     int byte_idx = (y / 8) * BytesPerRow + x;
     uint8_t byte = buf[byte_idx];
 
     if (on)
        byte |=  1 << (y % 8);
     else
        byte &= ~(1 << (y % 8));
 
     buf[byte_idx] = byte;
 }


 // Basic Bresenhams.
 static void DrawLine(uint8_t *buf, int x0, int y0, int x1, int y1, bool on) {
 
     int dx =  abs(x1-x0);
     int sx = x0<x1 ? 1 : -1;
     int dy = -abs(y1-y0);
     int sy = y0<y1 ? 1 : -1;
     int err = dx+dy;
     int e2;
 
     while (true) {
         SetPixel(buf, x0, y0, on);
         if (x0 == x1 && y0 == y1)
             break;
         e2 = 2*err;
 
         if (e2 >= dy) {
             err += dy;
             x0 += sx;
         }
         if (e2 <= dx) {
             err += dx;
             y0 += sy;
         }
     }
 }
 
 //if we keep adding letters, might want to refactor this



 static inline int GetFontIndex(uint8_t ch) {
    //I get it, it uses the int value of the character passed 
    //but what is it returning?

    //the character, minus 'a' (65) + 1 (67
    //its returning an int
    //I get it, its outputting an actual number between 1-26 representing the actual number
    //thats why it has the +1, to get around the blank letter at the start
    //but the actual logic for finding the letter in the array in ssd1306_font is handled later
    //so A is 65, if a C is passed in its 67 - 65 + 1 = 3

    if (ch >= 'A' && ch <='Z') {
        return  ch - 'A' + 1;
     }

     //but this one just removes 0, which is actually 48. 
     //so if 4 is added its 52 - 48 + 27 = 31
     //these seem to be skipping the empty space at the start actaully
     else if (ch >= '0' && ch <='9') {
        return  ch - '0' + 27;
     }else if(ch == '-'){
        return  37;
     }else if(ch == '/'){
        return  38;
     }else if(ch == '/'){
        return  39;
     }else if(ch == '!'){
        return  38;
     }else if(ch == '?'){
        return  38;
     }

     else return  0; // Not got that char so space.
 }
 


//for now, we'll just pass in an int that represents what we should be multiplying y by (64 for half the screen etc.)

//  static void WriteChar(uint8_t *buf,  int16_t x, int16_t y, uint8_t ch, int number, bool invert, int *counter) {
//      if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
//          return;

//      // For the moment, only write on Y row boundaries (every 8 vertical pixels)
//      y = y/8;
 
//      ch = toupper(ch);
//      int idx = GetFontIndex(ch);
//      int fb_idx = y * number + x;
 

//      for (int i=0;i<8;i++) {

//         uint8_t letter = font[idx * 8 + i];

//         if(invert){
//             letter = 255 - letter;
//         }
        

//         buf[*counter] = letter;

//         bufferGlobal[fb_idx++] |= letter;

//         *counter = *counter + 1;

//     }
//  }
 

static void WriteCharReworked(uint8_t *buf,  int16_t x, int16_t y, uint8_t ch, int* counter, bool invert) {
     if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
         return;

     // For the moment, only write on Y row boundaries (every 8 vertical pixels)

     ch = toupper(ch);
     int idx = GetFontIndex(ch);

     for (int i=0;i<8;i++) {

        uint8_t letter = font[idx * 8 + i];
        if(invert){
            letter = 255 - letter;
        }
        buf[*counter] = letter;

        *counter = *counter + 1;
    }
 }

void WriteStringBlock(uint8_t *buf,  int16_t x, int16_t y, const char *str, int longest, int* counter, bool invert) {
     // Cull out any string off the screen
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
        return;

 
    for(int i=0;i<longest;i++) {
        //the string should never, ever be longer than the longest variable
        //nevertheless, some safety would be good

        //if i>=strlen(str), send in a space, otherwise send in the letter
        WriteCharReworked(buf, x, y, (i>=strlen(str)) ? ' ' : str[i], counter, invert);

        x+=8;
    }
 }
 
 /// @brief loops through an array of strings (*char) and returns the length of the longest (not what the longest is)
/// @param text 
/// @return 
int GetLongestString(const char **text, int length){

    size_t longest = strlen(text[0]);

    for (int i = 1; i < length; ++i) {

        int len = strlen(text[i]);

        if (len > longest) {
            longest = len;
        }
    }   
    
    return longest;
}
 
 
void WriteString(uint8_t *buf,  int16_t x, int16_t y, const char *str, bool invert) {
     // Cull out any string off the screen
     if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
         return;

     int counterForThingy = 0;

    for(int i=0;i<strlen(str);i++) {
        //the string should never, ever be longer than the longest variable
        //nevertheless, some safety would be good

        //if i>=strlen(str), send in a space, otherwise send in the letter
        WriteCharReworked(buf, x, y,str[i], &counterForThingy, invert);
    }
 }


 #endif

  //We need to figre out two things in these functions:

  //the actual window size and position we're drawing to
  //the position of the thing within that window
  //last one is easy

  //TODO: how does the position work with the window size, is it relative or absolute



  //this doesn't work like text, we need to make the frame the same size as the image i think?

  //NOTES
  //changing this to make the center point whats passed in
  //TODO make this work properly

void DisplayImage(int posX, int posY, int width, int height, const uint8_t *hex){
#if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
#warning i2c / SSD1306_i2d example requires a board with I2C pins
    puts("Default I2C pins were not defined");
#else
    //useful information for picotool

    //Initialize render area for entire frame (SSD1306_WIDTH pixels by SSD1306_NUM_PAGES pages)

    int equalizer = 1 - (width % 2); //this allows odd-width sprites
 //   printf(equalizer);
    
    struct render_area frame_area = {
        start_col: posX,//posX - (width / 2),
        end_col : posX + width,

        start_page : posY,//round((posY - (height / 2)) / 8),
        
        end_page : posY + height / SSD1306_PAGE_HEIGHT,
    };


    calc_render_area_buflen(&frame_area);
    
    render(hex, &frame_area, true);        


#endif

}


void DisplayItemFoundText(int posX, int posY, char * letters){

    struct render_area frame_area = {
        start_col: 0,
        end_col : SSD1306_WIDTH - 1,
        start_page : 0,
        end_page : SSD1306_NUM_PAGES -1
    };


    calc_render_area_buflen(&frame_area);

    // zero the entire display
    uint8_t buf[SSD1306_BUF_LEN];
    memset(buf, 0, SSD1306_BUF_LEN);
    render(buf, &frame_area, false);

    //still need our method to re-center text
    WriteString(buf, 50, 32, letters, false);
}

//how do we actually get rid of frame_area now is the question

// void DisplayText(int posX, int posY, int frameStartPosX, int frameEndPosX, int frameStartPosY, 
//     int frameEndPosY, char * letters){
//     // SSD1306_send_cmd(SSD1306_SET_ALL_ON);    // Set all pixels on
//     //SSD1306_init();

//     //we have the size and position, but its not that simple
//     //size of the frame_area is based off the start positions, the end positions

//     //and we have the total size 
//     //if we want the width to be a quarter the width, and start in the middle
//     //we need the start col to be the middle (64) and the end col to be 64+32

//     //so to get the start_col: thats just the start col that we pass in
//     //and the end_col: start col+width
//     //same for the height stuff


//     struct render_area frame_area = {
//         start_col: frameStartPosX,
//         end_col : frameEndPosX,
//         start_page : frameStartPosY,
//         end_page : frameEndPosY
//     };

    
//     calc_render_area_buflen(&frame_area);

//     // zero the entire display
//     uint8_t buf[SSD1306_BUF_LEN];
//     memset(buf, 0, SSD1306_BUF_LEN);
//     render(buf, &frame_area, false);



//    // colors everything in
//     // for (int x = 0;x < SSD1306_WIDTH;x++) {
//     //     for (int y = SSD1306_HEIGHT-1; y >= 0 ;y--) {
//     //         SetPixel(buf,x,y,true);
            
//     //     }
//     // }

//     WriteString(buf, posX, posY, letters, 128,false);

//     int length = sizeof(buf) / sizeof(buf[0]);


// }





void DeleteWindow(int startCol, int endCol, int startPage, int endPage){
    struct render_area frame_area = {
        start_col: startCol,
        end_col : endCol ,
        start_page : startPage,
        end_page : endPage
        };


    calc_render_area_buflen(&frame_area);

    // zero the entire display

    uint8_t buf[frame_area.buflen];
    memset(buf, 0, frame_area.buflen);

    //colors everything in
    for (int x = 0;x < SSD1306_WIDTH-1;x++) {
        for (int y = SSD1306_HEIGHT-1; y >= 0 ;y--) {
            SetPixel(buf,x,y,false);
        }
    }
    render(buf, &frame_area,  false);
}


void DeleteScreen(){

    for(int i = 0; i < count_of(bufferGlobal);i++){
        bufferGlobal[i] = (uint8_t)0;
    }

    //UpdateFromGlobal();
}



void InitializeScreen(){


    #if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
#warning i2c / SSD1306_i2d example requires a board with I2C pins
    puts("Default I2C pins were not defined");
#else
    // useful information for picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));
    bi_decl(bi_program_description("SSD1306 OLED driver I2C example for the Raspberry Pi Pico"));

    i2c_init(i2c_default, SSD1306_I2C_CLK * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    // run through the complete initialization process
    printf("123\n");
    SSD1306_init();
    printf("456\n");

    #endif

}