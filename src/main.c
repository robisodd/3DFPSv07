/**********************************************************************************
   Pebble 3D FPS Engine v0.7 beta
   Created by Rob Spiess (robisodd@gmail.com) on June 23, 2014
  *********************************************************************************
  v0.1: Initial Release
  
  v0.2: Converted all floating points to int32_t which increased framerate substancially
        Gets overflow errors, so when a number squared is negative, sets it to 2147483647.
        Added door blocks for another example
  
  v0.3: Added distance shading
        Removed door and zebra blocks
        
  v0.4: Optimizations
  
  v0.5: Changed 1000x1000 pixel blocks to 64 x 64
        Changed all /1000 to >>6 for quicker division (*64 = <<6)
        Updated Square Root Function
        Added 64x64bit Textures
        Added Texture Distance Shading overlay
        Modified FOV to give more square view (not squished or stretched)
        Added mirror block and black block
        Select button shoots ray to change blocks
  
  v0.6: Isolated shootray function
        Created new 3D routine - more optimzied and works better?
        All textures 32x32bits -> Faster due to single 
        Repaired shoot_ray function (no longer need ceiling function)
        
  v0.7: Cleaned up mess
        Added comments to inform those how this thing kinda works
        ray.dist is now unsigned
        raycasting is more optimized
        Got rid of floor/ceil function
        Super optimized raycasting (got rid of square root)
        Added strafing (hold the DOWN button to strafe)
        BEEFED up the drawing routine (direct framebuffer writing)
        Added MazeMap Generation
        Added Floor/Ceiling casting
        
        
  To Do:
        Make texture pointer array
        Texture looping
  *********************************************************************************
  Created with the help of these tutorials:
    http://www.playfuljs.com/a-first-person-engine-in-265-lines/
    http://www.permadi.com/tutorial/raycast/index.html

  CC Copyright (c) 2014 All Right Reserved
  THIS CODE AND INFORMATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY 
  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
  http://creativecommons.org/licenses/by/3.0/legalcode
  
  *********************************************************************************
   Notes and Ideas
  *********************************************************************************
   Poisoned = Black Boxes and White Blocks -- i.e. Can't Detect Textures
   Night time = Darkness fades off closer.  Can only see 3 away unless torch is lit
   Full brightness = no darkness based on distance
   Mirror seems to reflect magic, too. Magic repulsing shield?
   Rain or Snow overlay
     Snow would eventually change ground to white, maybe raise lower-wall level when deep, slowing walking
     Rain would also increase darkness due to cloudcover.  Black sky?  Clouds in sky?
   Drunk or Poisoned mode: autowalk or autoturn or random strafing when walking or inverted accel.x
   IDCLIP on certain block types -- walk through blocks which look solid (enemies can't penetrate to expose - unless in chase mode?)
   
   
   Map[] int8_t  bit:76543210
     bit7 traversable: 1 yes, 0 no  Note: Map needs to be initialized as -1, not 0.
     bit6 sprite here: 1 yes, 0 no  Note: Sprites only exist where traversable?
  
  ERROR WITH CORNERS
  
  
  *********************************************************************************/
// 529a7262-efdb-48d4-80d4-da14963099b9
#include "pebble.h"

#define ACCEL_STEP_MS 10       // Update frequency
#define mapsize 20             // Map is 90x90 squares, or whatever number is here

#define RANGE 64 * 30          // Distance player can see - Pixels-per-square * #-of-squares -- max 1024 squares due to (64*1024)^2 = 32bit max
#define IDCLIP false           // Walk thru walls
#define view_border true       // Draw border around viewing window

//Draw Mode
#define DRAWMODE_TEXTURES true
#define DRAWMODE_LINES false
  
//----------------------------------//
// Viewing Window Size and Position //
//----------------------------------//
// beneficial to: (set fov as divisible by view_w) and (have view_w evenly divisible by 2)
// e.g.: view_w=144 is good since 144/2=no remainder. Set fov = 13104fov (since it = 144w x 91 and is close to 20% of 65536)
  
