/* roadmap|_bar.c - Handle main bar
 *
 * LICENSE:
 *
 *   Copyright 2008 Avi B.S
 *   Copyright 2008 Ehud Shabtai
 *
 *   This file is part of RoadMap.
 *
 *   RoadMap is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   RoadMap is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with RoadMap; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "roadmap.h"
#include "roadmap_canvas.h"
#include "roadmap_screen_obj.h"
#include "roadmap_screen.h"
#include "roadmap_file.h"
#include "roadmap_res.h"
#include "roadmap_math.h"
#include "roadmap_time.h"
#include "roadmap_state.h"
#include "roadmap_message.h"
#include "roadmap_bar.h"
#include "roadmap_factory.h" 
#include "roadmap_start.h"
#include "roadmap_pointer.h"
#include "roadmap_softkeys.h"
#include "roadmap_skin.h"

#include "Realtime/RealtimeAlerts.h"

#define MAX_STATES 9
#define TOP_BAR_IMAGE 		"top_bar_background" 
#define BOTTOM_BAR_IMAGE 	"bottom_bar_background"
 
static RoadMapImage TopBarBgImage;
static RoadMapImage BottomBarBgImage;

void roadmap_bar_switch_skins(void);

static BOOL bar_initialized = FALSE;

#define TOPBAR_FLAG 	0x10000
#define BOTTOM_BAR_FLAG 0x20000

#define IMAGE_STATE_NORMAL   0
#define IMAGE_STATE_SELECTED 1


const char * get_time_str(void);
const char * get_current_street(void);
const char * get_time_to_destination(void);
const char * get_dist_to_destination(void);

#define MAX_OBJECTS 40

typedef struct {

    const char *name;

    RoadMapPen foreground;
    
    const char *default_foreground;
    
    RoadMapBarTextFn bat_text_fn;

} BarText; 

#define ROADMAP_TEXT(p,w,t) \
    {p, NULL, w, t}
     
BarText RoadMapTexts[] = {

    ROADMAP_TEXT("clock", "#303030", get_time_str),
    ROADMAP_TEXT("num_alerts", "#303030",RTAlerts_Count_Str),
    ROADMAP_TEXT("left_softkey", "#303030",roadmap_softkeys_get_left_soft_key_text),
    ROADMAP_TEXT("right_softkey", "#303030",roadmap_softkeys_get_right_soft_key_text),
    ROADMAP_TEXT("right_softkey", "#303030",get_current_street),
    ROADMAP_TEXT("time_to_dest", "#303030",get_time_to_destination),
    ROADMAP_TEXT("dist_to_dest", "#303030",get_dist_to_destination),
    ROADMAP_TEXT(NULL, NULL, NULL)
};

typedef struct {
   char           *name;
   const char     *image_file[MAX_STATES];
   int 			  pos_x;
   int 			  pos_y;	
   RoadMapImage   images[MAX_STATES];
   RoadMapImage   image_selected[MAX_STATES];
   const char     *state;
   int            states_count;		
   RoadMapStateFn state_fn;
   BarText		  *bar_text;
   int			  font_size;
   int 			  text_alling;
   char 		  *text_color;
   RoadMapGuiRect bbox;
   const RoadMapAction *action;
   RoadMapStateFn  condition_fn;
   int			   condition_value;
   int 			   image_state;
} BarObject;

typedef struct {
	BarObject *object[MAX_OBJECTS];
	int		   draw_bg;
	int		   count;
} BarObjectTable_s;


static BarObjectTable_s TopBarObjectTable;
static BarObjectTable_s BottomBarObjectTable;


static BarText * roadmap_bar_text_find(const char *name){
   BarText *text;

    for (text = RoadMapTexts; text->name != NULL; ++text) {
        if (strcmp (text->name, name) == 0) {
            return text;
        }
    }
    roadmap_log (ROADMAP_ERROR, "unknown bar text '%s'", name);
    return NULL;
	
}

static void roadmap_bar_pos (BarObject *object,
                             RoadMapGuiPoint *pos) {

   pos->x = object->pos_x;
   pos->y = object->pos_y;

   if (pos->x < 0) 
      pos->x += roadmap_canvas_width ();

   if (pos->y < 0) 
      pos->y += roadmap_canvas_height ();
}

static char *roadmap_bar_object_string (const char *data, int length) {

    char *p = malloc (length+1);

    roadmap_check_allocated(p);

    strncpy (p, data, length);
    p[length] = 0;

    return p;
}

static void roadmap_bar_decode_arg (char *output, int o_size,
                                           const char *input, int i_size) {

   int size;

   o_size -= 1;
      
   size = i_size < o_size ? i_size : o_size;

   strncpy (output, input, size);
   output[size] = '\0';
}


static void roadmap_bar_decode_integer (int *value, int argc,
                                               const char **argv, int *argl) {

   char arg[255];

   argc -= 1;

   if (argc != 1) {
      roadmap_log (ROADMAP_ERROR, "roadmap bar: illegal integer value - %s",
                  argv[0]);
      return;
   }

   roadmap_bar_decode_arg (arg, sizeof(arg), argv[1], argl[1]);
   *value = atoi(arg);
}  
      
static void roadmap_bar_decode_position
                        (BarObject *object,
                         int argc, const char **argv, int *argl) {

   char arg[255];
   int pos;

   argc -= 1;
   if (argc != 2) {
      roadmap_log (ROADMAP_ERROR, "roadmap bar:'%s' illegal position.",
                  object->name);
      return;
   }

   roadmap_bar_decode_arg (arg, sizeof(arg), argv[1], argl[1]);
   pos = atoi(arg);
   object->pos_x = pos;

   roadmap_bar_decode_arg (arg, sizeof(arg), argv[2], argl[2]);
   pos = atoi(arg);
   object->pos_y = pos;
}

static void roadmap_bar_decode_icon
                        (BarObject *object,
                         int argc, const char **argv, int *argl) {

   int i;

   argc -= 1;

   if (object->states_count > MAX_STATES) {
      roadmap_log (ROADMAP_ERROR, "roadmap bar:'%s' has too many states.",
                  object->name);
      return;
   }

   for (i = 1; i <= argc; ++i) {
      RoadMapImage image = NULL;
      char arg[256];
      
      roadmap_bar_decode_arg (arg, sizeof(arg), argv[i], argl[i]);

      image = roadmap_res_get (RES_BITMAP, RES_SKIN, arg);

      if (image == NULL) {
            roadmap_log (ROADMAP_ERROR,
                  "roadmap bar:'%s' can't load image:%s.",
                  object->name,
                  arg);
      }
      else
      	if (i == 1)
       		 object->images[object->states_count] = image;
        else
        	object->image_selected[object->states_count] = image;
   }

   object->image_state = IMAGE_STATE_NORMAL;
   object->states_count++;
}


static void roadmap_bar_decode_state
                        (BarObject *object,
                         int argc, const char **argv, int *argl) {

   char arg[255];

   argc -= 1;
   if (argc != 1) {
      roadmap_log (ROADMAP_ERROR, "roadmap bar:'%s' illegal state indicator.",
                  object->name);
      return;
   }

   roadmap_bar_decode_arg (arg, sizeof(arg), argv[1], argl[1]);

   object->state_fn = roadmap_state_find (arg);

   if (!object->state_fn) {
      roadmap_log (ROADMAP_ERROR,
                  "roadmap bar:'%s' can't find state indicator.",
                  object->name);
   }
}


static void roadmap_bar_decode_text_color
                        (BarObject *object,
                         int argc, const char **argv, int *argl) {

   char arg[255];

   argc -= 1;
   if (argc != 1) {
      roadmap_log (ROADMAP_ERROR, "roadmap bar:'%s' illegal text color.",
                  object->name);
      return;
   }

   roadmap_bar_decode_arg (arg, sizeof(arg), argv[1], argl[1]);

   object->text_color = roadmap_bar_object_string (argv[1], argl[1]);
}
static void roadmap_bar_decode_condition
                        (BarObject *object,
                         int argc, const char **argv, int *argl) {

   char arg[255];

   argc -= 1;
   if (argc != 2) {
      roadmap_log (ROADMAP_ERROR, "roadmap bar:'%s' illegal condition indicator.",
                  object->name);
      return;
   }

   roadmap_bar_decode_arg (arg, sizeof(arg), argv[1], argl[1]);

   object->condition_fn = roadmap_state_find (arg);

   if (!object->condition_fn) {
      roadmap_log (ROADMAP_ERROR,
                  "roadmap bar:'%s' can't find condition indicator.",
                  object->name);
      return;
   }
   
   roadmap_bar_decode_arg (arg, sizeof(arg), argv[2], argl[2]);
   object->condition_value = atoi(arg);
}

static void roadmap_bar_decode_text
                        (BarObject *object,
                         int argc, const char **argv, int *argl) {

   char arg[255];

   argc -= 1;
   if (argc != 1) {
      roadmap_log (ROADMAP_ERROR, "roadmap bar:'%s' illegal condition indicator.",
                  object->name);
      return;
   }

   roadmap_bar_decode_arg (arg, sizeof(arg), argv[1], argl[1]);

   object->bar_text = roadmap_bar_text_find (arg);

}
static BarObject *roadmap_bar_new
          (int argc, const char **argv, int *argl) {

   BarObject *object = calloc(sizeof(*object), 1);

   roadmap_check_allocated(object);

   object->name = roadmap_bar_object_string (argv[1], argl[1]);
   object->text_alling = -1;
   object->text_color = NULL;

   return object;
}


static void roadmap_bar_decode_bbox
                        (BarObject *object,
                         int argc, const char **argv, int *argl) {

   char arg[255];

   argc -= 1;
   if (argc != 4) {
      roadmap_log (ROADMAP_ERROR, "roadmap bar:'%s' illegal bbox.",
                  object->name);
      return;
   }

   roadmap_bar_decode_arg (arg, sizeof(arg), argv[1], argl[1]);
   object->bbox.minx = atoi(arg);
   roadmap_bar_decode_arg (arg, sizeof(arg), argv[2], argl[2]);
   object->bbox.miny = atoi(arg);
   roadmap_bar_decode_arg (arg, sizeof(arg), argv[3], argl[3]);
   object->bbox.maxx = atoi(arg);
   roadmap_bar_decode_arg (arg, sizeof(arg), argv[4], argl[4]);
   object->bbox.maxy = atoi(arg);
}

static void roadmap_bar_decode_action
                        (BarObject *object,
                         int argc, const char **argv, int *argl) {

   char arg[255];

   argc -= 1;
   if (argc < 1) {
      roadmap_log (ROADMAP_ERROR, "roadmap bar:'%s' illegal action.",
                  object->name);
      return;
   }

   roadmap_bar_decode_arg (arg, sizeof(arg), argv[1], argl[1]);

   object->action = roadmap_start_find_action (arg);

   if (!object->action) {
      roadmap_log (ROADMAP_ERROR, "roadmap bar:'%s' can't find action.",
                  object->name);
   }
}

static BarObject *roadmap_bar_by_pos (RoadMapGuiPoint *point, BarObjectTable_s *table) {

   int i;
   int condition;
   
   for (i = 0; i < table->count; i++) {

      RoadMapGuiPoint pos;
      roadmap_bar_pos(table->object[i], &pos);

      if ((point->x >= (pos.x + table->object[i]->bbox.minx)) &&
          (point->x <= (pos.x + table->object[i]->bbox.maxx)) &&
          (point->y >= (pos.y + table->object[i]->bbox.miny)) &&
          (point->y <= (pos.y + table->object[i]->bbox.maxy))) {

			if (table->object[i]->condition_fn){
				condition = (*table->object[i]->condition_fn) ();
    			if (condition !=  table->object[i]->condition_value)
    			continue;
    		}
				
         return table->object[i];
      }
   }

   return NULL;
}

static void roadmap_bar_load (const char *data, int size, BarObjectTable_s *BarObjectTable) {

   int argc;
   int argl[256];
   const char *argv[256];

   const char *p;
   const char *end;

   BarObject *object = NULL;


   end  = data + size;

   while (data < end) {

      while (data[0] == '#' || data[0] < ' ') {

         if (*(data++) == '#') {
            while ((data < end) && (data[0] >= ' ')) data += 1;
         }
         while (data < end && data[0] == '\n' && data[0] != '\r') data += 1;
      }

      argc = 1;
      argv[0] = data;

      for (p = data; p < end; ++p) {

         if (*p == ' ') {
            argl[argc-1] = p - argv[argc-1];
            argv[argc]   = p+1;
            argc += 1;
            if (argc >= 255) break;
         }

         if (p >= end || *p < ' ') break;
      }
      argl[argc-1] = p - argv[argc-1];

      while (p < end && *p < ' ' && *p > 0) ++p;
      data = p;

      if (object == NULL) {

         if (argv[0][0] != 'N') {
            roadmap_log (ROADMAP_ERROR, "roadmap bar name is missing");
            break;
         }

      }

      switch (argv[0][0]) {

      case 'P':

         roadmap_bar_decode_position (object, argc, argv, argl);
         break;

      case 'I':

         roadmap_bar_decode_icon (object, argc, argv, argl);
         break;

     case 'A':

         roadmap_bar_decode_action (object, argc, argv, argl);
         break;

      case 'B':

         roadmap_bar_decode_bbox (object, argc, argv, argl);
         break;

      case 'S':

         roadmap_bar_decode_state (object, argc, argv, argl);
         break;

      case 'N':
         object = roadmap_bar_new (argc, argv, argl);
         
         BarObjectTable->object[BarObjectTable->count] = object;
         BarObjectTable->count++;
         break;
      case 'C':
      	 roadmap_bar_decode_condition (object, argc, argv, argl);
         break;
           
      case 'T':
      	roadmap_bar_decode_text (object, argc, argv, argl);
      	break;
      	
      case 'F':
      	roadmap_bar_decode_integer (&object->font_size, argc, argv,
                                             argl);
        break;
        
     case 'L':
     	roadmap_bar_decode_integer(&object->text_alling, argc, argv,
                                             argl);   
        break;
     case 'D':
      	roadmap_bar_decode_text_color (object, argc, argv, argl);
        break;     
        
	  case 'Y':
      	roadmap_bar_decode_integer (&BarObjectTable->draw_bg, argc, argv,
                                             argl);
        break;	
      }


	  
      while (p < end && *p < ' ') p++;
      data = p;
   }
}

const char * get_time_str(void){
	time_t now;
	now = time(NULL);
	return roadmap_time_get_hours_minutes(now);
}

const char * get_current_street(void){
	static char text[256];
	roadmap_message_format (text, sizeof(text), "%Y");
	return &text[0];
	
}


const char *get_time_to_destination(void){
  static char text[256];

  	if (roadmap_message_format (text, sizeof(text), "%A|%T"))
  		return &text[0];
  	return "";
}
  	
const char *get_dist_to_destination(void){
  static char text[256];

  	if (roadmap_message_format (text, sizeof(text), "%D (%W)|%D"))
  		return &text[0];
  	return "";
}
  	
void draw_objects(BarObjectTable_s *table){
   int font_size;
   int i;
   int state, condition;
   RoadMapGuiPoint TextLocation;
   int text_flag = ROADMAP_CANVAS_LEFT|ROADMAP_CANVAS_TOP ;
   
	for (i=0; i < table->count; i++) {
    	RoadMapGuiPoint ObjectLocation; 
    	if (table->object[i] == NULL)
    		continue;
    	
    	if (table->object[i]->condition_fn){
    		condition = (*table->object[i]->condition_fn) ();
    		if (condition !=  table->object[i]->condition_value)
    			continue;
    	}
    	
		roadmap_bar_pos(table->object[i], &ObjectLocation);
		if (table->object[i]->state_fn) {
      		state = (*table->object[i]->state_fn) ();
      		if ((state < 0) || (state >= MAX_STATES)){
   			}
   			else{
   				if (table->object[i]->images[state] != NULL){
   					if ((table->object[i]->image_state == IMAGE_STATE_SELECTED) && (table->object[i]->image_selected[state] != NULL))
   						roadmap_canvas_draw_image (table->object[i]->image_selected[state], &ObjectLocation, 0,IMAGE_NORMAL);	
   					else 
   						roadmap_canvas_draw_image (table->object[i]->images[state], &ObjectLocation, 0,IMAGE_NORMAL);
   				}
   			}
		}
		else{
			if ((table->object[i]->image_state == IMAGE_STATE_SELECTED) && (table->object[i]->image_selected[0] != NULL)){
				roadmap_canvas_draw_image (table->object[i]->image_selected[0], &ObjectLocation, 0,IMAGE_NORMAL);
				
			}
			else if (table->object[i]->images[0])
	   			roadmap_canvas_draw_image (table->object[i]->images[0], &ObjectLocation, 0,IMAGE_NORMAL);
		}

    	if (table->object[i]->bar_text){
    		
    		if (table->object[i]->font_size != 0) 
    			font_size = table->object[i]->font_size;
    		else
    			font_size = 10;
    		
    		if (table->object[i]->text_alling != -1){
    				text_flag = table->object[i]->text_alling;
    		}
    		else if (table->object[i]->pos_x < 0){
    			text_flag = ROADMAP_CANVAS_RIGHT|ROADMAP_CANVAS_TOP;
    		}
    			
    		roadmap_canvas_create_pen (table->object[i]->bar_text->name);
    		if (table->object[i]->text_color){
    			roadmap_canvas_set_foreground (table->object[i]->text_color);
    		}
    		else{
    			if (roadmap_skin_state())
					roadmap_canvas_set_foreground ("#ffffff");		
				else			    			
    				roadmap_canvas_set_foreground (table->object[i]->bar_text->default_foreground);
    		}
    		
			if (table->object[i]->state_fn) {
    	  		state = (*table->object[i]->state_fn) ();
      			if (state >0){
      				if (table->object[i]->images[state]){
      					TextLocation.x = ObjectLocation.x + roadmap_canvas_image_width(table->object[i]->images[state])/2;
      					TextLocation.y = ObjectLocation.y + roadmap_canvas_image_height(table->object[i]->images[state])/2;
      				}
      				else
						roadmap_bar_pos(table->object[i], &TextLocation);
  					roadmap_canvas_draw_string_size (&TextLocation,
       			    			                    text_flag,
           			    			                font_size,(*table->object[i]->bar_text->bat_text_fn)());
      			}
			}
			else{
				if (table->object[i]->images[0]){
      				TextLocation.x = ObjectLocation.x + roadmap_canvas_image_width(table->object[i]->images[0])/2;
      				TextLocation.y = ObjectLocation.y + roadmap_canvas_image_height(table->object[i]->images[0])/2 -2;
				}
				else
					roadmap_bar_pos(table->object[i], &TextLocation);
  				roadmap_canvas_draw_string_size (&TextLocation,
       		    			                    text_flag,
       			    			                font_size,(*table->object[i]->bar_text->bat_text_fn)());
			}	
    	}           		    	                
    }
}

void roadmap_bar_draw_top_bar (BOOL draw_bg) {
	int i;
	int num_images;
	int image_width;
	int width;

 
	width = roadmap_canvas_width ();
	
		
	if (!bar_initialized)
		return;
		
	if (TopBarObjectTable.draw_bg && draw_bg){
	
		
		image_width = roadmap_canvas_image_width(TopBarBgImage);
		
		num_images = width / image_width;
		
		for (i = 0; i < num_images; i++){
			RoadMapGuiPoint BarLocation;
			BarLocation.y = 0;
			BarLocation.x = i * image_width;
		   	roadmap_canvas_draw_image (TopBarBgImage, &BarLocation, 0,IMAGE_NORMAL);
		}
	}
	draw_objects(&TopBarObjectTable);    
}

int roadmap_bar_short_click (RoadMapGuiPoint *point) {
   
   BarObject *object = roadmap_bar_by_pos(point, &TopBarObjectTable);

   if (!object) {
   	  object = roadmap_bar_by_pos(point, &BottomBarObjectTable);
   	  if (!object)
      	return 0;
   }

   if (object->action) {
      static RoadMapSoundList list;
   
	  object->state = IMAGE_STATE_NORMAL;
	  roadmap_screen_redraw();

      if (!list) {
         list = roadmap_sound_list_create (SOUND_LIST_NO_FREE);
         roadmap_sound_list_add (list, "click");
         roadmap_res_get (RES_SOUND, 0, "click");
      }

      roadmap_sound_play_list (list);
	  
	  roadmap_screen_redraw();
	  
      (*(object->action->callback)) ();
      
      roadmap_screen_redraw();
   }

   return 1;
}

int roadmap_bar_long_click (RoadMapGuiPoint *point) {
   
   BarObject *object = roadmap_bar_by_pos(point, &TopBarObjectTable);

   if (!object) {
   	  object = roadmap_bar_by_pos(point, &BottomBarObjectTable);
   	  if (!object)
      	return 0;
   }

   if (object->action) {
//      static RoadMapSoundList list;
//   
//      if (!list) {
//         list = roadmap_sound_list_create (SOUND_LIST_NO_FREE);
//         roadmap_sound_list_add (list, "click");
//         roadmap_res_get (RES_SOUND, 0, "click");
//      }

//      roadmap_sound_play_list (list);
	  
	  roadmap_screen_redraw();
      (*(object->action->callback)) ();
      
      roadmap_screen_redraw();
   }

   return 1;
}

int roadmap_bar_obj_pressed (RoadMapGuiPoint *point) {
   
   BarObject *object = roadmap_bar_by_pos(point, &TopBarObjectTable);

   if (!object) {
   	  object = roadmap_bar_by_pos(point, &BottomBarObjectTable);
   	  if (!object)
      	return 0;
   }
   
   object->image_state = IMAGE_STATE_SELECTED;
   roadmap_screen_redraw();
   return 1;
}

int roadmap_bar_obj_released (RoadMapGuiPoint *point) {
   
   BarObject *object = roadmap_bar_by_pos(point, &TopBarObjectTable);

   if (!object) {
   	  object = roadmap_bar_by_pos(point, &BottomBarObjectTable);
   	  if (!object)
      	return 0;
   }
   
   object->image_state = IMAGE_STATE_NORMAL;
  roadmap_screen_redraw();
   return 1;
}

void roadmap_bar_draw_bottom_bar (BOOL draw_bg) {
	int i;
	int num_images;
	int image_width, image_height;
	int screen_width, screen_height;
//	int width, ascent, descent;
//	RoadMapGuiPoint TextLocation;
//	char *text;
	
	screen_width = roadmap_canvas_width ();
	screen_height = roadmap_canvas_height();
	
	if (!bar_initialized)
		return;
		
	image_width = roadmap_canvas_image_width(BottomBarBgImage);
	image_height = roadmap_canvas_image_height(BottomBarBgImage);
	
	if (BottomBarObjectTable.draw_bg && draw_bg){
		num_images = screen_width / image_width;
		for (i = 0; i < num_images; i++){
			RoadMapGuiPoint BarLocation;
			BarLocation.y = screen_height - image_height;
			BarLocation.x = i * image_width;
		   roadmap_canvas_draw_image (BottomBarBgImage, &BarLocation, 0,
	         IMAGE_NORMAL);
		}
	}	
	draw_objects(&BottomBarObjectTable); 
	
}

void roadmap_bar_draw(void){
	if (!bar_initialized)
		return;
		
	roadmap_bar_draw_top_bar(TRUE);
	roadmap_bar_draw_bottom_bar(TRUE);
}

void roadmap_bar_draw_objects(void){
	if (!bar_initialized)
		return;
		
	roadmap_bar_draw_bottom_bar(TRUE);
}

void roadmap_bar_set_mode (int flags) {
   int x_offset;
   int y_offset;

   x_offset = 0;
   y_offset = roadmap_canvas_image_height (TopBarBgImage);

   if (flags & TOPBAR_FLAG) {
      roadmap_screen_obj_offset (x_offset, y_offset);
      roadmap_screen_move_center (-y_offset / 2);
   } 
   
   if (flags & BOTTOM_BAR_FLAG){
      roadmap_screen_obj_offset (-x_offset, -y_offset);
      roadmap_screen_move_center (y_offset / 2);
   }
   
}

static void roadmap_bar_after_refresh (void) {

    roadmap_bar_draw ();
    
}

void roadmap_bar_initialize(void){
	int width;
	int image_width;
	int i;
	const char *cursor;
    RoadMapFileContext file;
   
	TopBarObjectTable.count = 0;
	
	for (i=0; i< MAX_OBJECTS;i++){
		TopBarObjectTable.object[i] = NULL;
		BottomBarObjectTable.object[i] = NULL;
	}
	TopBarObjectTable.draw_bg = TRUE;
	BottomBarObjectTable.draw_bg = TRUE;
	
	width = roadmap_canvas_width ();
	TopBarBgImage = (RoadMapImage) roadmap_res_get(RES_BITMAP, RES_SKIN, TOP_BAR_IMAGE);
	if (TopBarBgImage == NULL){
		return;
	}

	BottomBarBgImage = (RoadMapImage) roadmap_res_get(RES_BITMAP, RES_SKIN, BOTTOM_BAR_IMAGE);
	if (BottomBarBgImage == NULL){
		return;
	}

	// Load top bar
	cursor = roadmap_file_map ("skin", "top_bar", NULL, "r", &file);
	if (cursor == NULL){
		roadmap_log (ROADMAP_ERROR, "roadmap bar top_bar file is missing");
		return;
	}

    roadmap_bar_load(roadmap_file_base(file), roadmap_file_size(file), &TopBarObjectTable);
    
    roadmap_file_unmap (&file);

    // Load bottom bar
    cursor = roadmap_file_map ("skin", "bottom_bar", NULL, "r", &file);
	if (cursor == NULL){
		roadmap_log (ROADMAP_ERROR, "roadmap bottom top_bar file is missing");
		return;
	}
    
    roadmap_bar_load(roadmap_file_base(file), roadmap_file_size(file), &BottomBarObjectTable);
    
    roadmap_file_unmap (&file);
       	
	image_width = roadmap_canvas_image_width(TopBarBgImage);
	
	roadmap_screen_subscribe_after_refresh (roadmap_bar_after_refresh);
	
	roadmap_bar_set_mode(TOPBAR_FLAG);
	
	roadmap_pointer_register_short_click
      (roadmap_bar_short_click, POINTER_HIGH);
	
	roadmap_pointer_register_long_click
      (roadmap_bar_long_click, POINTER_HIGH);

	roadmap_pointer_register_pressed
 		(roadmap_bar_obj_pressed, POINTER_HIGH);
	
	roadmap_pointer_register_released
 		(roadmap_bar_obj_released, POINTER_HIGH);

	roadmap_skin_register (roadmap_bar_switch_skins);
	
	bar_initialized = TRUE;
}

int roadmap_bar_top_height(){
	if (TopBarBgImage == NULL){
		return 0;
	}
	else
		return roadmap_canvas_image_height(TopBarBgImage);
}

int roadmap_bar_bottom_height(){
	if (BottomBarBgImage == NULL){
		return 0;
	}
	else
		return roadmap_canvas_image_height(BottomBarBgImage);
}

void roadmap_bar_switch_skins(void){

	TopBarBgImage = (RoadMapImage) roadmap_res_get(RES_BITMAP, RES_SKIN|RES_NOCACHE, TOP_BAR_IMAGE);

	BottomBarBgImage = (RoadMapImage) roadmap_res_get(RES_BITMAP, RES_SKIN|RES_NOCACHE, BOTTOM_BAR_IMAGE);

#ifdef IPHONE
   roadmap_main_adjust_skin (roadmap_skin_state());
#endif //IPHONE	

}
