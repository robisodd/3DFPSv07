/**********************************************************************************
   Pebble 3D FPS Engine v0.5
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
  
  To Do:
        See if "rendering all in 1 stroke color, then rendering again with 2nd" is faster
          e.g. draw all white points in a column then all black points
        Make texture pointer array
        Completely redo casting
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
     Rain would also increase darkness due to cloudcover.  Black sky?
   
   Map[] int8_t  bit:76543210
     bit7 traversable: 1 yes, 0 no  Note: Map needs to be initialized as -1, not 0.
     bit6 sprite here: 1 yes, 0 no  Note: Sprites only exist where traversable?
  
  *********************************************************************************/
// 529a7262-efdb-48d4-80d4-da14963099b9
#include "pebble.h"

#define ACCEL_STEP_MS 10       // Update frequency
#define root_depth 10          // How many iterations square root function performs. Less=faster but less accurate
#define mapsize 90             // Map is 90x90 squares, or whatever number is here

#define range 64 * 30          // Distance player can see - Pixels-per-square * #-of-squares
#define idclip false           // Walk thru walls
#define view_border true       // Draw border around viewing window
#define draw_textbox true      // Draw textbox or not

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
int32_t view_h =   140;             // View Hight in pixels
int32_t    fov = 10650;             // Field of view angle (20% of a circle is good) (TRIG_MAX_RATIO = 0x10000 or 65536) * 20%

  
typedef struct PlayerVar {
  int32_t x;                  // Player's X Position x64
  int32_t y;                  // Player's Y Position x64
  int32_t facing;           // Player Direction Facing (from 0 - TRIG_MAX_ANGLE)
} PlayerVar;
static PlayerVar player;

typedef struct RayVar {
  int32_t x;                  // Origin X
  int32_t y;                  // Origin Y
  int32_t dist;               // Distance
  int8_t hit;                 // Hit (What on the map it hit)
  int32_t offset;             // Offset (used for wall texture)
} RayVar;
static RayVar ray;

static Window *window;
static GRect window_frame;
static Layer *graphics_layer;
//static AppTimer *timer;

GBitmap *wBrick, *wCircle, *wFifty;
uint32_t *target;
int32_t colh=0;

static int8_t map[mapsize * mapsize];  // int8 means cells can be from -128 to 127

// Floor: (x>>6)<<6) and x&(-64), probably x&!63 too
int32_t floor_int(int32_t a){int32_t b=(a-a%64); if(b!=a) if(a<0)b-=64; return b;} // Floors to nearest multiple of 64
int32_t  sqrt_int(int32_t a){int32_t b=a; for(int8_t i=0; i<root_depth; i++) b=(b+(a/b))/2; return b;} // Square Root
int32_t   abs_int(int32_t a){return (a<0 ? 0 - a : a);} // Absolute Value
//int32_t playerheight = 0;

