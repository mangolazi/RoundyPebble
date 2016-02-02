//======================================
// ROUNDY watch face
// Copyright Mango Lazi 2015
// Released under GPLv3
// Lots of stuff from Pebble SDK, config stuff thanks to Tom Gidden
//======================================

#include <ctype.h>
#include <stdio.h>
#include "roundy.h" // hour ticks and hand designs in here
#include "pebble.h"

static Window *window;
static GFont custom_font;
static GColor color_background, color_circle, color_circle_fill, color_ticks, color_maintext, color_cornertext, color_maintextbackground, color_12background, color_cornertextbackground, color_second, color_hand_fill, color_hand_stroke, color_center, color_center2;
static Layer *s_simple_bg_layer, *s_date_layer, *s_text_layer, *s_hands_layer;
static TextLayer *s_label_12, *s_label_9; // number labels
static TextLayer *s_day_label, *s_battery_label, *s_bluetooth_label, *s_weather_label, *s_misc_label, *s_misc2_label, *s_misc3_label; // information labels
static char s_day_buffer[18], s_battery_buffer[4], s_bluetooth_buffer[6]; // s_weather_buffer[10], s_misc_buffer[10], s_misc2_buffer[10], s_misc3_buffer[30]; // buffers for information labels
static bool weather_fetched = false; // new weather data fetched for 60 minute period

// bitmaps and layers for weather icon
static BitmapLayer *s_icon_layer;
static GBitmap *s_icon_bitmap = NULL;
static uint8_t s_icon_current = 3; // default loading icon

// background hour ticks and arrows
//static GPath *s_tick_paths[NUM_CLOCK_TICKS];
static GPath *s_minute_arrow, *s_hour_arrow;

// appsync stuff
static AppSync s_sync;
static uint8_t s_sync_buffer[128];

// struct for cached weather data
typedef struct weatherdata {
    uint8_t icon_current;   // weather icon
    char weather[10];       // temperature
    char misc[10];         // sunrise
    char misc2[10];         // sunset
    char misc3[30];         // city
} __attribute__((__packed__)) weatherdata;

weatherdata cachedWeather = {
    .icon_current = 3, // loading icon
    .weather = "",
    .misc = "",
    .misc2 = "",
    .misc3 = ""
};

// preferences, stored in watch persistent storage
typedef struct persist {
    uint8_t seconds; // show seconds, default false
    uint8_t hourvibes; // vibrate at the start of every hour, default false
    uint8_t reverse; // reverse display: white background with black text, default false
} __attribute__((__packed__)) persist;

persist settings = {
    .seconds = 0, // show seconds, default false
    .hourvibes = 0, // vibrate at the start of every hour, default false
    .reverse = 0 // reverse display: white background with black text, default false
};

enum PersistKey {
    PERSIST_SETTINGS = 0,
    PERSIST_WEATHERDATA = 1
};

// appkeys, should match stuff in appinfo.json
enum WeatherKey {
	WEATHER_ICON = 0x0,			// TUPLE_CSTRING
	WEATHER_WEATHERDATA = 0x1,	// TUPLE_CSTRING
	WEATHER_MISC = 0x2,			// TUPLE_CSTRING
	WEATHER_MISC2 = 0x3,		// TUPLE_CSTRING
    WEATHER_MISC3 = 0x4,		// TUPLE_CSTRING
	CONFIG_SECONDS = 0x5,		// TUPLE_INT
	CONFIG_HOURVIBES = 0x6,		// TUPLE_INT
	CONFIG_REVERSE = 0x7		// TUPLE_INT
};