// Full Screen (You should also comment out drawing the text box)
//#define view_x 0             // View Left Edge
//#define view_y 0             // View Top Edge
//#define view_w 144           // View Width in pixels
//#define view_h 168           // View Hight in pixels
//#define fov 13104            // Field of view angle (20% of a circle is good) (TRIG_MAX_RATIO = 0x10000 or 65536) * 20%

// Smaller square
//#define view_x 20            // View Left Edge
//#define view_y 30            // View Top Edge
//#define view_w 100           // View Width in pixels
//#define view_h 100           // View Hight in pixels
//#define fov 13100            // Field of view angle (20% of a circle is good) (TRIG_MAX_RATIO = 0x10000 or 65536) * 20%

//Nearly full screen
//#define fov 13064              // Field of view angle (20% of a circle is good) (TRIG_MAX_RATIO = 0x10000 or 65536) * 20%


//----------------------------------//
//#define fov_over_w fov/view_w  // Do math now so less during execution
//#define half_view_w view_w/2   //
//#define half_view_h view_h/2
//----------------------------------//
 
  
int32_t view_x =     1;             // View Left Edge
int32_t view_y =    25;             // View Top Edge
int32_t view_w =   142;             // View Width in pixels
int32_t view_h =   128;             // View Hight in pixels
GRect view;
int32_t    fov = 10650;             // Field of view angle (20% of a circle is good) (TRIG_MAX_RATIO = 0x10000 or 65536) * 20%

  
typedef struct PlayerStruct {
  int32_t x;                  // Player's X Position x64
  int32_t y;                  // Player's Y Position x64
  int32_t facing;             // Player Direction Facing (from 0 - TRIG_MAX_ANGLE)
} PlayerStruct;
static PlayerStruct player;

typedef struct RayStruct {
   int32_t x;                 // x coordinate on map the ray hit
   int32_t y;                 // y coordinate on map the ray hit
  uint32_t dist;              // length of the ray / distance ray traveled
    int8_t hit;               // block type the ray hit
   int32_t offset;            // horizontal spot on texture the ray hit [0-63]
   uint8_t face;              // face of the block it hit (00=  , 01= , 10= , 11= )
} RayStruct;
static RayStruct ray;

static Window *window;
static GRect window_frame;
static Layer *graphics_layer;
static bool up_button_depressed = false;   // Whether Pebble's   Up   button is held
static bool dn_button_depressed = false;   // Whether Pebble's  Down  button is held
//static bool sl_button_depressed = false; // Whether Pebble's Select button is held
//static bool bk_button_depressed = false; // Whether Pebble's  Back  button is held

GBitmap *wBrick, *wCircle, *wFifty, *fTile, *cLights;
uint32_t *target;

static int8_t map[mapsize * mapsize];  // int8 means cells can be from -128 to 127

int32_t  sqrt_int(int32_t a, int8_t root_depth) {int32_t b=a; for(int8_t i=0; i<root_depth; i++) b=(b+(a/b))/2; return b;} // Square Root
int32_t   abs_int(int32_t a){return (a<0 ? 0 - a : a);} // Absolute Value

#define root_depth 10          // How many iterations square root function performs
int32_t  sqrt32(int32_t a) {int32_t b=a; for(int8_t i=0; i<root_depth; i++) b=(b+(a/b))/2; return b;} // Square Root

int32_t abs32(int32_t x) {return (x^(x>>31)) - (x>>31);}
int16_t abs16(int16_t x) {return (x^(x>>15)) - (x>>15);}
int8_t  abs8 (int8_t  x) {return (x^(x>> 7)) - (x>> 7);}

int8_t  sign8 (int8_t  x){return (x > 0) - (x < 0);}
int16_t sign16(int16_t x){return (x > 0) - (x < 0);}
int32_t sign32(int32_t x){return (x > 0) - (x < 0);}

