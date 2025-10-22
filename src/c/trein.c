/* 
 * This file is part of the Trein Pebble app distribution (https://github.com/guusbeckett/trein-pebble).
 * Copyright (c) 2025 Guus Beckett.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <pebble.h>
#include <stdlib.h>
#include "stations.h"
#include "trein_data.h"

// --- Function Declarations ---
static void prv_send_trip_request();
static void prv_dest_menu_window_load(Window *window);
static void prv_dest_menu_window_unload(Window *window);
static void prv_alpha_menu_window_load(Window *window);
static void prv_alpha_menu_window_unload(Window *window);
static void prv_confirmation_window_load(Window *window);
static void prv_confirmation_window_unload(Window *window);
static void prv_confirmation_click_config_provider(void *context);
static void prv_update_confirmation_display();
static void prv_update_confirmation_display_animated(AnimationDirection direction);
static void prv_animation_started_handler(Animation *animation, void *context);
static void prv_animation_stopped_handler(Animation *animation, bool finished, void *context);
static void prv_fade_in_stopped_handler(Animation *animation, bool finished, void *context);
static void prv_trip_leg_layer_update_proc(Layer *layer, GContext *ctx);

// --- Global Application Data ---
static AppData s_app;
#ifdef PBL_COLOR
// This function will be used to draw the blue top bar
static void prv_bg_blue_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

// This function will be used to draw the yellow main area
static void prv_bg_yellow_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorYellow);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}
#endif

// All data moved to s_app structure defined in trein_data.h

static void prv_countdown_timer_callback(void *data) {
  time_t now = time(NULL);
  int remaining_seconds = s_app.state.departure_time - now;
  if (strncmp (s_app.trips.delay[s_app.journey.selected_trip_index],"Cancelled",9) == 0) {
    text_layer_set_font(s_app.conf_ui.countdown_layer, fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS));
    text_layer_set_text(s_app.conf_ui.countdown_layer, "--:--");
    return;
  }
  if (remaining_seconds > 0) {
    int hours = remaining_seconds / 3600;
    int minutes = (remaining_seconds % 3600) / 60;
    int seconds = remaining_seconds % 60;

    if (hours > 0) {
      snprintf(s_app.buffers.countdown_buffer, sizeof(s_app.buffers.countdown_buffer), "%02d:%02d", hours, minutes);
    } else {
      snprintf(s_app.buffers.countdown_buffer, sizeof(s_app.buffers.countdown_buffer), "%02d:%02d", minutes, seconds);
    }
    text_layer_set_font(s_app.conf_ui.countdown_layer, fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS));
    text_layer_set_text(s_app.conf_ui.countdown_layer, s_app.buffers.countdown_buffer);
    s_app.state.countdown_timer = app_timer_register(1000, prv_countdown_timer_callback, NULL);
  } else {
    text_layer_set_font(s_app.conf_ui.countdown_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_text(s_app.conf_ui.countdown_layer, "Departed");
  }
}

static void prv_parse_time_and_start_timer() {
  if (s_app.state.countdown_timer) {
    app_timer_cancel(s_app.state.countdown_timer);
    s_app.state.countdown_timer = NULL;
  }
  if (s_app.trips.count == 0 || s_app.trips.departures[s_app.journey.selected_trip_index] == 0) {
    text_layer_set_text(s_app.conf_ui.countdown_layer, "--:--");
    return;
  }
  s_app.state.departure_time = s_app.trips.departures[s_app.journey.selected_trip_index];
  prv_countdown_timer_callback(NULL);
}

static void prv_trip_leg_layer_update_proc(Layer *layer, GContext *ctx) {
  int transfers = 0;
  if (s_app.trips.count > 0 && s_app.trips.transfers[s_app.journey.selected_trip_index][0] != '\0') {
    transfers = atoi(s_app.trips.transfers[s_app.journey.selected_trip_index]);
  }
  int num_legs = transfers + 1;

  const int max_legs = 12;
  if (num_legs > max_legs) {
    num_legs = max_legs;
  }

  graphics_context_set_stroke_width(ctx, 2);

#ifdef PBL_ROUND
  GRect unobstructed_bounds = layer_get_unobstructed_bounds(layer);

  GPoint center = grect_center_point(&unobstructed_bounds);
  int radius = (unobstructed_bounds.size.w / 2) - 8;
  
  int32_t total_angle = TRIG_MAX_ANGLE * 80 / 360;
  int32_t center_angle = TRIG_MAX_ANGLE / 4;
  int32_t start_angle = center_angle - (total_angle / 2);

  int32_t angle_per_leg = total_angle / num_legs;
  int32_t gap_angle = angle_per_leg / 8;

  GRect polar_rect = GRect(center.x - radius, center.y - radius, radius * 2, radius * 2);
  GPoint dot_points[max_legs * 2];

  for (int i = 0; i < num_legs; i++) {
    int32_t leg_start_angle_trig = start_angle + i * angle_per_leg + gap_angle;
    int32_t leg_end_angle_trig = start_angle + (i + 1) * angle_per_leg - gap_angle;

    GPoint p0 = gpoint_from_polar(polar_rect, GOvalScaleModeFitCircle, leg_start_angle_trig);
    GPoint p1 = gpoint_from_polar(polar_rect, GOvalScaleModeFitCircle, leg_end_angle_trig);
    
    dot_points[i * 2] = p0;
    dot_points[i * 2 + 1] = p1;

    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_arc(ctx, polar_rect, GOvalScaleModeFitCircle, leg_start_angle_trig, leg_end_angle_trig);
  }

  for (int i = 0; i < num_legs * 2; i++) {
    GPoint p = dot_points[i];
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, p, 4);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, p, 2);
  }

#else // PBL_RECT
  GRect bounds = layer_get_bounds(layer);
  int line_x = bounds.size.w - 12;
  int total_height = bounds.size.h - 80;
  int start_y = 40;

  int height_per_leg = total_height / num_legs;
  int gap_y = height_per_leg / 8;
  GPoint dot_points[max_legs * 2];

  for (int i = 0; i < num_legs; i++) {
    int leg_start_y = start_y + i * height_per_leg + gap_y;
    int leg_end_y = start_y + (i + 1) * height_per_leg - gap_y;
    
    GPoint p0 = GPoint(line_x, leg_start_y);
    GPoint p1 = GPoint(line_x, leg_end_y);
    
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_line(ctx, p0, p1);

    dot_points[i * 2] = p0;
    dot_points[i * 2 + 1] = p1;
  }

  for (int i = 0; i < num_legs * 2; i++) {
    GPoint p = dot_points[i];
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, p, 4);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, p, 2);
  }
#endif
}

static void prv_update_confirmation_display() {
  snprintf(s_app.buffers.platform_buffer, sizeof(s_app.buffers.platform_buffer), "Platform %s", s_app.trips.platform[s_app.journey.selected_trip_index]);
  text_layer_set_text(s_app.conf_ui.platform_layer, s_app.buffers.platform_buffer);
  APP_LOG(APP_LOG_LEVEL_INFO, "s_trip_departures[s_selected_trip_index]: %d", s_app.trips.departures[s_app.journey.selected_trip_index]);
  if (strncmp (s_app.trips.delay[s_app.journey.selected_trip_index],"Cancelled",9) == 0) {
    snprintf(s_app.buffers.delay_buffer, sizeof(s_app.buffers.delay_buffer), "%s", "");
    text_layer_set_text(s_app.conf_ui.delay_layer, s_app.buffers.delay_buffer);
  }
  else {
    snprintf(s_app.buffers.delay_buffer, sizeof(s_app.buffers.delay_buffer), "%s", s_app.trips.delay[s_app.journey.selected_trip_index]);
    text_layer_set_text(s_app.conf_ui.delay_layer, s_app.buffers.delay_buffer);
  }

  if (s_app.trips.planned_departures[s_app.journey.selected_trip_index][0] != '\0') {
    strncpy(s_app.buffers.departure_time_buffer, &s_app.trips.planned_departures[s_app.journey.selected_trip_index][11], 5);
    s_app.buffers.departure_time_buffer[5] = '\0';
    text_layer_set_text(s_app.conf_ui.departure_time_layer, s_app.buffers.departure_time_buffer);
  }

if (s_app.trips.planned_arrivals[s_app.journey.selected_trip_index][0] != '\0') {
    if (strcmp(s_app.trips.planned_arrivals[s_app.journey.selected_trip_index], "--:--") == 0) {
      snprintf(s_app.buffers.arrival_time_buffer, sizeof(s_app.buffers.arrival_time_buffer), "--:--");
    } else {
      strncpy(s_app.buffers.arrival_time_buffer, &s_app.trips.planned_arrivals[s_app.journey.selected_trip_index][11], 5);
      s_app.buffers.arrival_time_buffer[5] = '\0';
    }
    text_layer_set_text(s_app.conf_ui.arrival_time_layer, s_app.buffers.arrival_time_buffer);
  }

  if(s_app.conf_ui.trip_leg_layer) {
    layer_mark_dirty(s_app.conf_ui.trip_leg_layer);
  }

  prv_parse_time_and_start_timer();
}

// Animation callbacks
static void prv_animation_started_handler(Animation *animation, void *context) {
  // Animation started, we can safely null out the pointer
  s_app.state.content_animation = NULL;
}

static void prv_animation_stopped_handler(Animation *animation, bool finished, void *context) {
  if (!finished) {
    s_app.state.is_animating = false;
    return;
  }

  // Update all text content with new trip data
  prv_update_confirmation_display();

  // Fade in from opposite direction
  Layer *window_layer = window_get_root_layer(s_app.windows.confirmation_window);
  GRect bounds = layer_get_bounds(window_layer);

  // Come from opposite direction: if we slid up (UP button), new content comes from below
  // if we slid down (DOWN button), new content comes from above
  int offset = -20 * s_app.state.animation_direction;
  GRect from_frame = GRect(bounds.origin.x, bounds.origin.y + offset, bounds.size.w, bounds.size.h);
  GRect to_frame = bounds;

  s_app.state.content_animation = property_animation_create_layer_frame(window_layer, &from_frame, &to_frame);
  animation_set_duration((Animation*)s_app.state.content_animation, 200);
  animation_set_curve((Animation*)s_app.state.content_animation, AnimationCurveEaseOut);
  animation_set_handlers((Animation*)s_app.state.content_animation, (AnimationHandlers) {
    .started = prv_animation_started_handler,
    .stopped = prv_fade_in_stopped_handler
  }, NULL);
  animation_schedule((Animation*)s_app.state.content_animation);
}

static void prv_fade_in_stopped_handler(Animation *animation, bool finished, void *context) {
  s_app.state.is_animating = false;
}

static void prv_update_confirmation_display_animated(AnimationDirection direction) {
  if (s_app.state.is_animating) return;

  s_app.state.is_animating = true;
  s_app.state.animation_direction = direction;

  // Slide out in the direction of the button press
  Layer *window_layer = window_get_root_layer(s_app.windows.confirmation_window);
  GRect bounds = layer_get_bounds(window_layer);
  GRect from_frame = bounds;

  // UP button: slide upward (negative), DOWN button: slide downward (positive)
  int offset = 20 * direction;
  GRect to_frame = GRect(bounds.origin.x, bounds.origin.y + offset, bounds.size.w, bounds.size.h);

  // Only destroy if animation still exists (hasn't been auto-freed)
  if (s_app.state.content_animation) {
    animation_unschedule((Animation*)s_app.state.content_animation);
    property_animation_destroy(s_app.state.content_animation);
    s_app.state.content_animation = NULL;
  }

  s_app.state.content_animation = property_animation_create_layer_frame(window_layer, &from_frame, &to_frame);
  animation_set_duration((Animation*)s_app.state.content_animation, 200);
  animation_set_curve((Animation*)s_app.state.content_animation, AnimationCurveEaseIn);
  animation_set_handlers((Animation*)s_app.state.content_animation, (AnimationHandlers) {
    .started = prv_animation_started_handler,
    .stopped = prv_animation_stopped_handler
  }, NULL);
  animation_schedule((Animation*)s_app.state.content_animation);
}

static void prv_confirmation_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_app.trips.count > 0 && !s_app.state.is_animating) {
    s_app.journey.selected_trip_index++;
    if (s_app.journey.selected_trip_index >= s_app.trips.count) { s_app.journey.selected_trip_index = 0; }
    prv_update_confirmation_display_animated(ANIMATION_DIRECTION_UP);
  }
}

static void prv_confirmation_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_app.trips.count > 0 && !s_app.state.is_animating) {
    s_app.journey.selected_trip_index--;
    if (s_app.journey.selected_trip_index < 0) { s_app.journey.selected_trip_index = s_app.trips.count - 1; }
    prv_update_confirmation_display_animated(ANIMATION_DIRECTION_DOWN);
  }
}

static void prv_fallback_timer_callback(void *context) {
  if (!s_app.stations.loaded) {
    text_layer_set_text(s_app.main_ui.text_layer, "Failed to fetch stations...");
  }
  s_app.state.fallback_timer = NULL;
}

static uint16_t prv_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return s_app.stations.loaded ? s_app.stations.count : 1;
}

static void prv_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  if (s_app.stations.loaded && cell_index->row < s_app.stations.count) {
    menu_cell_basic_draw(ctx, cell_layer, s_app.stations.names[cell_index->row], NULL, NULL);
  } else {
    menu_cell_basic_draw(ctx, cell_layer, "Loading...", NULL, NULL);
  }
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  if (!s_app.stations.loaded) { return; }
  s_app.state.last_selected_index = cell_index->row;
  strncpy(s_app.journey.start_station_name, s_app.stations.names[cell_index->row], sizeof(s_app.journey.start_station_name) - 1);
  strncpy(s_app.journey.start_station_code, s_app.stations.codes[cell_index->row], sizeof(s_app.journey.start_station_code) - 1);
  if (!s_app.windows.dest_menu_window) {
    s_app.windows.dest_menu_window = window_create();
    window_set_window_handlers(s_app.windows.dest_menu_window, (WindowHandlers) {
      .load = prv_dest_menu_window_load, .unload = prv_dest_menu_window_unload,
    });
  }
  window_stack_push(s_app.windows.dest_menu_window, true);
}

static void prv_menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_app.menu_layers.menu_layer = menu_layer_create(bounds);
  menu_layer_set_click_config_onto_window(s_app.menu_layers.menu_layer, window);
  menu_layer_set_callbacks(s_app.menu_layers.menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_menu_get_num_rows_callback,
    .draw_row = prv_menu_draw_row_callback,
    .select_click = prv_menu_select_callback,
  });
  #ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_app.menu_layers.menu_layer, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(s_app.menu_layers.menu_layer, GColorOxfordBlue, GColorWhite);
  #endif
  layer_add_child(window_layer, menu_layer_get_layer(s_app.menu_layers.menu_layer));
  MenuIndex index = MenuIndex(0, s_app.state.last_selected_index);
  menu_layer_set_selected_index(s_app.menu_layers.menu_layer, index, MenuRowAlignCenter, false);
}

static void prv_menu_window_unload(Window *window) { menu_layer_destroy(s_app.menu_layers.menu_layer); }

static uint16_t prv_alpha_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return alphabet_index[s_app.state.selected_alphabet_index].count;
}

static void prv_alpha_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  int station_index = alphabet_index[s_app.state.selected_alphabet_index].start_index + cell_index->row;
  const Station *station = &all_stations[station_index];
  menu_cell_basic_draw(ctx, cell_layer, station->name, NULL, NULL);
}

static void prv_alpha_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  int station_index = alphabet_index[s_app.state.selected_alphabet_index].start_index + cell_index->row;
  const Station *station = &all_stations[station_index];
  strncpy(s_app.journey.dest_station_code, station->code, sizeof(s_app.journey.dest_station_code) - 1);
  strncpy(s_app.journey.dest_station_name, station->name, sizeof(s_app.journey.dest_station_name) - 1);
  prv_send_trip_request();
}

static void prv_alpha_menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_app.menu_layers.alpha_menu_layer = menu_layer_create(bounds);
  menu_layer_set_click_config_onto_window(s_app.menu_layers.alpha_menu_layer, window);
  menu_layer_set_callbacks(s_app.menu_layers.alpha_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_alpha_menu_get_num_rows_callback,
    .draw_row = prv_alpha_menu_draw_row_callback,
    .select_click = prv_alpha_menu_select_callback,
  });
  #ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_app.menu_layers.alpha_menu_layer, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(s_app.menu_layers.alpha_menu_layer, GColorOxfordBlue, GColorWhite);
  #endif
  layer_add_child(window_layer, menu_layer_get_layer(s_app.menu_layers.alpha_menu_layer));
}

static void prv_alpha_menu_window_unload(Window *window) { menu_layer_destroy(s_app.menu_layers.alpha_menu_layer); }

static uint16_t prv_dest_menu_get_num_sections_callback(MenuLayer *menu_layer, void *context) { return 2; }

static uint16_t prv_dest_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return (section_index == 0) ? NUM_TOP_STATIONS : ALPHABET_INDEX_COUNT;
}

static void prv_dest_menu_draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *context) {
  snprintf(s_app.buffers.section_header, sizeof(s_app.buffers.section_header), (section_index == 0) ? "Destination" : "By Letter");
  menu_cell_basic_header_draw(ctx, cell_layer, s_app.buffers.section_header);
}

static void prv_dest_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->section == 0) {
    const Station *station = &top_stations[cell_index->row];
    menu_cell_basic_draw(ctx, cell_layer, station->name, NULL, NULL);
  } else {
    s_app.buffers.letter_str[0] = alphabet_index[cell_index->row].letter;
    s_app.buffers.letter_str[1] = '\0';
    menu_cell_basic_draw(ctx, cell_layer, s_app.buffers.letter_str, NULL, NULL);
  }
}

static void prv_dest_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->section == 0) {
    const Station *station = &top_stations[cell_index->row];
    strncpy(s_app.journey.dest_station_code, station->code, sizeof(s_app.journey.dest_station_code) - 1);
    strncpy(s_app.journey.dest_station_name, station->name, sizeof(s_app.journey.dest_station_name) - 1);
    prv_send_trip_request();
  } else {
    s_app.state.selected_alphabet_index = cell_index->row;
    if (!s_app.windows.alpha_menu_window) {
      s_app.windows.alpha_menu_window = window_create();
      window_set_window_handlers(s_app.windows.alpha_menu_window, (WindowHandlers) {
        .load = prv_alpha_menu_window_load, .unload = prv_alpha_menu_window_unload,
      });
    }
    window_stack_push(s_app.windows.alpha_menu_window, true);
  }
}

static void prv_dest_menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_app.menu_layers.dest_menu_layer = menu_layer_create(bounds);
  menu_layer_set_click_config_onto_window(s_app.menu_layers.dest_menu_layer, window);
  menu_layer_set_callbacks(s_app.menu_layers.dest_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_sections = prv_dest_menu_get_num_sections_callback,
    .get_num_rows = prv_dest_menu_get_num_rows_callback,
    .draw_header = prv_dest_menu_draw_header_callback,
    .draw_row = prv_dest_menu_draw_row_callback,
    .select_click = prv_dest_menu_select_callback,
  });
  #ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_app.menu_layers.dest_menu_layer, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(s_app.menu_layers.dest_menu_layer, GColorOxfordBlue, GColorWhite);
  #endif
  layer_add_child(window_layer, menu_layer_get_layer(s_app.menu_layers.dest_menu_layer));
}

static void prv_dest_menu_window_unload(Window *window) { menu_layer_destroy(s_app.menu_layers.dest_menu_layer); }

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *station_index_tuple = dict_find(iter, MESSAGE_KEY_STATION_INDEX);
  Tuple *station_name_tuple = dict_find(iter, MESSAGE_KEY_STATION_NAME);
  Tuple *station_code_tuple = dict_find(iter, MESSAGE_KEY_STATION_CODE);
  Tuple *station_count_tuple = dict_find(iter, MESSAGE_KEY_STATION_COUNT);
  Tuple *trip_index_tuple = dict_find(iter, MESSAGE_KEY_TRIP_INDEX);
  Tuple *trip_planned_departure_time_tuple = dict_find(iter, MESSAGE_KEY_TRIP_PLANNED_DEPARTURE_TIME);
  Tuple *trip_departure_time_epoch_tuple = dict_find(iter, MESSAGE_KEY_TRIP_DEPARTURE_TIME_EPOCH);
  Tuple *trip_planned_arrival_time_tuple = dict_find(iter, MESSAGE_KEY_TRIP_PLANNED_ARRIVAL_TIME);
  Tuple *trip_arrival_time_tuple = dict_find(iter, MESSAGE_KEY_TRIP_ARRIVAL_TIME);
  Tuple *trip_transfers_tuple = dict_find(iter, MESSAGE_KEY_TRIP_TRANSFERS);
  Tuple *trip_platform_tuple = dict_find(iter, MESSAGE_KEY_TRIP_PLATFORM);
  Tuple *trip_count_tuple = dict_find(iter, MESSAGE_KEY_TRIP_COUNT);
  Tuple *trip_delay_tuple = dict_find(iter, MESSAGE_KEY_TRIP_DELAY);
  Tuple *error_tuple = dict_find(iter, MESSAGE_KEY_ERROR);
  
  if (error_tuple) {
    text_layer_set_text(s_app.main_ui.text_layer, "Add API key in settings...");
    return;
  }

  if (station_index_tuple && station_name_tuple && station_count_tuple && station_code_tuple) {
    int index = station_index_tuple->value->int32;
    const char *name = station_name_tuple->value->cstring;
    const char *code = station_code_tuple->value->cstring;
    int count = station_count_tuple->value->int32;
    if (index >= 0 && index < MAX_STATIONS) {
      strncpy(s_app.stations.names[index], name, MAX_STATION_NAME_LENGTH - 1);
      s_app.stations.names[index][MAX_STATION_NAME_LENGTH - 1] = '\0';
      strncpy(s_app.stations.codes[index], code, MAX_STATION_CODE_LENGTH - 1);
      s_app.stations.codes[index][MAX_STATION_CODE_LENGTH-1] = '\0';
      if (index + 1 > s_app.stations.count) { s_app.stations.count = index + 1; }
      if (s_app.stations.count >= count) {
        s_app.stations.loaded = true;
        if (s_app.state.fallback_timer) { app_timer_cancel(s_app.state.fallback_timer); s_app.state.fallback_timer = NULL; }

        if (s_app.menu_layers.menu_layer) { menu_layer_reload_data(s_app.menu_layers.menu_layer); }

        if (!s_app.windows.menu_window) {
          s_app.windows.menu_window = window_create();
          window_set_window_handlers(s_app.windows.menu_window, (WindowHandlers) { .load = prv_menu_window_load, .unload = prv_menu_window_unload, });
        }
        window_stack_push(s_app.windows.menu_window, true);
      }
    }
  }

  if (trip_index_tuple && trip_departure_time_epoch_tuple && trip_arrival_time_tuple && trip_transfers_tuple && trip_count_tuple && trip_platform_tuple && trip_delay_tuple && trip_planned_departure_time_tuple && trip_planned_arrival_time_tuple) {
    int index = trip_index_tuple->value->int32;
    const char *planned_departure_time = trip_planned_departure_time_tuple->value->cstring;
    int departure_time = trip_departure_time_epoch_tuple->value->int32;
    const char *planned_arrival_time = trip_planned_arrival_time_tuple->value->cstring;
    const char *arrival_time = trip_arrival_time_tuple->value->cstring;
    int count = trip_count_tuple->value->int32;
    int transfers_val = trip_transfers_tuple->value->int32;
    const char *platform = trip_platform_tuple->value->cstring;
    const char *delay = trip_delay_tuple->value->cstring;

    if (index >= 0 && index < MAX_TRIPS) {
      strncpy(s_app.trips.planned_departures[index], planned_departure_time, MAX_DATE_TIME_LENGTH - 1);
      s_app.trips.planned_departures[index][MAX_DATE_TIME_LENGTH - 1] = '\0';
      s_app.trips.departures[index] = departure_time;

      strncpy(s_app.trips.planned_arrivals[index], planned_arrival_time, MAX_DATE_TIME_LENGTH - 1);
      s_app.trips.planned_arrivals[index][MAX_DATE_TIME_LENGTH - 1] = '\0';
      strncpy(s_app.trips.arrivals[index], arrival_time, MAX_DATE_TIME_LENGTH - 1);
      s_app.trips.arrivals[index][MAX_DATE_TIME_LENGTH - 1] = '\0';

      snprintf(s_app.trips.transfers[index], MAX_TRANSFERS_LENGTH, "%d", transfers_val);
      strncpy(s_app.trips.platform[index], platform, MAX_PLATFORM_LENGTH - 1);
      s_app.trips.platform[index][MAX_PLATFORM_LENGTH - 1] = '\0';

      strncpy(s_app.trips.delay[index], delay, MAX_DELAY_LENGTH - 1);
      s_app.trips.delay[index][MAX_DELAY_LENGTH - 1] = '\0';

      if (index + 1 > s_app.trips.count) { s_app.trips.count = index + 1; }
      if (s_app.trips.count >= count) {
        s_app.trips.loaded = true;
        if (!s_app.windows.confirmation_window) {
          s_app.windows.confirmation_window = window_create();
          window_set_window_handlers(s_app.windows.confirmation_window, (WindowHandlers) {
            .load = prv_confirmation_window_load, .unload = prv_confirmation_window_unload,
          });
          window_set_click_config_provider(s_app.windows.confirmation_window, prv_confirmation_click_config_provider);
        }
        window_stack_push(s_app.windows.confirmation_window, true);
      }
    }
  }
}

static void prv_inbox_dropped_handler(AppMessageResult reason, void *context) { APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason); }
static void prv_outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) { APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", (int)reason); }
static void prv_outbox_sent_handler(DictionaryIterator *iter, void *context) { APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success"); }

static void prv_request_stations_from_phone(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_STATIONS, 1);
    if (app_message_outbox_send() == APP_MSG_OK) {
      text_layer_set_text(s_app.main_ui.text_layer, "Fetching nearby stations...");
    }
  }
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!s_app.stations.loaded) { return; }
  if (!s_app.windows.menu_window) {
    s_app.windows.menu_window = window_create();
    window_set_window_handlers(s_app.windows.menu_window, (WindowHandlers) { .load = prv_menu_window_load, .unload = prv_menu_window_unload, });
  }
  window_stack_push(s_app.windows.menu_window, true);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_app.stations.loaded = false;
  s_app.stations.count = 0;
  prv_request_stations_from_phone();
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  #ifdef PBL_COLOR
    const int bar_height = 40;
    s_app.main_ui.bg_blue_layer = layer_create(GRect(0, 0, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.main_ui.bg_blue_layer, prv_bg_blue_update_proc);
    layer_add_child(window_layer, s_app.main_ui.bg_blue_layer);
    s_app.main_ui.bg_blue_bottom_layer = layer_create(GRect(0, bounds.size.h - bar_height, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.main_ui.bg_blue_bottom_layer, prv_bg_blue_update_proc);
    layer_add_child(window_layer, s_app.main_ui.bg_blue_bottom_layer);
    s_app.main_ui.bg_yellow_layer = layer_create(GRect(0, bar_height, bounds.size.w, bounds.size.h - (bar_height * 2)));
    layer_set_update_proc(s_app.main_ui.bg_yellow_layer, prv_bg_yellow_update_proc);
    layer_add_child(window_layer, s_app.main_ui.bg_yellow_layer);
  #endif

  s_app.main_ui.text_layer = text_layer_create(GRect(0, grect_center_point(&bounds).y - 15, bounds.size.w, 30));
  text_layer_set_text_alignment(s_app.main_ui.text_layer, GTextAlignmentCenter);
  #ifdef PBL_COLOR
    text_layer_set_background_color(s_app.main_ui.text_layer, GColorClear);
    text_layer_set_text_color(s_app.main_ui.text_layer, GColorBlack);
  #endif
  layer_add_child(window_layer, text_layer_get_layer(s_app.main_ui.text_layer));

  prv_request_stations_from_phone();
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_app.main_ui.text_layer);
  #ifdef PBL_COLOR
    layer_destroy(s_app.main_ui.bg_blue_layer);
    layer_destroy(s_app.main_ui.bg_yellow_layer);
    layer_destroy(s_app.main_ui.bg_blue_bottom_layer);
  #endif
}

static void prv_init(void) {
  // Initialize all app data to zero
  memset(&s_app, 0, sizeof(AppData));
  s_app.buffers.letter_str[0] = 'A';
  s_app.buffers.letter_str[1] = '\0';

  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_register_inbox_dropped(prv_inbox_dropped_handler);
  app_message_register_outbox_failed(prv_outbox_failed_handler);
  app_message_register_outbox_sent(prv_outbox_sent_handler);
  app_message_open(256, 256);
  s_app.windows.main_window = window_create();
  window_set_click_config_provider(s_app.windows.main_window, prv_click_config_provider);
  window_set_window_handlers(s_app.windows.main_window, (WindowHandlers) { .load = prv_window_load, .unload = prv_window_unload, });
  window_stack_push(s_app.windows.main_window, true);
  s_app.state.fallback_timer = app_timer_register(10000, prv_fallback_timer_callback, NULL);
}

static void prv_deinit(void) {
  if(s_app.state.fallback_timer) app_timer_cancel(s_app.state.fallback_timer);
  if(s_app.windows.menu_window) window_destroy(s_app.windows.menu_window);
  if(s_app.windows.dest_menu_window) window_destroy(s_app.windows.dest_menu_window);
  if(s_app.windows.alpha_menu_window) window_destroy(s_app.windows.alpha_menu_window);
  if(s_app.windows.confirmation_window) window_destroy(s_app.windows.confirmation_window);
  window_destroy(s_app.windows.main_window);
}

static void prv_send_trip_request(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_cstring(iter, MESSAGE_KEY_START_STATION_CODE, s_app.journey.start_station_code);
    dict_write_cstring(iter, MESSAGE_KEY_DEST_STATION_CODE, s_app.journey.dest_station_code);
    app_message_outbox_send();
  }
}

static void prv_confirmation_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop_all(true);
}

static void prv_confirmation_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_confirmation_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_confirmation_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_confirmation_down_click_handler);
}

 static void prv_confirmation_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  const int bar_height = 40;
  s_app.journey.selected_trip_index = 0;

  #ifdef PBL_COLOR
    s_app.conf_ui.bg_blue_layer = layer_create(GRect(0, 0, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.conf_ui.bg_blue_layer, prv_bg_blue_update_proc);
    layer_add_child(window_layer, s_app.conf_ui.bg_blue_layer);
    s_app.conf_ui.bg_blue_bottom_layer = layer_create(GRect(0, bounds.size.h - bar_height, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.conf_ui.bg_blue_bottom_layer, prv_bg_blue_update_proc);
    layer_add_child(window_layer, s_app.conf_ui.bg_blue_bottom_layer);
    s_app.conf_ui.bg_yellow_layer = layer_create(GRect(0, bar_height, bounds.size.w, bounds.size.h - (bar_height * 2)));
    layer_set_update_proc(s_app.conf_ui.bg_yellow_layer, prv_bg_yellow_update_proc);
    layer_add_child(window_layer, s_app.conf_ui.bg_yellow_layer);
  #endif

  s_app.conf_ui.trip_leg_layer = layer_create(bounds);
  layer_set_update_proc(s_app.conf_ui.trip_leg_layer, prv_trip_leg_layer_update_proc);
  layer_add_child(window_layer, s_app.conf_ui.trip_leg_layer);

  s_app.conf_ui.start_station_layer = text_layer_create(GRect(0, 5, bounds.size.w, 30));
  text_layer_set_text(s_app.conf_ui.start_station_layer, s_app.journey.start_station_name);
  text_layer_set_font(s_app.conf_ui.start_station_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_app.conf_ui.start_station_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_app.conf_ui.start_station_layer, GColorClear);
  #ifdef PBL_COLOR
  text_layer_set_text_color(s_app.conf_ui.start_station_layer, GColorWhite);
  #endif
  layer_add_child(window_layer, text_layer_get_layer(s_app.conf_ui.start_station_layer));

  s_app.conf_ui.destination_layer = text_layer_create(GRect(0, bounds.size.h - bar_height + 5, bounds.size.w, 30));
  text_layer_set_text(s_app.conf_ui.destination_layer, s_app.journey.dest_station_name);
  text_layer_set_font(s_app.conf_ui.destination_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_app.conf_ui.destination_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_app.conf_ui.destination_layer, GColorClear);
  #ifdef PBL_COLOR
  text_layer_set_text_color(s_app.conf_ui.destination_layer, GColorWhite);
  #endif
  layer_add_child(window_layer, text_layer_get_layer(s_app.conf_ui.destination_layer));

  int departure_y, arrival_y;
  const int text_height = 22;

  #ifdef PBL_ROUND
    GRect unobstructed_bounds = layer_get_unobstructed_bounds(window_get_root_layer(window));
    GPoint center = grect_center_point(&unobstructed_bounds);
    int radius = (unobstructed_bounds.size.w / 2) - 8;
    GRect polar_rect = GRect(center.x - radius, center.y - radius, radius * 2, radius * 2);
    
    int32_t top_angle_trig = TRIG_MAX_ANGLE * 50 / 360;
    int32_t bottom_angle_trig = TRIG_MAX_ANGLE * 130 / 360;

    GPoint top_point = gpoint_from_polar(polar_rect, GOvalScaleModeFitCircle, top_angle_trig);
    GPoint bottom_point = gpoint_from_polar(polar_rect, GOvalScaleModeFitCircle, bottom_angle_trig);

    departure_y = top_point.y - text_height + 10;
    arrival_y = bottom_point.y - 5;
  #else // PBL_RECT
    int total_height = bounds.size.h - 80;
    int start_y_lines = 40;
    departure_y = start_y_lines - text_height + 3;
    arrival_y = start_y_lines + total_height - 5;
  #endif

  s_app.conf_ui.departure_time_layer = text_layer_create(
    PBL_IF_ROUND_ELSE(GRect(0, departure_y, bounds.size.w - 30, text_height),
                      GRect(5, departure_y, bounds.size.w - 10, text_height)));
  text_layer_set_font(s_app.conf_ui.departure_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_app.conf_ui.departure_time_layer, GTextAlignmentRight);
  text_layer_set_background_color(s_app.conf_ui.departure_time_layer, GColorClear);
  #ifdef PBL_COLOR
  text_layer_set_text_color(s_app.conf_ui.departure_time_layer, GColorWhite);
  #endif
  layer_add_child(window_layer, text_layer_get_layer(s_app.conf_ui.departure_time_layer));

  s_app.conf_ui.arrival_time_layer = text_layer_create(
    PBL_IF_ROUND_ELSE(GRect(0, arrival_y, bounds.size.w - 30, text_height),
                      GRect(5, arrival_y+5, bounds.size.w - 10, text_height)));
  text_layer_set_font(s_app.conf_ui.arrival_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_app.conf_ui.arrival_time_layer, GTextAlignmentRight);
  text_layer_set_background_color(s_app.conf_ui.arrival_time_layer, GColorClear);
  #ifdef PBL_COLOR
  text_layer_set_text_color(s_app.conf_ui.arrival_time_layer, GColorWhite);
  #endif
  layer_add_child(window_layer, text_layer_get_layer(s_app.conf_ui.arrival_time_layer));

  const int platform_y  = PBL_IF_ROUND_ELSE(40, 35);
  const int delay_y  = PBL_IF_ROUND_ELSE(105, 100);
  const int countdown_y = PBL_IF_ROUND_ELSE(65, 60);

  s_app.conf_ui.platform_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(0, platform_y, bounds.size.w, 30), GRect(5, platform_y - 2, bounds.size.w - 10, 30)));
  text_layer_set_font(s_app.conf_ui.platform_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_app.conf_ui.platform_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_background_color(s_app.conf_ui.platform_layer, GColorClear);
  text_layer_set_text_color(s_app.conf_ui.platform_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(s_app.conf_ui.platform_layer));

  s_app.conf_ui.delay_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(0, delay_y, bounds.size.w, 30), GRect(5, delay_y - 2, bounds.size.w - 10, 30)));
  text_layer_set_font(s_app.conf_ui.delay_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_app.conf_ui.delay_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_background_color(s_app.conf_ui.delay_layer, GColorClear);
  text_layer_set_text_color(s_app.conf_ui.delay_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(s_app.conf_ui.delay_layer));

  s_app.conf_ui.countdown_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(0, countdown_y, bounds.size.w, 50), GRect(5, countdown_y - 2, bounds.size.w - 10, 50)));
  text_layer_set_text(s_app.conf_ui.countdown_layer, "Loading...");
  text_layer_set_font(s_app.conf_ui.countdown_layer, fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS));
  text_layer_set_text_alignment(s_app.conf_ui.countdown_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_background_color(s_app.conf_ui.countdown_layer, GColorClear);
  text_layer_set_text_color(s_app.conf_ui.countdown_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(s_app.conf_ui.countdown_layer));

  prv_update_confirmation_display();
}

static void prv_confirmation_window_unload(Window *window) {
  if (s_app.state.countdown_timer) {
    app_timer_cancel(s_app.state.countdown_timer);
    s_app.state.countdown_timer = NULL;
  }

  // Unschedule animation if running, but don't destroy (animations auto-free when complete)
  if (s_app.state.content_animation) {
    animation_unschedule((Animation*)s_app.state.content_animation);
    property_animation_destroy(s_app.state.content_animation);
    s_app.state.content_animation = NULL;
  }
  s_app.state.is_animating = false;

  text_layer_destroy(s_app.conf_ui.destination_layer);
  text_layer_destroy(s_app.conf_ui.start_station_layer);
  text_layer_destroy(s_app.conf_ui.platform_layer);
  text_layer_destroy(s_app.conf_ui.countdown_layer);
  text_layer_destroy(s_app.conf_ui.departure_time_layer);
  text_layer_destroy(s_app.conf_ui.arrival_time_layer);
  text_layer_destroy(s_app.conf_ui.delay_layer);

  layer_destroy(s_app.conf_ui.trip_leg_layer);

  #ifdef PBL_COLOR
    layer_destroy(s_app.conf_ui.bg_blue_layer);
    layer_destroy(s_app.conf_ui.bg_yellow_layer);
    layer_destroy(s_app.conf_ui.bg_blue_bottom_layer);
  #endif
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}