// ------------------------------------------------------------------------ //
//  Map Functions
// ------------------------------------------------------------------------ //
void GenerateMap() {
  for (int16_t i=0; i<mapsize*mapsize; i++) map[i] = rand() % 3 == 0 ? 1 : 0;       // Randomly 1/3 of spots are normal [type 1] blocks
  for (int16_t i=0; i<mapsize*mapsize; i++) if(map[i]==1 && rand()%10==0) map[i]=2; // Changes 10% of normal blocks to [type 2] blocks
  //for (int16_t i=0; i<mapsize*mapsize; i++) if(map[i]==2 && rand()%2==0) map[i]=3;  // Changes 50% of [type 2] blocks to [type 3] blocks
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

void walk(int32_t distance) {
  int32_t dx = (cos_lookup(player.facing) * distance) / TRIG_MAX_RATIO;
  int32_t dy = (sin_lookup(player.facing) * distance) / TRIG_MAX_RATIO;
  if(getmap(floor_int(player.x + dx), floor_int(player.y)) <= 0 || idclip) player.x += dx;
  if(getmap(floor_int(player.x), floor_int(player.y + dy)) <= 0 || idclip) player.y += dy;
}

static void main_loop(void *data) {
  AccelData accel=(AccelData){.x=0, .y=0, .z=0}; // All three are int16_t
  accel_service_peek(&accel);                    // Read Accelerometer
  player.facing += (10 * accel.x);               // Spin based on accel.x
  walk((int32_t)(accel.y>>4));                   // Walk based on accel.y  Technically: walk(accel.y * 64px / 1000);
  layer_mark_dirty(graphics_layer);              // Tell pebble to draw when it's ready
}

int32_t shoot_ray(int32_t x, int32_t y, int32_t angle) {
  int32_t sin, cos, result=0;
  int32_t Xdx=0, Xdy=0, Ydx=0, Ydy=0, Xlen, Ylen;

  sin = sin_lookup(angle);
  cos = cos_lookup(angle); 
  ray = (RayVar){.x=x, .y=y, .dist=0};
  
  bool going = true;  // Loop until something causes you to stop
    while(going) {
      // Calculate distance to next X gridline in the ray's direction
      if (cos == 0)
        Xlen = 2147483647;  // If ray is vertical, will never hit next X
      else {
        Xdx = cos > 0 ? (floor_int(ray.x) - ray.x) + 64 : (floor_int(ray.x) - ray.x) - 1;
        Xdy = (Xdx * sin) / cos;
        Xlen = Xdx * Xdx + Xdy * Xdy;  // Multiplying 2 32bit numbers might overflow
        if(Xlen<0) Xlen = 2147483647;  // Overflow detected: just make length the max
      }

      // Calculate distance to next Y gridline in the ray's direction
	    if (sin == 0)
        Ylen = 2147483647;    // If ray is horizontal, will never hit next Y
	    else {
        Ydy = sin > 0 ? (floor_int(ray.y) - ray.y) + 64 : (floor_int(ray.y) - ray.y) - 1;
	      Ydx = (Ydy * cos)/sin;
        Ylen = Ydx * Ydx + Ydy * Ydy;  // Multiplying 2 32bit numbers might overflow
        if(Ylen<0) Ylen = 2147483647;  // Overflow detected: just make length the max
      }

      // move ray to next step whichever is closer
	    if(Xlen < Ylen) {
        ray.x += Xdx;
        ray.y += Xdy;
	      //ray.hit = getmap(floor_int(ray.x - (cos<0?64:0)), floor_int(ray.y));
        ray.hit = getmap(floor_int(ray.x), floor_int(ray.y));
        ray.dist = ray.dist + sqrt_int(Xlen);
	      ray.offset = ray.y;
      } else {
        ray.x += Ydx;
        ray.y += Ydy;
        //ray.hit = getmap(floor_int(ray.x),floor_int(ray.y - (sin<0?64:0)));
        ray.hit = getmap(floor_int(ray.x), floor_int(ray.y));
        ray.dist = ray.dist + sqrt_int(Ylen);
	      ray.offset = ray.x;
	    }

	    if (ray.hit > 0) {	   // if ray hits a wall (a block)
        if(ray.hit==4) {     // if it hit a [block type 4] = mirror block
            //graphics_context_set_stroke_color(ctx, 0); graphics_draw_line(ctx, GPoint((int)col + view_x, view_y + view_h/2 + colh), GPoint((int)col + view_x,view_y + view_h/2 - colh));  //Draw black line.  Cool for black block
            if(Xlen < Ylen) cos = -1 * cos; else sin = -1 * sin;  // Bounce ray off mirror, continue ray
        } else {
          going = false;       // stop ray
          result = 1;
          ray.offset &= 63;  // (was: ray.offset%64) Get fractional part of offset: offset is where on wall ray hits: 0 (left edge) to 63 (right edge)
        } // End Else Mirror
      } // End if hit

      if((sin<0&&ray.y<0)||(sin>0&&ray.y>=(mapsize<<6))||(cos<0&&ray.x<0)||(cos>0&&ray.x>=(mapsize<<6))) { going=false; result = 0;} // stop if ray is out of bounds AND going wrong way result=0;
      if(ray.dist > range) {result = -1; going=false;}  // Stop ray after traveling too far result=-1;
    } //End While
  return result;
}

/*
Notes:
If going positive, next multiple of 64
if going negative, truncates and subtracts one

If going positive, linear length = 64 - (position & 63) (aka: len = 64 - pos%64), then 64 each time after
If going negative, linear length =  1 + (position & 63) (aka: len =  1 + pos%64), then 64 each time after

*/

/*
int32_t shoot_ray2(int32_t StartX, int32_t StartY, int32_t angle) {
  int32_t Xdx=0, Xdy=0, Ydx=0, Ydy=0, Xlen, Ylen;
  int32_t sin, cos, result=0;
  int32_t fincx, Ay, Xinc, Bx, By, Yinc;
  
  sin = sin_lookup(angle);
  cos = cos_lookup(angle);
  ray = (RayVar){.x=x, .y=y, .dist=0};

  
  PosX=StartX;
  PosY=StartY;
  / * Maybe try:
  If cos = 0 or sin = 0
    while(casting) { }
  If cos>0
   If sin>0
    while(casting) { }
   Else
    while(casting) { }
  else
   If sin>0
    while(casting) { }
   Else
    while(casting) { }
  * /
  while (casting) {
    if(cos!=0 || sin!=0) {
      if(sin>0) { // facing down Y+
        Hdy = 64 - (PosY & 63);
      } else { // facing up Y-
        Hdy = 1 + (PosY & 63);
      }
      Hdx = (fDy * cos)/sin;
      if(Hdx<0) Hdx = 0 - Hdx; // Absolute Value

      //Next Vdx = abs (cos<<6)/sin;

      if(cos>0) { // facing right X+
        Vdx = 64 - (PosX & 63);
      } else { // facing left X-
        Vdx = 1 + (PosX & 63);
      }
      Vdy = (Vdx * sin)/cos;
      if(Vdy<0) Vdy = 0 - Vdy; // Absolute Value

      //Next Vdy = abs (sin<<6)/cos;
  
      if( ((Vdx*Vdx)+(Vdy*Vdy)) > ((Hdx*Hdx)+(Hdy*Hdy)) ) {
        if(cos<0) PosX -= Hdx; else PosX += Hdx;
        if(sin<0) PosY -= Hdy; else PosY += Hdy;
      } else {
        if(cos<0) PosX -= Vdx; else PosX += Vdx;
        if(sin<0) PosY -= Vdy; else PosY += Vdy;
      }

    } else { // Either cos or sin = 0
      if(cos==0){
        if(sin>0)  // facing straight down Y+
          PosY += 64 - (PosY & 63);
        else  // facing straight up Y-
          PosY -= 1 + (PosY & 63);
      } else {    // sin==0, facing straight left or right
        if(cos>0)  // facing straight right X+
          PosX += 64 - (PosX & 63);
         else  // facing straight left X-
          PosX -= 1 + (PosX & 63);
        
      }
    
    }
    
    
    
  
  
  
  
  
  // Find point A
  //ay=((y>>6)<<6); if sin>0 ya=ya+64 else ya=ya-1
  if(sin>0) { // facing down Y+
    Ay = ((y>>6)++)<<6;
  } else if(sin<0) { // facing up Y-
    Ay = ((y>>6)<<6)--;
  } else { // sin=0, Facing left or right
    //ray.y = y;
  }
  FirstDy = (Ay - y)
    ((Dy * cos)/sin);
  MainDx = (cos<<6)/sin;
  
  if(cos>0) { // facing right X+
    Ax = ((x>>6)++)<<6;
  } else if(cos<0) { // facing left X-
    Ax = ((x>>6)<<6)--;
  } else { // sin=0, Facing up or down
    //ray.y = y;
  }
  FirstDx = (Ax - x)
  FirstYinc = ((Dx * sin)/cos);
  MainXinc = (cos<<6)/sin;
  
  
  
    
  bool going = true;  // Loop until something causes you to stop
    while(going) {
      // Calculate distance to next X gridline in the ray's direction
      if (cos == 0)
        Xlen = 2147483647;  // If ray is vertical, will never hit next X
      else {
	      Xdx = cos > 0 ? floor_int(ray.x + 64) - ray.x : ceil_int(ray.x - 64) - ray.x;
        Xdy = (Xdx * sin)/cos;
        Xlen = Xdx * Xdx + Xdy * Xdy;  // Multiplying 2 32bit numbers might overflow
        if(Xlen<0) Xlen = 2147483647;  // Overflow detected so just make length the max
      }

      // Calculate distance to next Y gridline in the ray's direction
	    if (sin == 0)
        Ylen = 2147483647;    // If ray is horizontal, will never hit next Y
	    else {
        Ydy = sin > 0 ? floor_int(ray.y + 64) - ray.y : ceil_int(ray.y - 64) - ray.y;
	      Ydx = (Ydy * cos)/sin;
        Ylen = Ydx * Ydx + Ydy * Ydy;  // Multiplying 2 32bit numbers might overflow
        if(Ylen<0) Ylen = 2147483647;  // Overflow detected so just make length the max
      }

      // move ray to next step whichever is closer
	    if(Xlen < Ylen) {
        ray.x += Xdx;
        ray.y += Xdy;
	      ray.hit = getmap(floor_int(ray.x - (cos<0?64:0)), floor_int(ray.y));
        ray.dist = ray.dist + sqrt_int(Xlen);
	      ray.offset = ray.y;
      } else {
        ray.x += Ydx;
        ray.y += Ydy;
        ray.hit = getmap(floor_int(ray.x),floor_int(ray.y - (sin<0?64:0)));
        ray.dist = ray.dist + sqrt_int(Ylen);
	      ray.offset = ray.x;
	    }

	    if (ray.hit > 0) {	   // if ray hits a wall
        if(ray.hit==4) {     // if it hit a mirror
            //graphics_context_set_stroke_color(ctx, 0); graphics_draw_line(ctx, GPoint((int)col + view_x, view_y + view_h/2 + colh), GPoint((int)col + view_x,view_y + view_h/2 - colh));  //Draw black line.  Cool for black block
            if(Xlen < Ylen) cos = -1 * cos; else sin = -1 * sin;
        } else {
          going = false;       // stop ray
          result = 1;
          ray.offset &= 63;  // (was=ray.offset%64) Get fractional part of offset: offset is where on wall ray hits: 0-3(left) to 60-63(right)
        } // End Else Mirror
      } // End if hit

      if((sin<0&&ray.y<0)||(sin>0&&ray.y>=(mapsize<<6))||(cos<0&&ray.x<0)||(cos>0&&ray.x>=(mapsize<<6))) { going=false; result = 0;} // stop if ray is out of bounds AND going wrong way result=0;
      if(ray.dist > range) {result = -1; going=false;}  // Stop ray after traveling too far result=-1;
    } //End While
  return result;
}

*/
// ------------------------------------------------------------------------ //

//uint8_t texture_point(int8_t hit, int32_t x, int32_t y) {
//  ((*target>> ((31-((i<<6)/colheight))))&1)
//}


static void graphics_layer_update_proc(Layer *me, GContext *ctx) {
  int32_t coltop=0, colbot=0, colheight, z, angle;
  
  time_t sec1, sec2; uint16_t ms1, ms2; int32_t dt; // time snapshot variables, to calculate render time and FPS
  time_ms(&sec1, &ms1);  //1st Time Snapshot

  // Draw background
    // Umm... ok... A nice black background.  Done.  Next?
  
  // Draw Box around view (not needed if fullscreen, i.e. view_w=144 and view_h=168 and view_x=view_y=0)
  if(view_border) {graphics_context_set_stroke_color(ctx, 1); graphics_draw_rect(ctx, GRect(view_x-1, view_y-1, view_w+2, view_h+2));}  //White Rectangle Border
  
  // Bring me the horizon
  graphics_context_set_stroke_color(ctx, 1); graphics_draw_line(ctx, GPoint(view_x, view_y + view_h/2), GPoint(view_x + view_w,view_y + view_h/2));
   
  // Begin RayTracing Loop
  for(int16_t col = 0; col < view_w; col++) {
    angle = (fov/view_w) * (col - (view_w/2));  // was: angle = (int32_t)(fov * (((float)col/view_w) - 0.5));
    // note: above, when view_w becomes variable, this equation should be: angle=(fov * (col - (view_w<<1))) / view_w

    switch(shoot_ray(player.x, player.y, player.facing + angle)) {  //Shoot rays out of player's eyes.  pew pew.
//    if(z!=0) { // If Z is non-zero
    case -1:
      //if(z==-1) {
        graphics_context_set_stroke_color(ctx, 0); graphics_draw_pixel(ctx, GPoint(col + view_x, view_y + (view_h/2)));
      break;
      //} else {  //z=-1 means too far.  Draw black dot over the horizion
        case 1:
        z = (ray.dist * cos_lookup(angle)) / TRIG_MAX_RATIO;  // reuse z = distance
        if(z==0) z++; // If distance=0, make it 1 so divide by 0 doesn't happen later
        //Ray: x, y, dist, hit, offset

        // Draw Wall Column
       if(DRAWMODE_LINES) { // Begin Simple Lines Drawing
          colheight = (view_h << 6)/ z; // Height of wall segment = view_h * wallheight * 64 / distance
          colbot = (view_h/2) + (colheight / 2);
          coltop = (view_h/2) - (colheight / 2);
          if(ray.offset<4 || ray.offset > 60) graphics_context_set_stroke_color(ctx, 0); else   // Black edges on left and right 5% of block (Comment this line to remove edges)
            graphics_context_set_stroke_color(ctx, 1);
          graphics_draw_line(ctx, GPoint((int)col + view_x,coltop + view_y), GPoint((int)col + view_x,colbot + view_y));  //Draw the line
        } // End DrawMode Lines
        

        if(DRAWMODE_TEXTURES) { // Begin Texture Drawing
          colheight = (view_h << 6)/ z;                                             // Height of wall segment
          if(colheight>view_h) colh=view_h/2; else colh=colheight/2; // Make sure line isn't drawn beyond bounding box
          
          // Next three lines are for tweaking shading
          z -= 64; if (z<0) z=0;  // Make everything closer (solid white without having to be nearly touching)
          z=sqrt_int(z) >> 1;     // Distance. z was 0-RANGE(aka 640), now z = 0 to 12: 0=close 10=distant.  Square Root makes it logarithmic
          z -= 2; if (z<0) z=0;   // Closer still (zWas=zNow: 0-64=0, 65-128=2, 129-192=3, 256=4, 320=6, 384=6, 448=7, 512=8, 576=9, 640=10)

          for(int32_t i=0; i<colh; i++) {
            // Texture the Ray hit, point to 1st half of texture (64px x 64px texture menas 2 uint32_t per line)
            switch(ray.hit) { // Convert this to an array of pointers in the future
              case 1: target = (uint32_t*)wBrick->addr + ray.offset * 2; break;
              case 2: target = (uint32_t*)wFifty->addr + ray.offset * 2; break;
              case 3: target = (uint32_t*)wCircle->addr + ray.offset * 2; break;
            }
          
            
          //colbot = (view_h/2) + (colheight / 2); coltop = (view_h/2) - (colheight / 2);  // Normal
          //colbot = (view_h/2) + (colheight*1/8); coltop = (view_h/2) - (colheight*7/8);  // Rodent
		      //colbot = (view_h/2) + (colheight*7/8); coltop = (view_h/2) - (colheight*1/8);  // Flying

            
            // Draw Top Half
            //if(((col*6) + 180 - i)%9<z) graphics_context_set_stroke_color(ctx, 0); else           // Shading (Comment this line out to disable shading)
              graphics_context_set_stroke_color(ctx, ((*target>> ((31-((i<<6)/colheight))))&1));  // texture point = i*64/(colheight/2)
              //graphics_context_set_stroke_color(ctx, texture_point(ray.hit, ray.offset, (i<<6)/colheight ));  // texture point = i*64/(colheight/2)
            graphics_draw_pixel(ctx, GPoint(col+view_x, view_y + (view_h/2) - i ));
          
            // Draw Bottom Half
            target++; // Point to second half of texture
            //if((i+(col*6))%9<z) graphics_context_set_stroke_color(ctx, 0); else             // Shading (Comment this line out to disable shading)
              graphics_context_set_stroke_color(ctx, ((*target >> ((i<<6)/colheight))&1));  // texture point = i*64/colheight
            graphics_draw_pixel(ctx, GPoint(col+view_x, view_y + (view_h/2) +  i));
          }
        } // End DrawMode Shaded Textures
     // } //End Else Z = -1
    } // End If(Z)
  } //End For (End RayTracing Loop)

  time_ms(&sec2, &ms2);  //2nd Time Snapshot
  dt = ((int32_t)1000*(int32_t)sec2 + (int32_t)ms2) - ((int32_t)1000*(int32_t)sec1 + (int32_t)ms1);  //ms between two time snapshots
  
  //-----------------//
  // Display TextBox //
  //-----------------//
  if(draw_textbox) {
    GRect textframe = GRect(0, 0, 143, 20);  // Text Box Position and Size
    graphics_context_set_fill_color(ctx, 0);   graphics_fill_rect(ctx, textframe, 0, GCornerNone);  //Black Solid Rectangle
    graphics_context_set_stroke_color(ctx, 1); graphics_draw_rect(ctx, textframe);                //White Rectangle Border  
    static char text[40];  //Buffer to hold text
    snprintf(text, sizeof(text), " (%d,%d) %dms %dfps %d", (int)(player.x>>6), (int)(player.y>>6),(int)dt, (int)(1000/dt),(int)getmap(player.x,player.y));  // What text to draw
    graphics_context_set_text_color(ctx, 1);  // White Text
    graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_14), textframe, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);  //Write Text
  }
  
  //-----------------//
  //      Done!      //
  //  Set a timer to //
  //   restart loop  //
  //-----------------//
  if(dt<90 && dt>0) // Force 10FPS or worse
     app_timer_register(100-dt, main_loop, NULL); // 10FPS
  else
     app_timer_register(10, main_loop, NULL);     // worse
  //app_timer_register(ACCEL_STEP_MS, main_loop, NULL);
  
  // Perhaps clean up variables here?
}