// array for weather and forecast icons, default look
static const uint32_t WEATHER_ICONS[] = {
    RESOURCE_ID_IMAGE_CLEAR_DAY,    //0
    RESOURCE_ID_IMAGE_CLEAR_NIGHT,  //1
    RESOURCE_ID_IMAGE_CLOUD,        //2
    RESOURCE_ID_IMAGE_LOADING,      //3
    RESOURCE_ID_IMAGE_MIST_DAY,     //4
    RESOURCE_ID_IMAGE_MIST_NIGHT,   //5
    RESOURCE_ID_IMAGE_RAIN,         //6
    RESOURCE_ID_IMAGE_SNOW,         //7
};

// array for weather and forecast icons, reverse look
static const uint32_t WEATHER_ICONS_REVERSE[] = {
    RESOURCE_ID_IMAGE_CLEAR_DAY_REV,    //0
    RESOURCE_ID_IMAGE_CLEAR_NIGHT_REV,  //1
    RESOURCE_ID_IMAGE_CLOUD_REV,        //2
    RESOURCE_ID_IMAGE_LOADING_REV,      //3
    RESOURCE_ID_IMAGE_MIST_DAY_REV,     //4
    RESOURCE_ID_IMAGE_MIST_NIGHT_REV,   //5
    RESOURCE_ID_IMAGE_RAIN_REV,         //6
    RESOURCE_ID_IMAGE_SNOW_REV,         //7
};


//======================================
// REQUEST WEATHER USING PHONE
//======================================
// send out an empty dictionary for syncing
// someday I'll figure out how to do bidirectional syncing
static void request_weather(void) {
	DictionaryIterator *iter;
    weather_fetched = true;

	app_message_outbox_begin(&iter);

	if (!iter) {
		// Error creating outbound message
	    return;
		}

	int value = 3;
	dict_write_int(iter, 1, &value, sizeof(int), true);
	dict_write_end(iter);
	app_message_outbox_send();
}


//======================================
// COLOR HANDLER
//======================================
// white on black default, black on white reverse
static void color_handler() {
    if (settings.reverse == 1) { // reverse layout
        color_background = COLOR_FALLBACK(GColorBlack, GColorBlack);
        color_circle = COLOR_FALLBACK(GColorDarkGray, GColorBlack);
        color_circle_fill = COLOR_FALLBACK(GColorRed, GColorWhite);
        color_center = COLOR_FALLBACK(GColorBlack, GColorBlack);
        color_center2 = COLOR_FALLBACK(GColorWhite, GColorWhite);
        color_ticks = COLOR_FALLBACK(GColorWhite, GColorWhite);
        color_second = COLOR_FALLBACK(GColorCyan, GColorBlack);
        color_hand_fill = COLOR_FALLBACK(GColorWhite, GColorBlack);;
        color_hand_stroke = COLOR_FALLBACK(GColorBlack, GColorBlack);
        color_maintext = COLOR_FALLBACK(GColorWhite, GColorBlack); // for numerals, date, weather
        color_cornertext = COLOR_FALLBACK(GColorWhite, GColorWhite); // for battery, BT, misc, misc2
        color_maintextbackground = COLOR_FALLBACK(GColorClear, GColorClear); // background for main text
        color_12background = COLOR_FALLBACK(GColorClear, GColorClear); // background for 12 numeral
        color_cornertextbackground = COLOR_FALLBACK(GColorClear, GColorClear); // background for corner text
    }
    else { // default
        color_background = GColorBlack;
        color_circle = COLOR_FALLBACK(GColorFolly, GColorWhite);
        color_circle_fill = COLOR_FALLBACK(GColorBlack, GColorBlack);
        color_center = COLOR_FALLBACK(GColorWhite, GColorWhite);
        color_center2 = COLOR_FALLBACK(GColorRed, GColorBlack);
        color_ticks = COLOR_FALLBACK(GColorWhite, GColorWhite);
        color_second = COLOR_FALLBACK(GColorCyan, GColorWhite);
        color_hand_fill = COLOR_FALLBACK(GColorWhite, GColorWhite);;
        color_hand_stroke = COLOR_FALLBACK(GColorBlack, GColorBlack);
        color_maintext = COLOR_FALLBACK(GColorChromeYellow, GColorWhite); // for numerals, date, weather
        color_cornertext = COLOR_FALLBACK(GColorLightGray, GColorWhite); // for battery, BT, misc, misc2
        color_maintextbackground = COLOR_FALLBACK(GColorClear, GColorClear); // background for main text
        color_12background = COLOR_FALLBACK(GColorClear, GColorClear); // background for 12 numeral
        color_cornertextbackground = COLOR_FALLBACK(GColorClear, GColorClear); // background for corner text
    }
}