int8_t mode = 0;

// ------------------------------------------------------------------------ //
//  Map Functions
// ------------------------------------------------------------------------ //
void GenerateRandomMap() {
  for (int16_t i=0; i<mapsize*mapsize; i++) map[i] = rand() % 3 == 0 ? 1 : 0;       // Randomly 1/3 of spots are normal [type 1] blocks
  for (int16_t i=0; i<mapsize*mapsize; i++) if(map[i]==1 && rand()%10==0) map[i]=2; // Changes 10% of normal blocks to [type 2] blocks
  //for (int16_t i=0; i<mapsize*mapsize; i++) if(map[i]==2 && rand()%2==0) map[i]=3;  // Changes 50% of [type 2] blocks to [type 3] blocks
}

// Generates maze starting from startx, starty, filling map with (0=empty, 1=wall, -1=special)
void GenerateMazeMap(int32_t startx, int32_t starty) {
  int32_t x, y;
  int8_t try;
  int32_t cursorx, cursory, next=1;
  
  cursorx = startx; cursory=starty;  
  for (int16_t i=0; i<mapsize*mapsize; i++) map[i] = 0; // Fill map with 0s
  
  while(true) {
    int32_t current = cursory * mapsize + cursorx;
    if((map[current] & 15) == 15) {  // If No Tries Left
      if(cursory==starty && cursorx==startx) {  // If back at the start, then we're done.
        map[current]=1;
        for (int16_t i=0; i<mapsize*mapsize; i++) map[i] = 1-map[i]; // invert map bits (0=empty, 1=wall, -1=special)
        return;
      }
      switch(map[current] >> 4) { // Else go back to the previous cell:  NOTE: If the 1st two bits are used, need to "&3" mask this
       case 0: cursorx++; break;
       case 1: cursory++; break;
       case 2: cursorx--; break;
       case 3: cursory--; break;
      }
      map[current]=next; next=1;
    } else {
      do try = rand()%4; while (map[current] & (1<<try));  // Pick Random Directions until that direction hasn't been tried
      map[current] |= (1<<try); // turn on bit in this cell saying this path has been tried
      // below is just: x=0, y=0; if(try=0)x=1; if(try=1)y=1; if(try=2)x=-1; if(try=3)y=-1;
      y=(try&1); x=y^1; if(try&2){y=(~y)+1; x=(~x)+1;} //  y = try's 1st bit, x=y with 1st bit xor'd (toggled).  Then "Two's Complement Negation" if try's 2nd bit=1
      
      // Move if spot is blank and every spot around it is blank (except where it came from)
      if((cursory+y)>0 && (cursory+y)<mapsize-1 && (cursorx+x)>0 && (cursorx+x)<mapsize-1) // Make sure not moving to or over boundary
        if(map[(cursory+y) * mapsize + cursorx + x]==0)                                    // Make sure not moving to a dug spot
          if((map[(cursory+y-1) * mapsize + cursorx+x]==0 || try==1))                      // Nothing above (unless came from above)
            if((map[(cursory+y+1) * mapsize + cursorx+x]==0 || try==3))                    // nothing below (unless came from below)
              if((map[(cursory+y) * mapsize + cursorx+x - 1]==0 || try==0))                // nothing to the left (unless came from left)
                if((map[(cursory+y) * mapsize + cursorx + x + 1]==0 || try==2)) {          // nothing to the right (unless came from right)
                  next=2;          
                  cursorx += x; cursory += y;                                              // All's good!  Let's move
                  map[cursory * mapsize + cursorx] |= ((try+2)%4) << 4; //record in new cell where ya came from -- the (try+2)%4 is because when you move west, you came from east
                }
    }
  } //End While True
}

int8_t getmap(int32_t x, int32_t y) {
  x=x>>6; y=y>>6;
  if (x<0 || x>=mapsize || y<0 || y>=mapsize) return -1;
  return map[(y * mapsize) + x];
}

