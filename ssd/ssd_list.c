/* ssd_icon_list.c - list view widget
 *
 * LICENSE:
 *
 *   Copyright 2006 Ehud Shabtai
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
 *
 * SYNOPSYS:
 *
 *   See ssd_icon_list.h.
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "ssd_dialog.h"
#include "ssd_container.h"
#include "ssd_button.h"
#include "ssd_text.h"
#include "roadmap_keyboard.h"
#include "roadmap_softkeys.h"
#include "roadmap_lang.h"

#include "roadmap_screen.h"


#include "ssd_list.h"

#define MAX_ROWS 50
#define MIN_ROW_HEIGHT 42

static int ssd_list_drag_start (SsdWidget widget, const RoadMapGuiPoint *point);
static int ssd_list_drag_end (SsdWidget widget, const RoadMapGuiPoint *point) ;
static int ssd_list_drag_motion (SsdWidget widget,const RoadMapGuiPoint *point);
static int next_button_callback (SsdWidget widget, const char *new_value);

static RoadMapGuiPoint drag_start_point;

typedef struct tag_ssd_list_data
{

   int                        alloc_rows;
   int                        num_rows;
   roadmap_input_type         input_type;
   SsdWidget*                 rows;
   SsdIconListCallback        callback;
   SsdIconListDeleteCallback  del_callback;
   PFN_WIDGET_ONKEYPRESSED    on_unhandled_key_press;
   int                        num_values;
   const char**               labels;
   const void**               data;
   const char**               icons;
   const int* 				  flags;
   int                        first_row_index;
   int                        selected_row;
   SsdWidget		          widget_before_list;
   SsdWidget		          widget_after_list;
   int 				          min_row_height;
   SsdSize                    list_size;
   void (*list_container_draw) (SsdWidget widget, RoadMapGuiRect *rect, int flags);
   BOOL 					  add_next_button;
   
}  ssd_list_data, *ssd_list_data_ptr;



static roadmap_input_type get_input_type( SsdWidget this)
{
   //SsdWidget         list = this->parent->parent;
   ssd_list_data_ptr data = (ssd_list_data_ptr)this->data;

   return data->input_type;
}

static void setup_list_rows(ssd_list_data_ptr list)
{
   int current_index = list->first_row_index;
   int i;
   SsdWidget button;
   const char *button_icon[2];
   SsdWidget icon_container;
   SsdWidget next_icon_container;
   
   if (!list->rows) return;
      
   for( i=0; i<list->num_rows; i++)
   {
      SsdWidget row = list->rows[i];
      const char *label; 
      
      row->flags &= ~SSD_POINTER_COMMENT;
      
      if ((list->flags) && (i<list->num_values))
      	row->flags |= list->flags[current_index];

      button_icon[0] = NULL;
      if( current_index == list->num_values)
      {
         row->flags     &= ~SSD_WS_TABSTOP;
         row->tab_stop   = FALSE;
         label           = "";
      }
      else
      {
         label = list->labels[current_index];
         if (list->icons != NULL)
            button_icon[0] = list->icons[current_index];
         else
            button_icon[0] = NULL;
            
         row->flags     |= SSD_WS_TABSTOP;            
         row->tab_stop   = TRUE;

         button_icon[1] = NULL;
         current_index++;
      }

     ssd_widget_set_value (row, "label", label);
     icon_container = ssd_widget_get (row, "icon_container");
     next_icon_container = ssd_widget_get (row, "next_icon_container");
     
     if (list->icons == NULL){
           ssd_widget_hide(icon_container);
           if (next_icon_container)
           	 ssd_widget_hide(next_icon_container);
     }
     else
     {
        ssd_widget_show(icon_container);
        if (next_icon_container)
             ssd_widget_show(next_icon_container);
             
        button = ssd_widget_get(icon_container,"icon");
        if (button != NULL){
             if (button_icon[0] != NULL){
                ssd_button_change_icon(button, button_icon,1);
                ssd_widget_show(button);
                if (next_icon_container){
               	  SsdWidget next_button =  ssd_widget_get(next_icon_container,"next_icon");
               	  if ((next_button) && (list->add_next_button))
               		 ssd_widget_show(next_button);
                  else
                  	ssd_widget_hide(next_button);
                }
             }
             else{
               if (next_icon_container){
               	 SsdWidget next_button =  ssd_widget_get(next_icon_container,"next_icon");
               	 if (next_button)
               		ssd_widget_hide(next_button);
               }
               ssd_widget_hide(button);
             }
        }
        else{
           if (button_icon[0] != NULL){
               button = ssd_button_new ("icon","icon", button_icon, 1, SSD_ALIGN_CENTER | SSD_ALIGN_VCENTER, NULL);
                ssd_widget_add (icon_container,  button);
                if (next_icon_container){
                	const char *next_button_icon[1] = {"list_left"};
                	SsdWidget button_next = ssd_button_new ("next_icon","next_icon", next_button_icon, 1, SSD_ALIGN_VCENTER, next_button_callback);
         			ssd_widget_add (next_icon_container,  button_next);    
                }
           }
        }                  
     }     
     
     
     
     row = row->next;
   }
}

BOOL ssd_list_scroll_one_page_up( SsdWidget this, ssd_list_data_ptr data)
{
   data->first_row_index -= data->num_rows;
   
   if( data->first_row_index < 0)
      data->first_row_index = 0;
      
   setup_list_rows( data);
   return TRUE;
}

BOOL ssd_list_scroll_one_page_down( SsdWidget this, ssd_list_data_ptr data)
{
   if( (data->first_row_index + data->num_rows) >= data->num_values)
      return FALSE;

   data->first_row_index += data->num_rows;
   
   setup_list_rows( data);
   return TRUE;
}

BOOL ssd_list_scroll_list_begin( SsdWidget this, ssd_list_data_ptr data)
{
   data->first_row_index = 0;
   setup_list_rows( data);
   return TRUE;
}

BOOL ssd_list_scroll_list_end(SsdWidget this, ssd_list_data_ptr data)
{
   int last_partial_page_size = (data->num_values % data->num_rows);
   
   if( last_partial_page_size)
      data->first_row_index = data->num_values - last_partial_page_size;
   else
   {
      if(data->num_values)
         data->first_row_index = data->num_values - data->num_rows;
      else
         data->first_row_index = 0;
   }
      
   setup_list_rows( data);
   return TRUE;
}


static int button_callback (SsdWidget widget, const char *new_value) {

   ssd_list_data_ptr data;
   SsdWidget list = widget;

   /* Find the list main widget */
   while (strcmp(list->name, "list_container") &&
          strcmp(list->name, "scroll_container")) list = list->parent;
   list = list->parent;

   data = (ssd_list_data_ptr)list->data;

   if (!strcmp(widget->name, "scroll_up")) {
      ssd_list_scroll_one_page_up( widget, data);
      return 1;
   }

   if (!strcmp( widget->name, "scroll_down")) {
      ssd_list_scroll_one_page_down( widget, data);
         return 1;
      }

   return 0;
}