//======================================
// BACKGROUND UPDATER
//======================================
// white on black default, black on white reverse
static void bg_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
	graphics_context_set_fill_color(ctx, color_background);
  	graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    
	// circle frames
    graphics_context_set_fill_color(ctx, color_circle_fill);
    graphics_context_set_stroke_color(ctx, color_circle);
    #ifdef PBL_PLATFORM_BASALT
        graphics_context_set_stroke_width(ctx, 2);
    #endif
    graphics_fill_circle(ctx, GPoint(bounds.size.w/2, bounds.size.h/2), 67);
    graphics_draw_circle(ctx, GPoint(bounds.size.w/2, bounds.size.h/2), 70);
//    graphics_draw_circle(ctx, GPoint(bounds.size.w/2, bounds.size.h/2), 67);
    #ifdef PBL_PLATFORM_BASALT
        graphics_context_set_stroke_width(ctx, 1);
    #endif
    
	// hour ticks
    int ticklarge_size = 6;
    graphics_context_set_fill_color(ctx, color_circle);
    graphics_context_set_stroke_color(ctx, color_circle);
	graphics_fill_circle(ctx, GPoint(107, 24), ticklarge_size); // 1
	graphics_fill_circle(ctx, GPoint(132, 50), ticklarge_size); // 2
	graphics_fill_circle(ctx, GPoint(140, 85), ticklarge_size); // 3
	graphics_fill_circle(ctx, GPoint(132, 119), ticklarge_size); // 4
	graphics_fill_circle(ctx, GPoint(107, 144), ticklarge_size); // 5
	graphics_fill_circle(ctx, GPoint(72, 154), ticklarge_size); // 6
	graphics_fill_circle(ctx, GPoint(37, 144), ticklarge_size); // 7
	graphics_fill_circle(ctx, GPoint(12, 119), ticklarge_size); // 8
	graphics_fill_circle(ctx, GPoint(3, 85), ticklarge_size); // 9
	graphics_fill_circle(ctx, GPoint(12, 50), ticklarge_size); // 10
	graphics_fill_circle(ctx, GPoint(38, 24), ticklarge_size); // 11
    graphics_fill_circle(ctx, GPoint(72, 14), ticklarge_size); // 12
    
    // hour ticks interior
    int ticksmall_size = 4;
    graphics_context_set_fill_color(ctx, color_ticks);
    graphics_context_set_stroke_color(ctx, color_ticks);
    graphics_fill_circle(ctx, GPoint(107, 24), ticksmall_size); // 1
    graphics_fill_circle(ctx, GPoint(132, 50), ticksmall_size); // 2
    graphics_fill_circle(ctx, GPoint(140, 85), ticksmall_size); // 3
    graphics_fill_circle(ctx, GPoint(132, 119), ticksmall_size); // 4
    graphics_fill_circle(ctx, GPoint(107, 144), ticksmall_size); // 5
    graphics_fill_circle(ctx, GPoint(72, 154), ticksmall_size); // 6
    graphics_fill_circle(ctx, GPoint(37, 144), ticksmall_size); // 7
    graphics_fill_circle(ctx, GPoint(12, 119), ticksmall_size); // 8
    graphics_fill_circle(ctx, GPoint(3, 85), ticksmall_size); // 9
    graphics_fill_circle(ctx, GPoint(12, 50), ticksmall_size); // 10
    graphics_fill_circle(ctx, GPoint(38, 24), ticksmall_size); // 11
    graphics_fill_circle(ctx, GPoint(72, 14), ticksmall_size); // 12

	
}