void setmap(int32_t x, int32_t y, int8_t value) {
  x=x>>6; y=y>>6;
  if ((x >= 0) && (x < mapsize) && (y >= 0) && (y < mapsize))
    map[y * mapsize + x] = value;
}


// ------------------------------------------------------------------------ //

void walk(int32_t direction, int32_t distance) {
  int32_t dx = (cos_lookup(direction) * distance) / TRIG_MAX_RATIO;
  int32_t dy = (sin_lookup(direction) * distance) / TRIG_MAX_RATIO;
  if(getmap(player.x + dx, player.y) <= 0 || IDCLIP) player.x += dx;
  if(getmap(player.x, player.y + dy) <= 0 || IDCLIP) player.y += dy;
}

static void main_loop(void *data) {
  AccelData accel=(AccelData){.x=0, .y=0, .z=0};          // all three are int16_t
  accel_service_peek(&accel);                             // read accelerometer
  walk(player.facing, accel.y>>5);                        // walk based on accel.y  Technically: walk(accel.y * 64px / 1000);
  if(dn_button_depressed)                                 // if down button is held
    walk(player.facing + (TRIG_MAX_ANGLE/4), accel.x>>5); //   strafe
  else                                                    // else
    player.facing += (accel.x<<3);                        //   spin
  layer_mark_dirty(graphics_layer);                       // tell pebble to draw when it's ready
}

//shoot_ray(x, y, angle)
//  x, y = position on map to shoot the ray from
//  angle = direction to shoot the ray (in Pebble angle notation)
// returns int32_t: end result of the function
//              -1: Ray went longer than RANGE constant without hitting a block
//               0: Ray went out of bounds of the map before hitting a block
//               1: Successfully hit a block and stopped
//modifies: global RayStruct ray
int32_t shoot_ray(int32_t x, int32_t y, int32_t angle) {
  int32_t sin, cos, dx, dy, nx, ny;
  
  sin = sin_lookup(angle);
  cos = cos_lookup(angle);
  ray = (RayStruct){.x=x, .y=y};
    
  ny = sin>0 ? 64 : -1;
  nx = cos>0 ? 64 : -1;
  
  while(true) {
    dy = ny - (ray.y&63);
    dx = nx - (ray.x&63);
      
    if(abs32(dx * sin) < abs32(dy * cos)) {
      ray.x += dx;
      ray.y += ((dx * sin) / cos);
      ray.hit = getmap(ray.x, ray.y);
      if(ray.hit > 0) {               // if ray hits a wall (a block)
        if(ray.hit == 4) {            // if it hit a [block type 4] = mirror block
          cos = -1 * cos;             // Bounce ray off mirror (ray will continue)
        } else {
          ray.offset = ray.y&63;      // Get offset: offset is where on wall ray hits: 0 (left edge) to 63 (right edge)
          ray.dist = ((ray.x - x) << 16) / cos; // Distance ray traveled
          return 1;                   // Returning a "1" means "ray hit a wall"
        } // End else Mirror
      } // End if hit
    } else {
      ray.x += (dy * cos) / sin;
      ray.y += dy;
      ray.hit = getmap(ray.x, ray.y);
      if(ray.hit > 0) {               // if ray hits a wall (a block)
        if(ray.hit == 4) {            // if it hit a [block type 4] = mirror block
          sin = -1 * sin;             // Bounce ray off mirror (ray will continue)
        } else {
         ray.offset = ray.x&63;        // Get offset: offset is where on wall ray hits: 0 (left edge) to 63 (right edge)
         ray.dist = ((ray.y - y) << 16) / sin; // Distance ray traveled    <<16 = * TRIG_MAX_RATIO
         return 1;                     // Returning a "1" means "ray hit a wall"
        } // End else Mirror
      } // End if hit
    } // End else Xlen<Ylen
    
    if(ray.hit==-1) // if ray is out of bounds
      if((sin<0&&ray.y<0)||(sin>0&&ray.y>=(mapsize<<6))||(cos<0&&ray.x<0)||(cos>0&&ray.x>=(mapsize<<6))) return 0; // Returning "0" means ray ran out of bounds AND is going further out of bounds

    //if(ray.dist > RANGE) return -1;  // Stop ray after traveling too far result=-1;
  } //End While
}
// ------------------------------------------------------------------------ //