static int label_callback (SsdWidget widget, const char *new_value) {
   SsdWidget list = widget->parent->parent;
   SsdWidget text = ssd_widget_get (widget, "label");
   ssd_list_data_ptr data;
   int i;
   BOOL found = FALSE;

   data = (ssd_list_data_ptr)list->data;

   if (!data->callback) return 0;

   data->selected_row = -1;

   for (i=0; i<data->num_values; i++) {
      if (!strcmp(data->labels[i], text->value)) {
         data->selected_row = i;
         found = TRUE;
         break;
      }
   }
   
   if (!found)
   	return 0;
   

   return (*data->callback) (list, text->value,
             data->data ? (void *)data->data[data->selected_row ] : NULL);
}

int ssd_list_short_click (SsdWidget widget, const RoadMapGuiPoint *point) {


   if (widget->children != NULL)

   if (!ssd_widget_short_click (widget->children, point))
     label_callback(widget,"");
      	
   return 1;  
}
static int delete_callback (SsdWidget widget, const char *new_value) {
   ssd_list_data_ptr data;
   int            relative_index;
   int            absolute_index;
   SsdWidget list = widget->parent->parent;
   SsdWidget text = ssd_widget_get (widget, "label");
   SsdWidget      item = ssd_list_item_has_focus( list);   
   data = (ssd_list_data_ptr) list->data;

   if (!data->del_callback) return 0;

   if( !item)
      return 0;
   
   relative_index   = (int)(long)item->context;
   absolute_index   = data->first_row_index + relative_index;

   return (*data->del_callback) (list, text->value, data->data ? (void *)data->data[absolute_index ] : NULL);
}