//======================================
// HANDS UPDATER
//======================================
// draw second hand if config_seconds set to 1
// otherwise minute and hour hands only
static void hands_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	
	// minute hand
	graphics_context_set_fill_color(ctx, color_hand_fill);
	graphics_context_set_stroke_color(ctx, color_hand_stroke);
	gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
	gpath_draw_filled(ctx, s_minute_arrow);
	gpath_draw_outline(ctx, s_minute_arrow);

	// hour hand
	graphics_context_set_fill_color(ctx, color_hand_fill);
	graphics_context_set_stroke_color(ctx, color_hand_stroke);
	gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6)); // from Pebble SDK example
	gpath_draw_filled(ctx, s_hour_arrow);
	gpath_draw_outline(ctx, s_hour_arrow);
    
    // circles in the middle
    graphics_context_set_fill_color(ctx, color_center);
    graphics_context_set_stroke_color(ctx, color_center);
    graphics_fill_circle(ctx, GPoint(bounds.size.w/2, bounds.size.h/2), 6);
    graphics_context_set_fill_color(ctx, color_center2);
    graphics_context_set_stroke_color(ctx, color_center2);
    graphics_fill_circle(ctx, GPoint(bounds.size.w/2, bounds.size.h/2), 3);

    // draw second hand if config is set
    if (settings.seconds == 1) {
        GPoint center = grect_center_point(&bounds);
        int16_t second_hand_length = (bounds.size.w / 2) - 1;
        int32_t second_angle = TRIG_MAX_ANGLE * t->tm_sec / 60;
        GPoint second_hand = {
            .x = (int16_t)(sin_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.x,
            .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.y,
        };
        #ifdef PBL_PLATFORM_BASALT
            graphics_context_set_stroke_width(ctx, 2);
        #endif
        graphics_context_set_stroke_color(ctx, color_second);
        graphics_context_set_fill_color(ctx, color_second);
        graphics_fill_circle(ctx, GPoint(bounds.size.w/2, bounds.size.h/2), 2);
        graphics_draw_line(ctx, second_hand, center);
    }
   
    // update weather with phone every 01st and 31st minutes, but phone fetches fresh online data only every hour
    // reset update status at 59 minutes
//	if((t->tm_min == 1) && (t->tm_sec == 0)) {
//	if(((t->tm_min == 1) || (t->tm_min == 31)) && (weather_fetched == false)) {
    if((t->tm_min == 1) && (weather_fetched == false)) {
        request_weather();
	}
    if(t->tm_min == 59) {
        weather_fetched = false;
    }
	
	// vibrate at start of every hour
    if(settings.hourvibes == 1) {
		if((t->tm_min == 0) && (t->tm_sec == 0)) {
		vibes_long_pulse();
		}
	}
    
}


//======================================
// DATE UPDATER
//======================================
// should do localization here later
static void date_update_proc(Layer *layer, GContext *ctx) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(s_day_buffer, sizeof(s_day_buffer), "%a\n%d", t);

    // convert to all uppercase, looks nicer
//    unsigned int i = 0;
//    unsigned char c;
//    while (i < sizeof(s_day_buffer))
//    {
////        s_day_buffer[i] = toupper(s_day_buffer[i]);
//        c = s_day_buffer[i];
//        putchar (toupper(c));
//        s_day_buffer[i] = c;
//        i++;
//    }
//    for (i = 0; sizeof(s_day_buffer); i++) {
//        s_day_buffer[1] = "C";
//        s_day_buffer[i] = toupper(s_day_buffer[i]);
//    }
}


//======================================
// TIME TICK HANDLER
//======================================
// ticks per second or minute, see init below, also allows changes through appsync
static void handle_time_tick(struct tm *tick_time, TimeUnits units_changed) {
    layer_mark_dirty(window_get_root_layer(window));
}