//uint8_t texture_point(int8_t hit, int32_t x, int32_t y) {
//  ((*target>> ((31-((i<<6)/colheight))))&1)
//}

void fill_window(GContext *ctx, uint8_t *data) {
  for(uint16_t y=0, yaddr=0; y<168; y++, yaddr+=20)
    for(uint16_t x=0; x<19; x++)
      ((uint8_t*)(((GBitmap*)ctx)->addr))[yaddr+x] = data[y%8];
}

static void draw_textbox(GContext *ctx, GRect textframe, char *text) {
    graphics_context_set_fill_color(ctx, 0);   graphics_fill_rect(ctx, textframe, 0, GCornerNone);  //Black Solid Rectangle
    graphics_context_set_stroke_color(ctx, 1); graphics_draw_rect(ctx, textframe);                //White Rectangle Border  
    graphics_context_set_text_color(ctx, 1);  // White Text
    graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_14), textframe, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);  //Write Text
}


// 1-pixel-per-square map:
//   for (int16_t x = 0; x < mapsize; x++) for (int16_t y = 0; y < mapsize; y++) {graphics_context_set_stroke_color(ctx, map[y*mapsize+x]>0?1:0); graphics_draw_pixel(ctx, GPoint(x, y));}
static void draw_map(GContext *ctx, GRect box, int32_t zoom) {
  // note: Currently doesn't handle drawing beyond screen boundaries
  uint32_t *ctx32 = ((uint32_t*)(((GBitmap*)ctx)->addr));
  uint32_t xbit;
  int32_t x, y, yaddr, xaddr, xonmap, yonmap, yonmapinit;
  
  xonmap = ((player.x*zoom)>>6) - (box.size.w/2);  // Divide by ZOOM to get map X coord, but rounds [-ZOOM to 0] to 0 and plots it, so divide by ZOOM after checking if <0
  yonmapinit = ((player.y*zoom)>>6) - (box.size.h/2);
  for(x=0; x<box.size.w; x++, xonmap++) {
    xaddr = (x+box.origin.x) >> 5;        // X memory address
    xbit = ~(1<<((x+box.origin.x) & 31)); // X bit shift level (normally wouldn't ~ it, but ~ is used more often than not)
    if(xonmap>=0 && xonmap<(mapsize*zoom)) {
      yonmap = yonmapinit;
      yaddr = box.origin.y * 5;           // Y memory address
      for(y=0; y<box.size.h; y++, yonmap++, yaddr+=5) {
        if(yonmap>=0 && yonmap<(mapsize*zoom)) {             // If within Y bounds
          if(map[(((yonmap/zoom)*mapsize))+(xonmap/zoom)]>0) //   Map shows a wall >0
            ctx32[xaddr + yaddr] |= ~xbit;                   //     White dot
          else                                               //   Map shows <= 0
            ctx32[xaddr + yaddr] &= xbit;                    //     Black dot
        } else {                                             // Else: Out of Y bounds
          ctx32[xaddr + yaddr] &= xbit;                      //   Black dot
        }
      }
    } else {                                // Out of X bounds: Black vertical stripe
      for(yaddr=box.origin.y*5; yaddr<((box.size.h + box.origin.y)*5); yaddr+=5)
        ctx32[xaddr + yaddr] &= xbit;
    }
  }

  graphics_context_set_fill_color(ctx, (time_ms(NULL, NULL) % 250)>125?0:1);                      // Flashing dot
  graphics_fill_rect(ctx, GRect((box.size.w/2)+box.origin.x - 1, (box.size.h/2)+box.origin.y - 1, 3, 3), 0, GCornerNone); // Square Cursor

  graphics_context_set_stroke_color(ctx, 1); graphics_draw_rect(ctx, GRect(box.origin.x-1, box.origin.y-1, box.size.w+2, box.size.h+2)); // White Border
}