static BOOL move_focus( SsdWidget this, BOOL up /* or down */)
{
   int               relative_index = 0;
   int               absolute_index = 0;
   ssd_list_data_ptr list_data      = NULL;
   
   //   Valid input?   
   if( !this || !this->data)
      return FALSE;
   
   list_data = (ssd_list_data_ptr)this->data;

   if( !list_data->num_values)
      return FALSE;

   relative_index = (int)(long)this->context;
   absolute_index = list_data->first_row_index + relative_index;

#ifdef TOUCH_SCREEN

   if ((ssd_dialog_get_focus() == NULL) && (list_data->num_values > 0)){
   	
   	  ssd_dialog_set_focus(list_data->rows[0]);
      return TRUE;
   }
#endif

   if( up)
   {
      if( !absolute_index)
      {
         //   We are in item-0 and need to jump back to list last item?
         
         if( list_data->widget_before_list)
            ssd_dialog_set_focus( list_data->widget_before_list);
         else
         {
            ssd_list_scroll_list_end( this, list_data);
            ssd_dialog_set_focus( list_data->rows[((list_data->num_values - 1)% (list_data->num_rows ))]);
         }
      }
      else if( 0 == (absolute_index % list_data->num_rows))
      {
         //   Need to scroll one page up
         
         ssd_list_scroll_one_page_up( this, list_data);
         ssd_dialog_set_focus( list_data->rows[list_data->num_rows-1]);
      }
      else      
         ssd_dialog_set_focus (list_data->rows[(long)this->context - 1]);
   }
   else
   {
      // Down
      
      if( ((relative_index+1) == list_data->num_rows) && ((absolute_index+1) < list_data->num_values))
      {
         //   Need to scroll one-page down
         
         ssd_list_scroll_one_page_down( this, list_data);
         ssd_dialog_set_focus( list_data->rows[0]);
      }
      else if( (absolute_index+1) == list_data->num_values)
      {
         //   End of list_data - need to loop-back to list_data begin
         
         ssd_list_scroll_list_begin( this, list_data);

         if( list_data->widget_after_list)
            ssd_dialog_set_focus( list_data->widget_after_list);
         else
            ssd_dialog_set_focus( list_data->rows[0]);
      }
      else      
         ssd_dialog_set_focus(list_data->rows[(long)this->context + 1]);
   }
   
   return TRUE;
}

static BOOL ListItem_OnKeyPressed( SsdWidget this, const char* utf8char, uint32_t flags)
{
   int               relative_index = 0;
   ssd_list_data_ptr list           = (ssd_list_data_ptr)this->data;

   //   Valid input?   
   if( !this || !this->data)
      return FALSE;

   relative_index = (int)(long)this->context;

   //   Our task?
   if( !(KEYBOARD_VIRTUAL_KEY & flags))
   {
      //   Is this the 'Activate' ('enter' / 'select')
      if( KEY_IS_ENTER)
      {
         this->callback( this, this->name);
         return TRUE;
      }

      //  Del key
      if( KEY_IS_BACKSPACE)
      {
         if( delete_callback( list->rows[relative_index], list->rows[relative_index]->name))
            return TRUE;
      }

      if( list->on_unhandled_key_press)
         return list->on_unhandled_key_press( this, utf8char, flags);

      return FALSE;
   }



   //   Have data?
   if( !list->num_rows || !list->num_values)
      return FALSE;
   
   //   Go up:
   if( VK_Arrow_up == (*utf8char))
      return move_focus( this, TRUE /* Up? */);
   
   //   Go down:
   if( VK_Arrow_down == (*utf8char))
      return move_focus( this, FALSE /* Up? */);
   
   
    //  Del key
   if( KEY_IS_BACKSPACE)
   {
       if( delete_callback( list->rows[relative_index], list->rows[relative_index]->name))
         return TRUE;
   }

   if( list->on_unhandled_key_press)
      return list->on_unhandled_key_press( this, utf8char, flags);
   
   return FALSE;
}