//======================================
// APPSYNC STUFF
//======================================

//======================================
// SYNC ERROR CALLBACK
//======================================
static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

//======================================
// TUPLE CHANGED CALLBACK
//======================================
// Called every time watch or phone sends appsync dictionary

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* t, const Tuple* old_tuple, void* context) {
  switch (key) {	  
    case WEATHER_ICON:
      cachedWeather.icon_current = t->value->uint8;
      if (s_icon_bitmap) {
          gbitmap_destroy(s_icon_bitmap);
      }
      if (settings.reverse == 1) {
          s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_REVERSE[cachedWeather.icon_current]);
      }
      else {
          s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[cachedWeather.icon_current]);
      }
      bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);
    break;

  	case WEATHER_WEATHERDATA:
      snprintf(cachedWeather.weather, sizeof(cachedWeather.weather), "%s",  t->value->cstring);
      text_layer_set_text(s_weather_label, cachedWeather.weather);
	break;

  	case WEATHER_MISC:
      snprintf(cachedWeather.misc, sizeof(cachedWeather.misc), "%s",  t->value->cstring);
      text_layer_set_text(s_misc_label, cachedWeather.misc);
	break;
	
  	case WEATHER_MISC2:
      snprintf(cachedWeather.misc2, sizeof(cachedWeather.misc2), "%s",  t->value->cstring);
      text_layer_set_text(s_misc2_label, cachedWeather.misc2);
	break;

    case WEATHER_MISC3:
      snprintf(cachedWeather.misc3, sizeof(cachedWeather.misc3), "%s",  t->value->cstring);
      text_layer_set_text(s_misc3_label, cachedWeather.misc3);
    break;

	case CONFIG_SECONDS:
      settings.seconds = t->value->uint8;
      if (t->value->uint8 == 1) {
          tick_timer_service_subscribe(SECOND_UNIT, handle_time_tick);
      }
      else {
          tick_timer_service_subscribe(MINUTE_UNIT, handle_time_tick);
      }
	break;

	case CONFIG_HOURVIBES:
      settings.hourvibes = t->value->uint8;
	break;
      
    case CONFIG_REVERSE:
      settings.reverse = t->value->uint8;
      color_handler();
      
      // numerals for 12, 9 o'clock
      text_layer_set_text_color(s_label_12, color_maintext);
      text_layer_set_background_color(s_label_12, color_12background);
      text_layer_set_text_color(s_label_9, color_maintext);
      text_layer_set_background_color(s_label_9, color_maintextbackground);
      
      // date label
      text_layer_set_text_color(s_day_label, color_maintext);
      text_layer_set_background_color(s_day_label, color_maintextbackground);
      
      // battery label
      text_layer_set_text_color(s_battery_label, color_cornertext);
      text_layer_set_background_color(s_battery_label, color_cornertextbackground);
      
      // bluetooth label
      text_layer_set_text_color(s_bluetooth_label, color_cornertext);
      text_layer_set_background_color(s_bluetooth_label, color_cornertextbackground);
      
      // weather label
      text_layer_set_text_color(s_weather_label, color_maintext);
      text_layer_set_background_color(s_weather_label, color_cornertextbackground);
      
      if (s_icon_bitmap) {
          gbitmap_destroy(s_icon_bitmap);
      }
      if (settings.reverse == 1) {
          s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_REVERSE[cachedWeather.icon_current]);
      }
      else {
          s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[cachedWeather.icon_current]);
      }
      bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);
      
      // add misc label
      text_layer_set_text_color(s_misc_label, color_cornertext);
      text_layer_set_background_color(s_misc_label, color_cornertextbackground);
      
      // add misc2 label
      text_layer_set_text_color(s_misc2_label, color_cornertext);
      text_layer_set_background_color(s_misc2_label, color_cornertextbackground);
      
      // add misc3 label
      text_layer_set_text_color(s_misc3_label, color_maintext);
      text_layer_set_background_color(s_misc3_label, color_maintextbackground);

    break;

  }
}