// implement more options
//draw_3D_wireframe?  draw_3D_shaded?
static void draw_3D(GContext *ctx, GRect box) { //, int32_t zoom) {
  int32_t colheight, angle; //colh, z;
  uint32_t x, xaddr, xbit, yaddr;

  // Draw Box around view (not needed if fullscreen)
  if(view_border) {graphics_context_set_stroke_color(ctx, 1); graphics_draw_rect(ctx, GRect(box.origin.x-1, box.origin.y-1, box.size.w+2, box.size.h+2));}  //White Rectangle Border

  // Draw background
    // Umm... ok... A nice black background.  Done.  Next?
    //graphics_context_set_fill_color(ctx, 1); graphics_fill_rect(ctx, GRect(box.x, box.origin.y, box.size.w, box.size.h/2), 0, GCornerNone); // White Sky  (Lightning?  Daytime?)

  for(int16_t col = 0; col < box.size.w; col++) {  // Begin RayTracing Loop
    angle = (fov * (col - (box.size.w>>1))) / box.size.w;
    
    x = col+box.origin.x;  // X screen coordinate
    xaddr = x >> 5;  // X memory address
    xbit = (x & 31); // X bit shift level
    
    if(shoot_ray(player.x, player.y, player.facing + angle)==0) {  //Shoot rays out of player's eyes.  pew pew.
      // 0 means out of map bounds, never hit anything.  Draw horizion dot
      //graphics_context_set_stroke_color(ctx, 1);
      //graphics_draw_pixel(ctx, GPoint(col + box.origin.x, box.origin.y + (box.size.h/2)));

      yaddr = ((box.origin.y + (box.size.h/2)) * 5);   // Y Address = Y screen coordinate * 5
      ((uint32_t*)(((GBitmap*)ctx)->addr))[yaddr + xaddr] += (1 << xbit);

      colheight = 1;
    } else {
      //1 means hit a block.  Draw the vertical line!

      // Calculate amount of shade
      //z = (ray.dist * cos_lookup(angle)) / TRIG_MAX_RATIO;  // z = distance
      //z -= 64; if(z<0) z=0;   // Make everything 1 block (64px) closer (solid white without having to be nearly touching)
      //z = sqrt_int(z,10) >> 1; // z was 0-RANGE(max dist visible), now z = 0 to 12: 0=close 10=distant.  Square Root makes it logarithmic
      //z -= 2; if(z<0) z=0;    // Closer still (zWas=zNow: 0-64=0, 65-128=2, 129-192=3, 256=4, 320=6, 384=6, 448=7, 512=8, 576=9, 640=10)

      
      
      colheight = (box.size.h << 22) /  (ray.dist * cos_lookup(angle));  // Height of wall segment = box.size.h * wallheight * 64(the "zoom factor") / distance (distance =  ray.dist * cos_lookup(angle))
      if(colheight>box.size.h) colheight=box.size.h/2; else colheight=colheight/2;   // Make sure line isn't drawn beyond bounding box (also halve it cause of 2 32bit textures)
      
      // Texture the Ray hit, point to 1st half of texture (half, cause a 64x64px texture menas there's 2 uint32_t per row)
      switch(ray.hit) { // Convert this to an array of pointers in the future
        case 1: target = (uint32_t*)wBrick->addr + ray.offset * 2; break;
        case 2: target = (uint32_t*)wFifty->addr + ray.offset * 2; break;
        case 3: target = (uint32_t*)wCircle->addr + ray.offset * 2; break;
      }

      // Note: "+=" addition in lines below only work on a black background (assumes 0 in the bit position).
      for(int32_t i=0; i<colheight; i++) {
        //yaddr = ((box.origin.y + (box.size.h/2) -+ i) * 5);   // Y Address = Y screen coordinate * 5
        int32_t ch = (i * ray.dist * cos_lookup(angle)) / (TRIG_MAX_RATIO * box.size.h);
        ((uint32_t*)(((GBitmap*)ctx)->addr))[((box.origin.y + (box.size.h/2) - i) * 5) + xaddr] += (((*target >> (31-ch))&1) << xbit);  // Draw Top Half
        ((uint32_t*)(((GBitmap*)ctx)->addr))[((box.origin.y + (box.size.h/2) + i) * 5) + xaddr] += (((*(target+1)  >> ch)&1) << xbit);   // Draw Bottom Half
      }
    } // End If(Shoot_Ray)
    
    int32_t distancex, distancey, distance, mapx, mapy, texturex, texturey, yaddr;
    // Draw Floor/Ceiling
    if(colheight<box.size.h/2)
    for(int32_t i=colheight; i<box.size.h/2; i++) {
      distance = (box.size.h * 32 * TRIG_MAX_ANGLE) / (i * cos_lookup(angle));
      //go over 64, go down i, how many until hit floor (aka h/2)
      //(h/2) / i * 64
      // distance = (((box.size.h/2) * 64) / i) / (cos_lookup(angle)/TRIG_MAX_ANGLE);  // Divide by cos(angle) to un-fisheye
        
      distancex = (distance * cos_lookup(player.facing + angle))/TRIG_MAX_RATIO;
      distancey = (distance * sin_lookup(player.facing + angle))/TRIG_MAX_RATIO;
      mapx = player.x + distancex;
      mapy = player.y + distancey;
      texturex=mapx&63;
      texturey=mapy&31;
      if(getmap(mapx, mapy)>=0) {
        yaddr = ((box.origin.y + (box.size.h/2) + i) * 5);   // Y Address = Y screen coordinate * 5
        target = (uint32_t*)fTile->addr + texturex * 2;
        ((uint32_t*)(((GBitmap*)ctx)->addr))[yaddr + xaddr] += (((*target >> (texturey))&1) << xbit);
        
        yaddr = ((box.origin.y + (box.size.h/2) - i) * 5);   // Y Address = Y screen coordinate * 5
        target = (uint32_t*)cLights->addr + texturex * 2;
        ((uint32_t*)(((GBitmap*)ctx)->addr))[yaddr + xaddr] += (((*target >> (texturey))&1) << xbit);
      }
    } // End Floor/Ceiling
    
  } //End For (End RayTracing Loop)
}