BOOL ssd_list_move_focus( SsdWidget list, BOOL up)
{
   if( !ssd_list_item_has_focus( list))
      return FALSE;
   
   return move_focus( list, up);
}

static int ssd_list_long_click (SsdWidget widget, const RoadMapGuiPoint *point) {

   return 1;
}

BOOL ssd_list_set_focus( SsdWidget list, BOOL first_item /* or last */)
{
   ssd_list_data_ptr list_data = (ssd_list_data_ptr)list->data;
   int               new_focus;
   
   if( !list_data->num_values)
      return FALSE;

   if( first_item)
   {
      ssd_list_scroll_list_begin( list, list_data);
      new_focus = 0;
   }
   else
   {
      // Last item
      ssd_list_scroll_list_end( list, list_data);
      new_focus = ((list_data->num_values - 1) % (list_data->num_rows));
   }
   
   return ssd_dialog_set_focus( list_data->rows[ new_focus]);
}

static int next_button_callback (SsdWidget widget, const char *new_value){
	
	ssd_widget_set_focus(widget->parent->parent);
	
	roadmap_softkeys_left_softkey_callback();
	return 0;
}

static void update_list_rows (SsdWidget list_container, SsdSize *size,
                              ssd_list_data *data) {
   
   int num_rows;
   int row_height;
   int i;
   
   //ssd_widget_container_size (list_container->parent, &size);
	
   num_rows = size->height / data->min_row_height;
   row_height = size->height / num_rows;

  // if (data->num_rows == num_rows) return;

   if (num_rows > data->alloc_rows) {
      data->rows = realloc (data->rows, sizeof(SsdWidget) * num_rows);

      for (i=data->alloc_rows; i<num_rows; i++) {
         SsdWidget row;
         SsdWidget label;         
      	int pointer_type = 0;
      	
        SsdWidget image_con; 
#ifdef IPHONE
        SsdWidget image_con2;
#endif
        
        if (data->flags && (data->flags[i] & SSD_POINTER_COMMENT))
        	pointer_type = SSD_POINTER_COMMENT;
        	
        row = ssd_container_new ("rowx", NULL, SSD_MAX_SIZE,
               row_height,
               SSD_WS_TABSTOP|SSD_CONTAINER_BORDER|SSD_END_ROW|SSD_ROUNDED_CORNERS|pointer_type);

         label = ssd_text_new ("label", "", 14, SSD_END_ROW|SSD_ALIGN_VCENTER);
           //ssd_widget_set_color (label, "#FFFFFF", "#0274bd");
         
         //ssd_widget_set_color (row, "#FFFFFF", "#0274bd");

         ssd_widget_set_callback (row, label_callback);
        
        image_con = ssd_container_new ("icon_container", NULL, 50,
               row_height-6,  SSD_DIALOG_TRANSPARENT|SSD_ALIGN_VCENTER);
               
         ssd_widget_set_color (image_con, NULL, NULL);


#ifdef IPHONE		 
         image_con2 = ssd_container_new ("next_icon_container", NULL,40 ,
         								row_height,  SSD_TAB_CONTROL|SSD_DIALOG_TRANSPARENT|SSD_ALIGN_VCENTER|SSD_ALIGN_RIGHT);
         
         ssd_widget_add(row, image_con2);
         	
         ssd_widget_set_color (image_con2, NULL, NULL);
#endif		 
         

         ssd_widget_add (row,  image_con);
         
         ssd_widget_add (row, label);
    	 row->short_click = ssd_list_short_click;     
        
         ssd_widget_add (list_container, row);
         row->key_pressed     = ListItem_OnKeyPressed;
         row->data            = data;
         row->context         = (void*)(long)i;
         row->get_input_type  = get_input_type;

         data->rows[i] = row;
      }

      data->alloc_rows = num_rows;
   }

   for (i=0; i<num_rows; i++) {
      ssd_widget_set_size (data->rows[i], SSD_MAX_SIZE, row_height);
      ssd_widget_show (data->rows[i]);
   }

   for (i=num_rows; i<data->num_rows; i++) {
      ssd_widget_hide (data->rows[i]);
   }

   data->num_rows = num_rows;
}