//======================================
// BATTERY LEVEL HANDLER
//======================================
static void handle_battery(BatteryChargeState charge_state) {
  if (charge_state.is_charging) {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "+%d",  charge_state.charge_percent);
  } else {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%%", charge_state.charge_percent);
  }
  text_layer_set_text(s_battery_label, s_battery_buffer);
}


//======================================
// BLUETOOTH CONNECTION HANDLER
//======================================
static void handle_bluetooth(bool connected_state) {
	if (connected_state == true) {
  		snprintf(s_bluetooth_buffer, sizeof(s_bluetooth_buffer), "BT");
  		request_weather();
	} 
	else {
  		snprintf(s_bluetooth_buffer, sizeof(s_bluetooth_buffer), "--");
		vibes_double_pulse(); // double pulse vibration if connection lost
  	}
	text_layer_set_text(s_bluetooth_label, s_bluetooth_buffer);
}


//======================================
// MAIN WINDOW LOADER
//======================================
static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
    
    // create custom GFont
    custom_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROUNDY_18));
    
	// black background
	s_simple_bg_layer = layer_create(bounds);
	layer_set_update_proc(s_simple_bg_layer, bg_update_proc);
	layer_add_child(window_layer, s_simple_bg_layer);
    
    // numerals for 12, 9 o'clock
    s_label_12 = text_layer_create(GRect(bounds.size.w /2 - 17, 13, 34, 36));
    text_layer_set_text(s_label_12, "12");
    text_layer_set_text_color(s_label_12, color_maintext);
    text_layer_set_background_color(s_label_12, color_12background);
    #ifdef PBL_PLATFORM_BASALT