static void graphics_layer_update_proc(Layer *me, GContext *ctx) {
  static char text[40];  //Buffer to hold text
  time_t sec1, sec2; uint16_t ms1, ms2, dt; // time snapshot variables, to calculate render time and FPS
  time_ms(&sec1, &ms1);  //1st Time Snapshot
  
  //draw_3D(ctx,  GRect(view_x, view_y, view_w, view_h));
  draw_3D(ctx,  view);
  draw_map(ctx, GRect(4, 110, 40, 40), 4);
  
  time_ms(&sec2, &ms2);  //2nd Time Snapshot
  dt = (uint16_t)(1000*(sec2 - sec1)) + (ms2 - ms1);  //dt=delta time: time between two time snapshots in milliseconds
  
  snprintf(text, sizeof(text), "(%ld,%ld) %ld %dms %dfps %d", player.x>>6, player.y>>6, player.facing, dt, 1000/dt, getmap(player.x,player.y));  // What text to draw
  draw_textbox(ctx, GRect(0, 0, 143, 20), text);
   
  //  Set a timer to restart loop in 50ms
  if(dt<40 && dt>0) // if time to render is less than 40ms, force framerate of 20FPS or worse
     app_timer_register(50-dt, main_loop, NULL); // 10FPS
  else
     app_timer_register(10, main_loop, NULL);     // took longer than 40ms, loop  in 10ms (asap)
}