static void ssd_list_draw (SsdWidget widget, RoadMapGuiRect *rect, int flags) {
   ssd_list_data_ptr data = (ssd_list_data_ptr) widget->data;

   if (!(flags & SSD_GET_CONTAINER_SIZE)) {
      int height = rect->maxy - rect->miny + 1;
      int width = rect->maxx - rect->minx + 1;

      if ((data->list_size.height != height) || (data->list_size.width != width)) {
         SsdWidget list_container;
      
         data->list_size.height = height;
         data->list_size.width = width;
         list_container = ssd_widget_get (widget, "list_container");
         update_list_rows (list_container, &data->list_size, data);
         setup_list_rows(data);
         ssd_dialog_resort_tab_order();
      }
   }

   (*data->list_container_draw)(widget, rect, flags);
}

SsdWidget ssd_list_item_has_focus( SsdWidget list)
{
   ssd_list_data_ptr data = (ssd_list_data_ptr) list->data;
   int i;
   
   if( !data->num_values)
      return NULL;

   for( i=0; i<data->num_rows; i++)
      if( data->rows[i]->in_focus)
         return data->rows[i];
   return NULL;
}

const char* ssd_list_selected_string( SsdWidget list)
{
   ssd_list_data_ptr data = (ssd_list_data_ptr)list->data;
   SsdWidget         item = ssd_list_item_has_focus( list);
   int               relative_index;
   int               absolute_index;
   
   if( !item)
      return NULL;
   
   relative_index   = (int)(long)item->context;
   absolute_index   = data->first_row_index + relative_index;   
   
   assert( (0 <= absolute_index) && (absolute_index < data->num_values));

   return data->labels[absolute_index];
}

const void* ssd_list_selected_value( SsdWidget list)
{
   ssd_list_data_ptr data = (ssd_list_data_ptr)list->data;
   SsdWidget         item = ssd_list_item_has_focus( list);
   int               relative_index;
   int               absolute_index;
   
   if( !item)
      return NULL;
   
   relative_index   = (int)(long)item->context;
   absolute_index   = data->first_row_index + relative_index;   
   
   assert( (0 <= absolute_index) && (absolute_index < data->num_values));

   return data->data[absolute_index];
} 

void ssd_list_resize (SsdWidget list, int min_height)
{
   ssd_list_data_ptr data = (ssd_list_data_ptr) list->data;

   if (min_height > 0) data->min_row_height = min_height;
   else data->min_row_height = MIN_ROW_HEIGHT;

   data->list_size.width = -1;
   data->list_size.height = -1;   
}


static const void *get_data (SsdWidget widget) {
   ssd_list_data_ptr data = (ssd_list_data_ptr)widget->data;

   if (!data || (data->selected_row == -1)) return NULL;

   return data->data[data->selected_row];
}