// ------------------------------------------------------------------------ //
//  Button Click Handlers
// ------------------------------------------------------------------------ //
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {// UP
	//playerheight = playerheight + 1;	if(playerheight>view_h) playerheight=view_h;
  view_h -= 2; if(view_h<2) view_h=2; else view_x += 1;
  view_w -= 2; if(view_w<2) view_w=2; else view_y += 1;
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Select");
  //window_set_background_color(window, rand()%2);
  int32_t blockhit;
  blockhit = shoot_ray(player.x, player.y, player.facing);         // Shoot Ray from center of screen
  if(blockhit==1) setmap(floor_int(ray.x), floor_int(ray.y), 3);   // If Ray hit normal block(1), change it to a Circle Block (3) (Changed from Mirror Block(4), as it was confusing)
  if(blockhit==3) setmap(floor_int(ray.x), floor_int(ray.y), 1);   // If Ray hit Circle Block(3), change it to a Normal Block (1)
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	//playerheight = playerheight - 1;	if(playerheight<0) playerheight=0;
  view_h += 2; if(view_h>180) view_h=180; else view_x -= 1;
  view_w += 2; if(view_w>150) view_w=150; else view_y -= 1;
}

// ------------------------------------------------------------------------ //

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
  wBrick = gbitmap_create_with_resource(RESOURCE_ID_WALL_BRICK);
  wFifty = gbitmap_create_with_resource(RESOURCE_ID_WALL_FIFTY);
  wCircle = gbitmap_create_with_resource(RESOURCE_ID_WALL_CIRCLE);
  
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
  player = (PlayerVar){.x=(64*5), .y=(-2 * 64), .facing=10000};  // Seems like a good place to start
  GenerateMap();      // Randomly generate a map
  app_timer_register(ACCEL_STEP_MS, main_loop, NULL);  // Begin main loop
}

static void deinit(void) {
  accel_data_service_unsubscribe();
  window_destroy(window);
}

int main(void) {
  init();
  //const uint16_t count = 15;  APP_LOG(APP_LOG_LEVEL_DEBUG, "Init Done: %u", count);
  app_event_loop();
  deinit();
}