// ------------------------------------------------------------------------ //
//  Button Click Handlers
// ------------------------------------------------------------------------ //
void up_push_in_handler(ClickRecognizerRef recognizer, void *context) {up_button_depressed = true;
                                                                      GenerateMazeMap(mapsize/2, 0);
                                                                      }
void up_release_handler(ClickRecognizerRef recognizer, void *context) {up_button_depressed = false;}
void dn_push_in_handler(ClickRecognizerRef recognizer, void *context) {dn_button_depressed = true;}
void dn_release_handler(ClickRecognizerRef recognizer, void *context) {dn_button_depressed = false;}
//void sl_push_in_handler(ClickRecognizerRef recognizer, void *context) {sl_button_depressed = true;}
//void sl_release_handler(ClickRecognizerRef recognizer, void *context) {sl_button_depressed = false;}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) { // SELECT button was pressed
  if(shoot_ray(player.x, player.y, player.facing)==1) {             // Shoot Ray from center of screen.  If it hit something:
    if(ray.hit==1) setmap(ray.x, ray.y, 3);   // If Ray hit normal block(1), change it to a Circle Block (3) (Changed from Mirror Block(4), as it was confusing)
    if(ray.hit==3) setmap(ray.x, ray.y, 1);   // If Ray hit Circle Block(3), change it to a Normal Block (1)
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  //window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  //window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_raw_click_subscribe(BUTTON_ID_UP, up_push_in_handler, up_release_handler, context);
  window_raw_click_subscribe(BUTTON_ID_DOWN, dn_push_in_handler, dn_release_handler, context);
  //window_raw_click_subscribe(BUTTON_ID_SELECT, sl_push_in_handler, sl_release_handler, context);
}

// ------------------------------------------------------------------------ //
//  Main Program Structure
// ------------------------------------------------------------------------ //

static void window_load(Window *window) {
  //wBrick = gbitmap_create_with_resource(RESOURCE_ID_WALL_BRICK);
  wBrick = gbitmap_create_with_resource(RESOURCE_ID_STONE);
  wFifty = gbitmap_create_with_resource(RESOURCE_ID_WALL_FIFTY);
  wCircle = gbitmap_create_with_resource(RESOURCE_ID_WALL_CIRCLE);
  fTile = gbitmap_create_with_resource(RESOURCE_ID_FLOOR_TILE);
  cLights = gbitmap_create_with_resource(RESOURCE_ID_CEILING_LIGHTS);
  
  Layer *window_layer = window_get_root_layer(window);
  window_frame = layer_get_frame(window_layer);

  graphics_layer = layer_create(window_frame);
  layer_set_update_proc(graphics_layer, graphics_layer_update_proc);
  layer_add_child(window_layer, graphics_layer);
}

static void window_unload(Window *window) {
  layer_destroy(graphics_layer);
  gbitmap_destroy(wBrick);
  gbitmap_destroy(wFifty);
  gbitmap_destroy(wCircle);
  gbitmap_destroy(fTile);
  gbitmap_destroy(cLights);
}

static void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_set_fullscreen(window, true);  // Get rid of the top bar
  window_stack_push(window, false /* False = Not Animated */);
  window_set_background_color(window, GColorBlack);
  accel_data_service_subscribe(0, NULL);  // Start accelerometer
  
  srand(time(NULL));  // Seed randomizer so different map every time
  GenerateRandomMap();                // Randomly generate a map
  //GenerateMazeMap(mapsize/2, 0);  // Randomly generate a maze
  player = (PlayerStruct){.x=(64*5), .y=(-2 * 64), .facing=10000};  // Seems like a good place to start
  player = (PlayerStruct){.x=(64*(mapsize/2)), .y=(-2 * 64), .facing=10000};
  view = GRect(1, 25, 142, 128);
  // MainLoop() automatically called with dirty layer drawing
}

static void deinit(void) {
  accel_data_service_unsubscribe();
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