SsdWidget ssd_list_new( const char*             name, 
                        int                     width, 
                        int                     height, 
                        roadmap_input_type      input_type,
                        int                     flags,
                        PFN_WIDGET_ONKEYPRESSED on_unhandled_key_press) {

   const char *scroll_up_icons[]   = {"up"};
   const char *scroll_down_icons[] = {"down"};
   SsdSize   button_size;
   SsdWidget list;
   SsdWidget scroll;
   SsdWidget scroll_up;
   SsdWidget scroll_down;
   SsdWidget scroll_bar;
   ssd_list_data_ptr data =
      (ssd_list_data_ptr)calloc (1, sizeof(*data));
      
   SsdWidget list_container = ssd_container_new (name, NULL, width, height, 0);
   /* Override list container draw */
   data->list_container_draw = list_container->draw;
   list_container->draw = ssd_list_draw;

   data->on_unhandled_key_press  = on_unhandled_key_press;
   data->input_type              = input_type;

   list_container->data = data;

#ifndef IPHONE
   scroll_up   = ssd_button_new ("scroll_up", "", scroll_up_icons, 1,
                                 SSD_ALIGN_CENTER|SSD_END_ROW, button_callback);
   scroll_down = ssd_button_new ("scroll_down", "", scroll_down_icons, 1,
                               SSD_ALIGN_CENTER|SSD_END_ROW|SSD_ORDER_LAST, button_callback);

   ssd_widget_get_size (scroll_up, &button_size, NULL);

   scroll = ssd_container_new ("scroll_container", NULL,
                               SSD_MIN_SIZE,
                               SSD_MAX_SIZE,
                               SSD_ALIGN_RIGHT);

   ssd_widget_set_color (scroll, "#000000", "#efefef");

   ssd_widget_add (scroll, scroll_up);

   scroll_bar = ssd_container_new ("scroll_bar", NULL,
                                   button_size.width,
                                   SSD_MAX_SIZE,
                                   SSD_CONTAINER_BORDER|SSD_END_ROW);

   ssd_widget_add (scroll, scroll_bar);
   ssd_widget_add (scroll, scroll_down);
#endif
   list = ssd_container_new ("list_container", NULL,SSD_MAX_SIZE,SSD_MAX_SIZE, 0);

   ssd_widget_set_color (list, "#FFFFFF", "#FFFFFF");

#ifndef IPHONE
   ssd_widget_add (list_container, scroll);
#endif
   ssd_widget_add (list_container, list);

   list->long_click = ssd_list_long_click;
   list_container->get_data = get_data;
   
   list_container->drag_start = ssd_list_drag_start;
   list_container->drag_end = ssd_list_drag_end;
   list_container->drag_motion = ssd_list_drag_motion;

   return list_container;
}


void ssd_list_populate (SsdWidget list, int count, const char **labels,
                        const void **values, const char **icons, const int *flags, SsdIconListCallback callback, SsdIconListDeleteCallback del_callback, BOOL add_next_button) {

   ssd_list_data_ptr data = (ssd_list_data_ptr)list->data;

   data->num_values = count;
   data->labels = labels;
   data->data = values;
   data->icons = icons;
   data->flags = flags;
   data->first_row_index = 0;
   data->callback = callback;
   data->del_callback = del_callback;
   data->add_next_button = add_next_button;
   
   if (count > 0)
   	setup_list_rows (data);
}

SsdWidget ssd_list_get_first_item( SsdWidget list)
{
   ssd_list_data_ptr data = (ssd_list_data_ptr)list->data;

   assert(data);

   if( !data->num_values || !data->alloc_rows)
      return NULL;
   
   return data->rows[0];
}

void ssd_list_taborder__set_widget_before_list( SsdWidget list, SsdWidget w)
{
   ssd_list_data_ptr data = (ssd_list_data_ptr)list->data;
   assert(data);
   data->widget_before_list = w;
}

void ssd_list_taborder__set_widget_after_list( SsdWidget list, SsdWidget w)
{
   ssd_list_data_ptr data = (ssd_list_data_ptr)list->data;
   assert(data);
   data->widget_after_list = w;
}

static int ssd_list_drag_start (SsdWidget widget, const RoadMapGuiPoint *point) {
	drag_start_point.x = point->x;
	drag_start_point.y = point->y;
	return 1;
}

static int ssd_list_drag_end (SsdWidget widget, const RoadMapGuiPoint *point) {
   ssd_list_data_ptr data;
   SsdWidget list = widget;
   SsdWidget focus_item;
   
   list = ssd_widget_get(widget,"list_container");
   /* Find the list main widget */
   
   list = list->parent;

   data = (ssd_list_data_ptr)list->data;

    if (point->y > drag_start_point.y)
		ssd_list_scroll_one_page_up( widget, data);
	else
		ssd_list_scroll_one_page_down( widget, data);
	
	focus_item = ssd_list_item_has_focus( list);
	if (focus_item)
		ssd_widget_loose_focus(focus_item);
		
    roadmap_screen_redraw ();
	return 1;
}

static int ssd_list_drag_motion (SsdWidget widget, const RoadMapGuiPoint *point) {
	return 1;
}