//        text_layer_set_font(s_label_12, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS));
    text_layer_set_font(s_label_12, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
    #else
        text_layer_set_font(s_label_12, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
    #endif
    text_layer_set_text_alignment(s_label_12, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_label_12));
    
    s_label_9 = text_layer_create(GRect(10, bounds.size.h /2 - 22, 35, 36));
    text_layer_set_text(s_label_9, "9");
    text_layer_set_text_color(s_label_9, color_maintext);
    text_layer_set_background_color(s_label_9, color_maintextbackground);
    #ifdef PBL_PLATFORM_BASALT
            text_layer_set_font(s_label_9, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
//        text_layer_set_font(s_label_9, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS));
    #else
        text_layer_set_font(s_label_9, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
    #endif
    text_layer_set_text_alignment(s_label_9, GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(s_label_9));
    
    // date label
    s_date_layer = layer_create(bounds);
    layer_set_update_proc(s_date_layer, date_update_proc);
    layer_add_child(window_layer, s_date_layer);
    
    s_day_label = text_layer_create(GRect(83, bounds.size.h /2 - 20, 50, 60));
    text_layer_set_text(s_day_label, s_day_buffer);
    text_layer_set_text_color(s_day_label, color_maintext);
    text_layer_set_background_color(s_day_label, color_maintextbackground);
    //text_layer_set_font(s_day_label, custom_font);
    #ifdef PBL_PLATFORM_BASALT
        text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    #else
        text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    #endif
    text_layer_set_text_alignment(s_day_label, GTextAlignmentRight);
    layer_add_child(s_date_layer, text_layer_get_layer(s_day_label));
    
    // add current weather icon
    s_icon_layer = bitmap_layer_create(GRect(bounds.size.w / 2 - 15, 105, 30, 30));
     bitmap_layer_set_compositing_mode(s_icon_layer, GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_icon_layer));
    if (s_icon_bitmap) {
        gbitmap_destroy(s_icon_bitmap);
    }
    if (settings.reverse == 1) {
        s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_REVERSE[cachedWeather.icon_current]);
    }
    else {
        s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[cachedWeather.icon_current]);
    }
    bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);
    
    // weather label
    s_weather_label = text_layer_create(GRect(bounds.size.w / 2 - 16, 125, 40, 20));
    text_layer_set_text_color(s_weather_label, color_maintext);
    text_layer_set_background_color(s_weather_label, color_maintextbackground);
    //text_layer_set_font(s_weather_label, custom_font);
    #ifdef PBL_PLATFORM_BASALT
        text_layer_set_font(s_weather_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    #else
        text_layer_set_font(s_weather_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    #endif
    text_layer_set_text_alignment(s_weather_label, GTextAlignmentCenter);
    text_layer_set_text(s_weather_label, cachedWeather.weather);
    layer_add_child(window_layer, text_layer_get_layer(s_weather_label));
    
    // battery label
    s_battery_label = text_layer_create(GRect(3, 0, 30, 20));
    text_layer_set_text_color(s_battery_label, color_cornertext);
    text_layer_set_background_color(s_battery_label, color_cornertextbackground);
    text_layer_set_font(s_battery_label, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    text_layer_set_text_alignment(s_battery_label, GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(s_battery_label));
    
    // bluetooth label
    s_bluetooth_label = text_layer_create(GRect(110, 0, 30, 20));
    text_layer_set_text_color(s_bluetooth_label, color_cornertext);
    text_layer_set_background_color(s_bluetooth_label, color_cornertextbackground);
    text_layer_set_font(s_bluetooth_label, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    text_layer_set_text_alignment(s_bluetooth_label, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(s_bluetooth_label));
    
    // add misc label, sunrise time or humidity
    s_misc_label = text_layer_create(GRect(3, 150, 48, 20));
    text_layer_set_text_color(s_misc_label, color_cornertext);
    text_layer_set_background_color(s_misc_label, color_cornertextbackground);
    text_layer_set_font(s_misc_label, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    text_layer_set_text_alignment(s_misc_label, GTextAlignmentLeft);
    text_layer_set_text(s_misc_label, cachedWeather.misc);
    layer_add_child(window_layer, text_layer_get_layer(s_misc_label));
    
    // add misc2 label, sunset time or wind speed
    s_misc2_label = text_layer_create(GRect(94, 150, 48, 20));
    text_layer_set_text_color(s_misc2_label, color_cornertext);
    text_layer_set_background_color(s_misc2_label, color_cornertextbackground);
    text_layer_set_font(s_misc2_label, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    text_layer_set_text_alignment(s_misc2_label, GTextAlignmentRight);
    text_layer_set_text(s_misc2_label, cachedWeather.misc2);
    layer_add_child(window_layer, text_layer_get_layer(s_misc2_label));

    // add misc3 label, city and update time
    s_misc3_label = text_layer_create(GRect(bounds.size.w / 2 - 40, 48, 80, 60));
    text_layer_set_text_color(s_misc3_label, color_maintext);
    text_layer_set_background_color(s_misc3_label, color_maintextbackground);
    #ifdef PBL_PLATFORM_BASALT
        text_layer_set_font(s_misc3_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    #else
        text_layer_set_font(s_misc3_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    #endif
    text_layer_set_text_alignment(s_misc3_label, GTextAlignmentCenter);
    text_layer_set_text(s_misc3_label, cachedWeather.misc3);
    layer_add_child(window_layer, text_layer_get_layer(s_misc3_label));
    
	// show hands
	s_hands_layer = layer_create(bounds);
	layer_set_update_proc(s_hands_layer, hands_update_proc);
	layer_add_child(window_layer, s_hands_layer);  

	// appsync dictionary initial setup
	// if I don't sync all appkeys, I get sync errors, but no idea why...
	Tuplet initial_values[] = {
		TupletInteger(WEATHER_ICON, (uint8_t) cachedWeather.icon_current), // loading icon
        TupletCString(WEATHER_WEATHERDATA, cachedWeather.weather),
        TupletCString(WEATHER_MISC, cachedWeather.misc),
        TupletCString(WEATHER_MISC2, cachedWeather.misc2),
        TupletCString(WEATHER_MISC3, cachedWeather.misc3),
        TupletInteger(CONFIG_SECONDS, (uint8_t) settings.seconds),
        TupletInteger(CONFIG_HOURVIBES, (uint8_t) settings.hourvibes),
        TupletInteger(CONFIG_REVERSE, (uint8_t) settings.reverse),
    };

	app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer),
	  initial_values, ARRAY_LENGTH(initial_values),
	  sync_tuple_changed_callback, sync_error_callback, NULL
	);

	// init status labels
	handle_battery(battery_state_service_peek());
	handle_bluetooth(bluetooth_connection_service_peek());

  	// get weather on load
 	request_weather();
}


//======================================
// WINDOW UNLOAD
//======================================
static void window_unload(Window *window) {
	layer_destroy(s_simple_bg_layer);
	layer_destroy(s_date_layer);
    layer_destroy(s_text_layer);

	text_layer_destroy(s_label_12);
	text_layer_destroy(s_label_9);
	text_layer_destroy(s_day_label);
	text_layer_destroy(s_battery_label);
	text_layer_destroy(s_bluetooth_label);
	text_layer_destroy(s_weather_label);
	text_layer_destroy(s_misc_label);
	text_layer_destroy(s_misc2_label);
    text_layer_destroy(s_misc3_label);
    
	fonts_unload_custom_font(custom_font);

	if (s_icon_bitmap) {
    	gbitmap_destroy(s_icon_bitmap);
	}

	layer_destroy(s_hands_layer);
    
}


//======================================
// INIT
//======================================
static void init() {
    // load persistent settings from struct
    if (persist_exists(PERSIST_SETTINGS)) {
        persist_read_data(PERSIST_SETTINGS, &settings, sizeof(settings));
    }

    // load cached data from struct
    if (persist_exists(PERSIST_WEATHERDATA)) {
        persist_read_data(PERSIST_WEATHERDATA, &cachedWeather, sizeof(cachedWeather));
    }

    // set second or minute updates
    if (settings.seconds == 1) {
        tick_timer_service_subscribe(SECOND_UNIT, handle_time_tick);
    }
    else {
        tick_timer_service_subscribe(MINUTE_UNIT, handle_time_tick);
    }

    // set color settings
    color_handler();
    
	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
	.load = window_load,
	.unload = window_unload,
	});
	window_stack_push(window, true);
	app_message_open(128, 256);

//	s_day_buffer[0] = '\0';
//	s_weather_buffer[0] = '\0';
//	s_misc_buffer[0] = '\0';
//	s_misc2_buffer[0] = '\0';
//    s_misc3_buffer[0] = '\0';

	// init hand paths
	s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
	s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	GPoint center = grect_center_point(&bounds);
	gpath_move_to(s_minute_arrow, center);
	gpath_move_to(s_hour_arrow, center);

	// init battery and bluetooth handlers
	battery_state_service_subscribe(handle_battery);
	bluetooth_connection_service_subscribe(handle_bluetooth);
}


//======================================
// DEINIT
//======================================
static void deinit() {
    // write persists
    persist_write_data(PERSIST_SETTINGS, &settings, sizeof(settings));
    persist_write_data(PERSIST_WEATHERDATA, &cachedWeather, sizeof(cachedWeather));
    
    app_sync_deinit(&s_sync);

    gpath_destroy(s_minute_arrow);
    gpath_destroy(s_hour_arrow);

    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();
    bluetooth_connection_service_unsubscribe();
    window_destroy(window);
}


//======================================
// MAIN
//======================================
int main() {
  init();
  app_event_loop();
  deinit();
}
