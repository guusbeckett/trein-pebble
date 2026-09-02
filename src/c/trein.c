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
#include "stations.h"
#include "trein_data.h"

// --- Function Declarations ---
static void prv_send_trip_request();
static void prv_dest_menu_window_load(Window *window);
static void prv_dest_menu_window_unload(Window *window);
static void prv_alpha_menu_window_load(Window *window);
static void prv_alpha_menu_window_unload(Window *window);
static void prv_countdown_window_load(Window *window);
static void prv_countdown_window_unload(Window *window);
static void prv_countdown_click_config_provider(void *context);
static void prv_update_countdown_display();
static void prv_settings_window_load(Window *window);
static void prv_settings_window_unload(Window *window);
static void prv_send_route_request(void);
static void prv_send_settings_to_phone(void);
static void prv_update_countdown_display_animated(AnimationDirection direction);
static void prv_animation_started_handler(Animation *animation, void *context);
static void prv_animation_stopped_handler(Animation *animation, bool finished, void *context);
static void prv_fade_in_stopped_handler(Animation *animation, bool finished, void *context);
static void prv_spinner_layer_update_proc(Layer *layer, GContext *ctx);
static void prv_spinner_timer_callback(void *data);
static void prv_trip_leg_layer_update_proc(Layer *layer, GContext *ctx);
static void prv_journey_details_window_load(Window *window);
static void prv_journey_details_window_unload(Window *window);
static void prv_pin_menu_window_load(Window *window);
static void prv_pin_menu_window_unload(Window *window);

// --- Global Application Data ---
static AppData s_app;

// --- Pin Menu State ---
static SimpleMenuSection s_pin_menu_sections[1];
static SimpleMenuItem s_pin_menu_items[3];
static SimpleMenuLayer *s_pin_simple_menu_layer;
static int s_pin_selected_leg_index;

static SimpleMenuSection s_settings_sections[1];
static SimpleMenuItem s_settings_items[3];
static SimpleMenuLayer *s_settings_menu_layer;
static char s_settings_sub[3][20];

// --- Pin Queue ---
#define MAX_PIN_QUEUE 4
typedef struct { int trip_index; int leg_index; bool is_last; } PinQueueItem;
static PinQueueItem s_pin_queue[MAX_PIN_QUEUE];
static int s_pin_queue_count = 0;
static int s_pin_queue_sent = 0;
static void prv_send_next_pin(void);
#ifdef PBL_COLOR
// This function will be used to draw the blue top bar
static void prv_bg_blue_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static GColor s_mid_band_color;
static void prv_bg_yellow_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, s_mid_band_color);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}
static void prv_set_mid_band(GColor c) {
  if (gcolor_equal(s_mid_band_color, c)) return;
  s_mid_band_color = c;
  if (s_app.countdown_ui.bg_yellow_layer) {
    layer_mark_dirty(s_app.countdown_ui.bg_yellow_layer);
  }
}
#else
// This function will be used to draw the black top/bottom bar for non-color displays
static void prv_bg_black_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}
#endif

// Draw platform indicator with small blue square in top-left
static void prv_platform_border_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Fill with white background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw blue border
  graphics_context_set_stroke_color(ctx, GColorOxfordBlue);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_rect(ctx, bounds);

  // Draw small blue square in top-left corner touching the border
  // Size proportional to the box size
  int square_size = (bounds.size.w == 24) ? 6 : 8;
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, GRect(1, 1, square_size, square_size), 0, GCornerNone);
}

// Draw animated clock spinner
static void prv_spinner_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  // Scale clock to fit the layer (with some padding)
  const int clock_radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 10;
  const int minute_hand_length = clock_radius - 5;
  const int hour_hand_length = clock_radius * 3 / 5;
  const int tick_length = clock_radius / 6;

  // Draw clock circle
  #ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorOxfordBlue);
  graphics_context_set_fill_color(ctx, GColorWhite);
  #else
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorWhite);
  #endif
  graphics_context_set_stroke_width(ctx, 4);
  graphics_fill_circle(ctx, center, clock_radius);
  graphics_draw_circle(ctx, center, clock_radius);

  // Draw clock tick marks (12, 3, 6, 9)
  for (int i = 0; i < 4; i++) {
    int32_t angle = TRIG_MAX_ANGLE * i / 4;
    GPoint outer = gpoint_from_polar(GRect(center.x - clock_radius, center.y - clock_radius,
                                           clock_radius * 2, clock_radius * 2),
                                    GOvalScaleModeFitCircle, angle);
    GPoint inner = {
      .x = center.x + ((outer.x - center.x) * (clock_radius - tick_length)) / clock_radius,
      .y = center.y + ((outer.y - center.y) * (clock_radius - tick_length)) / clock_radius
    };
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_line(ctx, inner, outer);
  }

  // Calculate hand angles based on spinner_angle
  int32_t minute_angle = (TRIG_MAX_ANGLE * s_app.state.spinner_angle / 360) - TRIG_MAX_ANGLE / 4;
  int32_t hour_angle = (TRIG_MAX_ANGLE * (s_app.state.spinner_angle / 12) / 360) - TRIG_MAX_ANGLE / 4;

  // Draw hour hand (shorter, thicker)
  GPoint hour_end = {
    .x = center.x + (sin_lookup(hour_angle) * hour_hand_length / TRIG_MAX_RATIO),
    .y = center.y - (cos_lookup(hour_angle) * hour_hand_length / TRIG_MAX_RATIO)
  };
  graphics_context_set_stroke_width(ctx, clock_radius / 10);
  #ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorOxfordBlue);
  #else
  graphics_context_set_stroke_color(ctx, GColorBlack);
  #endif
  graphics_draw_line(ctx, center, hour_end);

  // Draw minute hand (longer, thinner)
  GPoint minute_end = {
    .x = center.x + (sin_lookup(minute_angle) * minute_hand_length / TRIG_MAX_RATIO),
    .y = center.y - (cos_lookup(minute_angle) * minute_hand_length / TRIG_MAX_RATIO)
  };
  graphics_context_set_stroke_width(ctx, clock_radius / 15);
  graphics_draw_line(ctx, center, minute_end);

  // Draw center dot
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  #else
  graphics_context_set_fill_color(ctx, GColorBlack);
  #endif
  graphics_fill_circle(ctx, center, clock_radius / 8);
}

// Spinner animation timer callback
static void prv_spinner_timer_callback(void *data) {
  s_app.state.spinner_angle = (s_app.state.spinner_angle + 6) % 360;
  if (s_app.main_ui.spinner_layer) {
    layer_mark_dirty(s_app.main_ui.spinner_layer);
    s_app.state.spinner_timer = app_timer_register(50, prv_spinner_timer_callback, NULL);
  }
}

// All data moved to s_app structure defined in trein_data.h

static int prv_atoi(const char *s) {
  int v = 0, sign = 1;
  if (!s) return 0;
  if (*s == '-') { sign = -1; s++; }
  while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
  return sign * v;
}

static void prv_fmt_remain(char *buf, size_t n, int remaining, bool allow_negative) {
  if (!allow_negative && remaining < 0) {
    snprintf(buf, n, "--:--");
    return;
  }
  int neg = remaining < 0;
  if (neg) remaining = -remaining;
  int hours = remaining / 3600;
  int minutes = (remaining % 3600) / 60;
  int seconds = remaining % 60;
  if (hours > 0) snprintf(buf, n, "%s%02d:%02d", neg ? "-" : "", hours, minutes);
  else snprintf(buf, n, "%s%02d:%02d", neg ? "-" : "", minutes, seconds);
}

static void prv_stamp_button(void) {
  s_app.state.last_button_time = time(NULL);
}

static void prv_push_dest_menu(void) {
  if (!s_app.windows.dest_menu_window) {
    s_app.windows.dest_menu_window = window_create();
    window_set_window_handlers(s_app.windows.dest_menu_window, (WindowHandlers) {
      .load = prv_dest_menu_window_load, .unload = prv_dest_menu_window_unload,
    });
  }
  if (!window_stack_contains_window(s_app.windows.dest_menu_window)) {
    window_stack_push(s_app.windows.dest_menu_window, true);
  }
}

static void prv_open_start_station_picker(void) {
  s_app.state.selecting_start_station = true;
  if (s_app.state.fallback_timer) {
    app_timer_cancel(s_app.state.fallback_timer);
    s_app.state.fallback_timer = NULL;
  }
  if (s_app.state.spinner_timer) {
    app_timer_cancel(s_app.state.spinner_timer);
    s_app.state.spinner_timer = NULL;
  }
  prv_push_dest_menu();
}

static void prv_apply_station_choice(const char *code, const char *name) {
  if (s_app.state.selecting_start_station) {
    strncpy(s_app.journey.start_station_code, code, sizeof(s_app.journey.start_station_code) - 1);
    s_app.journey.start_station_code[sizeof(s_app.journey.start_station_code) - 1] = '\0';
    strncpy(s_app.journey.start_station_name, name, sizeof(s_app.journey.start_station_name) - 1);
    s_app.journey.start_station_name[sizeof(s_app.journey.start_station_name) - 1] = '\0';
    s_app.state.selecting_start_station = false;
    if (s_app.windows.alpha_menu_window &&
        window_stack_contains_window(s_app.windows.alpha_menu_window)) {
      window_stack_pop(true);
    }
    return;
  }
  strncpy(s_app.journey.dest_station_code, code, sizeof(s_app.journey.dest_station_code) - 1);
  s_app.journey.dest_station_code[sizeof(s_app.journey.dest_station_code) - 1] = '\0';
  strncpy(s_app.journey.dest_station_name, name, sizeof(s_app.journey.dest_station_name) - 1);
  s_app.journey.dest_station_name[sizeof(s_app.journey.dest_station_name) - 1] = '\0';
  prv_send_trip_request();
}

static void prv_send_route_request(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_uint8(iter, MESSAGE_KEY_REQUEST_ROUTE, 1);
  dict_write_int32(iter, MESSAGE_KEY_ROUTE_MODE, (int32_t)s_app.settings.vervoer_mode);
  app_message_outbox_send();
}

static void prv_send_settings_to_phone(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_int32(iter, MESSAGE_KEY_SETTINGS_TIJD_MODE, (int32_t)s_app.settings.tijd_mode);
  dict_write_int32(iter, MESSAGE_KEY_SETTINGS_REISTIJD, s_app.settings.reistijd_enabled ? 1 : 0);
  dict_write_int32(iter, MESSAGE_KEY_SETTINGS_VERVOER, (int32_t)s_app.settings.vervoer_mode);
  app_message_outbox_send();
}

static void prv_persist_settings(void) {
  persist_write_int(PERSIST_TIJD, (int)s_app.settings.tijd_mode);
  persist_write_bool(PERSIST_REISTIJD, s_app.settings.reistijd_enabled);
  persist_write_int(PERSIST_VERVOER, (int)s_app.settings.vervoer_mode);
}

static void prv_refresh_timer_callback(void *data) {
  s_app.state.refresh_timer = app_timer_register(30000, prv_refresh_timer_callback, NULL);
  if (s_app.state.refresh_in_flight) return;
  s_app.state.refresh_in_flight = true;
  prv_send_route_request();
}

static void prv_countdown_timer_callback(void *data) {
  time_t now = time(NULL);
  int idx = s_app.journey.selected_trip_index;
  int dep_remain = (int)(s_app.state.departure_time - now);
  bool cancelled = (strncmp(s_app.trips.delay[idx], "Cancelled", 9) == 0);
  bool hero_vertrek = s_app.routing.at_station ||
      (!s_app.settings.reistijd_enabled && s_app.settings.tijd_mode != TIJD_MODE_AANKOMST);
  int slack = (s_app.routing.travel_duration_min + s_app.routing.station_offset_min) * 60;
  int over_remain;
  bool over_uses_slack = s_app.settings.reistijd_enabled && !s_app.routing.at_station;

  if (over_uses_slack) {
    over_remain = dep_remain - slack;
  } else if (s_app.settings.tijd_mode == TIJD_MODE_AANKOMST) {
    time_t arr = (time_t)s_app.trips.arrivals_epoch[idx];
    over_remain = arr ? (int)(arr - now) : 0;
  } else {
    over_remain = dep_remain;
  }

#ifdef PBL_COLOR
  if (s_app.countdown_ui.bg_yellow_layer) {
    GColor band = GColorYellow;
    if (hero_vertrek) {
      band = GColorWhite;
    } else if ((over_uses_slack && s_app.routing.have_duration) ||
               (!over_uses_slack && s_app.settings.tijd_mode == TIJD_MODE_AANKOMST)) {
      if (over_remain < 0) band = GColorRed;
      else if (over_remain <= 120) band = GColorYellow;
      else band = GColorGreen;
    }
    prv_set_mid_band(band);
  }
#endif

  layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.vertrek_label_layer), hero_vertrek);
  layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.vertrek_time_layer), hero_vertrek);
  layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.over_label_layer), false);
  layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.over_time_layer), false);

  if (hero_vertrek) {
    text_layer_set_text(s_app.countdown_ui.over_label_layer, "VERTREK");
  } else {
    text_layer_set_text(s_app.countdown_ui.over_label_layer, "OVER");
    text_layer_set_text(s_app.countdown_ui.vertrek_label_layer, "VERTREK");
    if (cancelled) {
      text_layer_set_text(s_app.countdown_ui.over_time_layer, "--:--");
    } else if (over_uses_slack && !s_app.routing.have_duration) {
      text_layer_set_text(s_app.countdown_ui.over_time_layer, "...");
    } else {
      prv_fmt_remain(s_app.buffers.over_buffer, sizeof(s_app.buffers.over_buffer), over_remain, true);
      text_layer_set_text(s_app.countdown_ui.over_time_layer, s_app.buffers.over_buffer);
    }
  }

  if (cancelled) {
    snprintf(s_app.buffers.vertrek_buffer, sizeof(s_app.buffers.vertrek_buffer), "--:--");
  } else if (dep_remain > 0) {
    prv_fmt_remain(s_app.buffers.vertrek_buffer, sizeof(s_app.buffers.vertrek_buffer), dep_remain, false);
  } else {
    snprintf(s_app.buffers.vertrek_buffer, sizeof(s_app.buffers.vertrek_buffer), "Departed");
    time_t idle_since = s_app.state.last_button_time > s_app.state.departure_time
        ? s_app.state.last_button_time : s_app.state.departure_time;
    if (s_app.state.departure_time && (now - idle_since) >= 180) {
      window_stack_pop_all(true);
      return;
    }
  }
  if (hero_vertrek) {
    text_layer_set_text(s_app.countdown_ui.over_time_layer, s_app.buffers.vertrek_buffer);
  } else {
    text_layer_set_text(s_app.countdown_ui.vertrek_time_layer, s_app.buffers.vertrek_buffer);
  }

  s_app.state.countdown_timer = app_timer_register(1000, prv_countdown_timer_callback, NULL);
}

static void prv_parse_time_and_start_timer() {
  if (s_app.state.countdown_timer) {
    app_timer_cancel(s_app.state.countdown_timer);
    s_app.state.countdown_timer = NULL;
  }
  if (s_app.trips.count == 0 || s_app.trips.departures[s_app.journey.selected_trip_index] == 0) {
    text_layer_set_text(s_app.countdown_ui.vertrek_time_layer, "--:--");
    return;
  }
  s_app.state.departure_time = s_app.trips.departures[s_app.journey.selected_trip_index];
  prv_countdown_timer_callback(NULL);
}

static void prv_clock_timer_callback(void *data) {
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);

  strftime(s_app.buffers.clock_buffer, sizeof(s_app.buffers.clock_buffer), "%H:%M", current_time);
  text_layer_set_text(s_app.countdown_ui.clock_layer, s_app.buffers.clock_buffer);

  // Calculate milliseconds until next minute
  int seconds_until_next_minute = 60 - current_time->tm_sec;
  s_app.state.clock_timer = app_timer_register(seconds_until_next_minute * 1000, prv_clock_timer_callback, NULL);
}

static void prv_trip_leg_layer_update_proc(Layer *layer, GContext *ctx) {
  int transfers = 0;
  if (s_app.trips.count > 0 && s_app.trips.transfers[s_app.journey.selected_trip_index][0] != '\0') {
    transfers = prv_atoi(s_app.trips.transfers[s_app.journey.selected_trip_index]);
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

static void prv_update_countdown_display() {
  text_layer_set_text(s_app.countdown_ui.platform_number_layer, s_app.trips.platform[s_app.journey.selected_trip_index]);

  // Always show the delay text (including "Cancelled" if applicable)
  snprintf(s_app.buffers.delay_buffer, sizeof(s_app.buffers.delay_buffer), "%s", s_app.trips.delay[s_app.journey.selected_trip_index]);
  text_layer_set_text(s_app.countdown_ui.delay_layer, s_app.buffers.delay_buffer);

  if (s_app.trips.planned_departures[s_app.journey.selected_trip_index][0] != '\0') {
    strncpy(s_app.buffers.departure_time_buffer, &s_app.trips.planned_departures[s_app.journey.selected_trip_index][11], 5);
    s_app.buffers.departure_time_buffer[5] = '\0';
    text_layer_set_text(s_app.countdown_ui.departure_time_layer, s_app.buffers.departure_time_buffer);
  }

if (s_app.trips.planned_arrivals[s_app.journey.selected_trip_index][0] != '\0') {
    if (strcmp(s_app.trips.planned_arrivals[s_app.journey.selected_trip_index], "--:--") == 0) {
      snprintf(s_app.buffers.arrival_time_buffer, sizeof(s_app.buffers.arrival_time_buffer), "--:--");
    } else {
      strncpy(s_app.buffers.arrival_time_buffer, &s_app.trips.planned_arrivals[s_app.journey.selected_trip_index][11], 5);
      s_app.buffers.arrival_time_buffer[5] = '\0';
    }
    text_layer_set_text(s_app.countdown_ui.arrival_time_layer, s_app.buffers.arrival_time_buffer);
  }

  if(s_app.countdown_ui.trip_leg_layer) {
    layer_mark_dirty(s_app.countdown_ui.trip_leg_layer);
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
  prv_update_countdown_display();

  // Fade in from opposite direction
  Layer *window_layer = window_get_root_layer(s_app.windows.countdown_window);
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

static void prv_update_countdown_display_animated(AnimationDirection direction) {
  if (s_app.state.is_animating) return;

  s_app.state.is_animating = true;
  s_app.state.animation_direction = direction;

  // Slide out in the direction of the button press
  Layer *window_layer = window_get_root_layer(s_app.windows.countdown_window);
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

static void prv_countdown_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_stamp_button();
  if (s_app.trips.count > 0 && !s_app.state.is_animating) {
    s_app.journey.selected_trip_index++;
    if (s_app.journey.selected_trip_index >= s_app.trips.count) { s_app.journey.selected_trip_index = 0; }
    prv_update_countdown_display_animated(ANIMATION_DIRECTION_UP);
  }
}

static void prv_countdown_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_stamp_button();
  if (s_app.trips.count > 0 && !s_app.state.is_animating) {
    s_app.journey.selected_trip_index--;
    if (s_app.journey.selected_trip_index < 0) { s_app.journey.selected_trip_index = s_app.trips.count - 1; }
    prv_update_countdown_display_animated(ANIMATION_DIRECTION_DOWN);
  }
}

static void prv_fallback_timer_callback(void *context) {
  if (!s_app.stations.loaded) {
    text_layer_set_text(s_app.main_ui.text_layer, "Failed to fetch stations...");
  }
  s_app.state.fallback_timer = NULL;
}

static uint16_t prv_menu_get_num_sections_callback(MenuLayer *menu_layer, void *context) {
  return 1;
}

static uint16_t prv_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return s_app.stations.loaded ? s_app.stations.count : 1;
}

static int16_t prv_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_menu_draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *context) {
  #ifdef PBL_ROUND
  GRect bounds = layer_get_bounds(cell_layer);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "Nearby", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  #else
  menu_cell_basic_header_draw(ctx, cell_layer, "Nearby");
  #endif
}

static void prv_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  int row = cell_index->row;

  if (s_app.stations.loaded && row < s_app.stations.count) {
    menu_cell_basic_draw(ctx, cell_layer, s_app.stations.names[row], NULL, NULL);
  } else {
    menu_cell_basic_draw(ctx, cell_layer, "Loading...", NULL, NULL);
  }
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  int row = cell_index->row;

  if (!s_app.stations.loaded) { return; }
  s_app.state.last_selected_index = row;
  strncpy(s_app.journey.start_station_name, s_app.stations.names[row], sizeof(s_app.journey.start_station_name) - 1);
  strncpy(s_app.journey.start_station_code, s_app.stations.codes[row], sizeof(s_app.journey.start_station_code) - 1);
  s_app.state.selecting_start_station = false;
  prv_push_dest_menu();
}

static void prv_menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_app.menu_layers.menu_layer = menu_layer_create(bounds);
  menu_layer_set_click_config_onto_window(s_app.menu_layers.menu_layer, window);
  menu_layer_set_callbacks(s_app.menu_layers.menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_sections = prv_menu_get_num_sections_callback,
    .get_num_rows = prv_menu_get_num_rows_callback,
    .get_header_height = prv_menu_get_header_height_callback,
    .draw_header = prv_menu_draw_header_callback,
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
  prv_apply_station_choice(station->code, station->name);
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

static uint16_t prv_dest_menu_get_num_sections_callback(MenuLayer *menu_layer, void *context) {
  return (s_app.favourites.count > 0) ? 3 : 2;
}

static uint16_t prv_dest_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  if (s_app.favourites.count > 0) {
    if (section_index == 0) return s_app.favourites.count;
    if (section_index == 1) return NUM_TOP_STATIONS;
    return ALPHABET_INDEX_COUNT;
  }
  return (section_index == 0) ? NUM_TOP_STATIONS : ALPHABET_INDEX_COUNT;
}

static int16_t prv_dest_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_dest_menu_draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *context) {
  const char *header;
  if (s_app.favourites.count > 0) {
    if (section_index == 0) header = "Favourites";
    else if (section_index == 1) header = "Top Stations";
    else header = "By Letter";
  } else {
    header = (section_index == 0) ? "Top Stations" : "By Letter";
  }
  #ifdef PBL_ROUND
  GRect bounds = layer_get_bounds(cell_layer);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, header, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  #else
  menu_cell_basic_header_draw(ctx, cell_layer, header);
  #endif
}

static void prv_dest_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  int section = cell_index->section;
  int row = cell_index->row;

  if (s_app.favourites.count > 0) {
    if (section == 0) {
      menu_cell_basic_draw(ctx, cell_layer, s_app.favourites.names[row], NULL, NULL);
      return;
    }
    section--;
  }

  if (section == 0) {
    const Station *station = &top_stations[row];
    menu_cell_basic_draw(ctx, cell_layer, station->name, NULL, NULL);
  } else {
    s_app.buffers.letter_str[0] = alphabet_index[row].letter;
    s_app.buffers.letter_str[1] = '\0';
    menu_cell_basic_draw(ctx, cell_layer, s_app.buffers.letter_str, NULL, NULL);
  }
}

static void prv_dest_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  int section = cell_index->section;
  int row = cell_index->row;

  if (s_app.favourites.count > 0) {
    if (section == 0) {
      prv_apply_station_choice(s_app.favourites.codes[row], s_app.favourites.names[row]);
      return;
    }
    section--;
  }

  if (section == 0) {
    const Station *station = &top_stations[row];
    prv_apply_station_choice(station->code, station->name);
  } else {
    s_app.state.selected_alphabet_index = row;
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
    .get_header_height = prv_dest_menu_get_header_height_callback,
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
  Tuple *trip_arrival_epoch_tuple = dict_find(iter, MESSAGE_KEY_TRIP_ARRIVAL_TIME_EPOCH);
  Tuple *trip_transfers_tuple = dict_find(iter, MESSAGE_KEY_TRIP_TRANSFERS);
  Tuple *trip_platform_tuple = dict_find(iter, MESSAGE_KEY_TRIP_PLATFORM);
  Tuple *trip_count_tuple = dict_find(iter, MESSAGE_KEY_TRIP_COUNT);
  Tuple *trip_delay_tuple = dict_find(iter, MESSAGE_KEY_TRIP_DELAY);
  Tuple *error_tuple = dict_find(iter, MESSAGE_KEY_ERROR);
  Tuple *leg_trip_index_tuple = dict_find(iter, MESSAGE_KEY_LEG_TRIP_INDEX);
  Tuple *leg_index_tuple = dict_find(iter, MESSAGE_KEY_LEG_INDEX);
  Tuple *leg_count_tuple = dict_find(iter, MESSAGE_KEY_LEG_COUNT);
  Tuple *leg_departure_station_tuple = dict_find(iter, MESSAGE_KEY_LEG_DEPARTURE_STATION);
  Tuple *leg_departure_platform_tuple = dict_find(iter, MESSAGE_KEY_LEG_DEPARTURE_PLATFORM);
  Tuple *leg_departure_time_tuple = dict_find(iter, MESSAGE_KEY_LEG_DEPARTURE_TIME);
  Tuple *leg_arrival_station_tuple = dict_find(iter, MESSAGE_KEY_LEG_ARRIVAL_STATION);
  Tuple *leg_arrival_time_tuple = dict_find(iter, MESSAGE_KEY_LEG_ARRIVAL_TIME);
  Tuple *leg_duration_tuple = dict_find(iter, MESSAGE_KEY_LEG_DURATION);
  Tuple *favourite_index_tuple = dict_find(iter, MESSAGE_KEY_FAVOURITE_INDEX);
  Tuple *favourite_code_tuple = dict_find(iter, MESSAGE_KEY_FAVOURITE_CODE);
  Tuple *favourite_name_tuple = dict_find(iter, MESSAGE_KEY_FAVOURITE_NAME);
  Tuple *favourite_count_tuple = dict_find(iter, MESSAGE_KEY_FAVOURITE_COUNT);
  Tuple *pin_status_tuple = dict_find(iter, MESSAGE_KEY_PIN_STATUS);
  Tuple *route_duration_tuple = dict_find(iter, MESSAGE_KEY_ROUTE_DURATION);
  Tuple *station_offset_tuple = dict_find(iter, MESSAGE_KEY_STATION_OFFSET);
  Tuple *settings_tijd_tuple = dict_find(iter, MESSAGE_KEY_SETTINGS_TIJD_MODE);
  Tuple *settings_reistijd_tuple = dict_find(iter, MESSAGE_KEY_SETTINGS_REISTIJD);
  Tuple *settings_vervoer_tuple = dict_find(iter, MESSAGE_KEY_SETTINGS_VERVOER);
  
  if (error_tuple) {
    s_app.state.refresh_in_flight = false;
    if (s_app.trips.loaded) {
      return;
    }
    text_layer_set_text(s_app.main_ui.text_layer, "Add API key in settings...");
    return;
  }

  if (station_count_tuple && station_count_tuple->value->int32 <= 0) {
    prv_open_start_station_picker();
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

        // Stop the spinner animation
        if (s_app.state.spinner_timer) {
          app_timer_cancel(s_app.state.spinner_timer);
          s_app.state.spinner_timer = NULL;
        }

        if (s_app.menu_layers.menu_layer) { menu_layer_reload_data(s_app.menu_layers.menu_layer); }

        if (!s_app.windows.menu_window) {
          s_app.windows.menu_window = window_create();
          window_set_window_handlers(s_app.windows.menu_window, (WindowHandlers) { .load = prv_menu_window_load, .unload = prv_menu_window_unload, });
        }
        window_stack_push(s_app.windows.menu_window, true);
      }
    }
  }

  if (trip_index_tuple && trip_departure_time_epoch_tuple && trip_transfers_tuple && trip_count_tuple && trip_platform_tuple && trip_delay_tuple && trip_planned_departure_time_tuple && trip_planned_arrival_time_tuple) {
    int index = trip_index_tuple->value->int32;
    const char *planned_departure_time = trip_planned_departure_time_tuple->value->cstring;
    int departure_time = trip_departure_time_epoch_tuple->value->int32;
    const char *planned_arrival_time = trip_planned_arrival_time_tuple->value->cstring;
    const char *arrival_time = trip_arrival_time_tuple ? trip_arrival_time_tuple->value->cstring : "";
    int count = trip_count_tuple->value->int32;
    int transfers_val = trip_transfers_tuple->value->int32;
    const char *platform = trip_platform_tuple->value->cstring;
    const char *delay = trip_delay_tuple->value->cstring;

    if (index >= 0 && index < MAX_TRIPS) {
      strncpy(s_app.trips.planned_departures[index], planned_departure_time, MAX_DATE_TIME_LENGTH - 1);
      s_app.trips.planned_departures[index][MAX_DATE_TIME_LENGTH - 1] = '\0';
      int prev_dep = s_app.trips.departures[index];
      s_app.trips.departures[index] = departure_time;
      if (s_app.trips.loaded && index == s_app.journey.selected_trip_index &&
          prev_dep != 0 && prev_dep != departure_time) {
        vibes_short_pulse();
      }

      strncpy(s_app.trips.planned_arrivals[index], planned_arrival_time, MAX_DATE_TIME_LENGTH - 1);
      s_app.trips.planned_arrivals[index][MAX_DATE_TIME_LENGTH - 1] = '\0';
      strncpy(s_app.trips.arrivals[index], arrival_time, MAX_DATE_TIME_LENGTH - 1);
      s_app.trips.arrivals[index][MAX_DATE_TIME_LENGTH - 1] = '\0';
      s_app.trips.arrivals_epoch[index] = trip_arrival_epoch_tuple
          ? trip_arrival_epoch_tuple->value->int32 : 0;

      snprintf(s_app.trips.transfers[index], MAX_TRANSFERS_LENGTH, "%d", transfers_val);
      strncpy(s_app.trips.platform[index], platform, MAX_PLATFORM_LENGTH - 1);
      s_app.trips.platform[index][MAX_PLATFORM_LENGTH - 1] = '\0';

      strncpy(s_app.trips.delay[index], delay, MAX_DELAY_LENGTH - 1);
      s_app.trips.delay[index][MAX_DELAY_LENGTH - 1] = '\0';

      if (index + 1 > s_app.trips.count) { s_app.trips.count = index + 1; }
      if (s_app.trips.count >= count) {
        s_app.trips.loaded = true;
        s_app.state.refresh_in_flight = false;
        if (!s_app.windows.countdown_window) {
          s_app.windows.countdown_window = window_create();
          window_set_window_handlers(s_app.windows.countdown_window, (WindowHandlers) {
            .load = prv_countdown_window_load, .unload = prv_countdown_window_unload,
          });
          window_set_click_config_provider(s_app.windows.countdown_window, prv_countdown_click_config_provider);
        }
        if (!window_stack_contains_window(s_app.windows.countdown_window)) {
          window_stack_push(s_app.windows.countdown_window, true);
        } else if (s_app.countdown_ui.vertrek_time_layer) {
          prv_update_countdown_display();
        }
      }
    }
  }

  // Handle leg data messages
  if (leg_trip_index_tuple && leg_index_tuple && leg_count_tuple &&
      leg_departure_station_tuple && leg_departure_platform_tuple &&
      leg_departure_time_tuple && leg_arrival_station_tuple &&
      leg_arrival_time_tuple && leg_duration_tuple) {
    int trip_idx = leg_trip_index_tuple->value->int32;
    int leg_idx = leg_index_tuple->value->int32;
    int leg_count = leg_count_tuple->value->int32;
    const char *departure_station = leg_departure_station_tuple->value->cstring;
    const char *departure_platform = leg_departure_platform_tuple->value->cstring;
    const char *departure_time = leg_departure_time_tuple->value->cstring;
    const char *arrival_station = leg_arrival_station_tuple->value->cstring;
    const char *arrival_time = leg_arrival_time_tuple->value->cstring;
    const char *duration = leg_duration_tuple->value->cstring;

    if (trip_idx >= 0 && trip_idx < MAX_TRIPS && leg_idx >= 0 && leg_idx < MAX_LEGS) {
      strncpy(s_app.trip_legs[trip_idx].legs[leg_idx].departure_station, departure_station, MAX_LEG_STATION_LENGTH - 1);
      s_app.trip_legs[trip_idx].legs[leg_idx].departure_station[MAX_LEG_STATION_LENGTH - 1] = '\0';

      strncpy(s_app.trip_legs[trip_idx].legs[leg_idx].departure_platform, departure_platform, MAX_PLATFORM_LENGTH - 1);
      s_app.trip_legs[trip_idx].legs[leg_idx].departure_platform[MAX_PLATFORM_LENGTH - 1] = '\0';

      strncpy(s_app.trip_legs[trip_idx].legs[leg_idx].departure_time, departure_time, MAX_LEG_TIME_LENGTH - 1);
      s_app.trip_legs[trip_idx].legs[leg_idx].departure_time[MAX_LEG_TIME_LENGTH - 1] = '\0';

      strncpy(s_app.trip_legs[trip_idx].legs[leg_idx].arrival_station, arrival_station, MAX_LEG_STATION_LENGTH - 1);
      s_app.trip_legs[trip_idx].legs[leg_idx].arrival_station[MAX_LEG_STATION_LENGTH - 1] = '\0';

      strncpy(s_app.trip_legs[trip_idx].legs[leg_idx].arrival_time, arrival_time, MAX_LEG_TIME_LENGTH - 1);
      s_app.trip_legs[trip_idx].legs[leg_idx].arrival_time[MAX_LEG_TIME_LENGTH - 1] = '\0';

      strncpy(s_app.trip_legs[trip_idx].legs[leg_idx].duration, duration, MAX_LEG_DURATION_LENGTH - 1);
      s_app.trip_legs[trip_idx].legs[leg_idx].duration[MAX_LEG_DURATION_LENGTH - 1] = '\0';

      Tuple *leg_dep_epoch_tuple = dict_find(iter, MESSAGE_KEY_LEG_DEPARTURE_EPOCH);
      Tuple *leg_arr_epoch_tuple = dict_find(iter, MESSAGE_KEY_LEG_ARRIVAL_EPOCH);
      if (leg_dep_epoch_tuple) {
        s_app.trip_legs[trip_idx].legs[leg_idx].dep_epoch = (uint32_t)leg_dep_epoch_tuple->value->int32;
      }
      if (leg_arr_epoch_tuple) {
        s_app.trip_legs[trip_idx].legs[leg_idx].arr_epoch = (uint32_t)leg_arr_epoch_tuple->value->int32;
      }

      s_app.trip_legs[trip_idx].leg_count = leg_count;
    }
  }

  // Handle favourite station messages
  if (favourite_index_tuple && favourite_code_tuple && favourite_name_tuple && favourite_count_tuple) {
    int index = favourite_index_tuple->value->int32;
    const char *code = favourite_code_tuple->value->cstring;
    const char *name = favourite_name_tuple->value->cstring;
    int count = favourite_count_tuple->value->int32;

    if (index >= 0 && index < MAX_FAVOURITES) {
      strncpy(s_app.favourites.codes[index], code, MAX_STATION_CODE_LENGTH - 1);
      s_app.favourites.codes[index][MAX_STATION_CODE_LENGTH - 1] = '\0';
      strncpy(s_app.favourites.names[index], name, MAX_STATION_NAME_LENGTH - 1);
      s_app.favourites.names[index][MAX_STATION_NAME_LENGTH - 1] = '\0';

      if (index + 1 > s_app.favourites.count) {
        s_app.favourites.count = index + 1;
      }
      if (s_app.favourites.count >= count) {
        s_app.favourites.loaded = true;
        // Reload start menu if it exists to show favourites section
        if (s_app.menu_layers.menu_layer) {
          menu_layer_reload_data(s_app.menu_layers.menu_layer);
        }
      }
    }
  }

  if (pin_status_tuple) {
    if (pin_status_tuple->value->int32 == 1) {
      vibes_short_pulse();
    } else {
      vibes_long_pulse();
    }
  }

  if (station_offset_tuple) {
    s_app.routing.station_offset_min = (int)station_offset_tuple->value->int32;
    s_app.state.refresh_in_flight = false;
  }
  if (route_duration_tuple) {
    int d = (int)route_duration_tuple->value->int32;
    if (d < 0) {
      s_app.routing.at_station = true;
      s_app.routing.have_duration = true;
      s_app.routing.travel_duration_min = 0;
    } else {
      s_app.routing.at_station = false;
      s_app.routing.have_duration = true;
      s_app.routing.travel_duration_min = d;
    }
    s_app.state.refresh_in_flight = false;
    if (s_app.countdown_ui.vertrek_time_layer) {
      prv_update_countdown_display();
    }
  }
  if (settings_tijd_tuple) {
    s_app.settings.tijd_mode = settings_tijd_tuple->value->int32 ? TIJD_MODE_AANKOMST : TIJD_MODE_VERTREK;
  }
  if (settings_reistijd_tuple) {
    s_app.settings.reistijd_enabled = settings_reistijd_tuple->value->int32 != 0;
  }
  if (settings_vervoer_tuple) {
    s_app.settings.vervoer_mode = settings_vervoer_tuple->value->int32 ? VERVOER_FIETS : VERVOER_LOPEN;
  }
}

static void prv_inbox_dropped_handler(AppMessageResult reason, void *context) { APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason); }
static void prv_outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) { APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", (int)reason); }
static void prv_outbox_sent_handler(DictionaryIterator *iter, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success");
  s_pin_queue_sent++;
  if (s_pin_queue_sent < s_pin_queue_count) {
    prv_send_next_pin();
  }
}

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
  const int bar_height = 40;
  #ifdef PBL_COLOR
    s_app.main_ui.bg_blue_layer = layer_create(GRect(0, 0, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.main_ui.bg_blue_layer, prv_bg_blue_update_proc);
    layer_add_child(window_layer, s_app.main_ui.bg_blue_layer);
    s_app.main_ui.bg_blue_bottom_layer = layer_create(GRect(0, bounds.size.h - bar_height, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.main_ui.bg_blue_bottom_layer, prv_bg_blue_update_proc);
    layer_add_child(window_layer, s_app.main_ui.bg_blue_bottom_layer);
    s_app.main_ui.bg_yellow_layer = layer_create(GRect(0, bar_height, bounds.size.w, bounds.size.h - (bar_height * 2)));
    layer_set_update_proc(s_app.main_ui.bg_yellow_layer, prv_bg_yellow_update_proc);
    layer_add_child(window_layer, s_app.main_ui.bg_yellow_layer);
  #else
    s_app.main_ui.bg_blue_layer = layer_create(GRect(0, 0, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.main_ui.bg_blue_layer, prv_bg_black_update_proc);
    layer_add_child(window_layer, s_app.main_ui.bg_blue_layer);
    s_app.main_ui.bg_blue_bottom_layer = layer_create(GRect(0, bounds.size.h - bar_height, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.main_ui.bg_blue_bottom_layer, prv_bg_black_update_proc);
    layer_add_child(window_layer, s_app.main_ui.bg_blue_bottom_layer);
  #endif

  // Create spinner layer centered - taking up half the screen height
  GPoint center = grect_center_point(&bounds);
  int spinner_size = bounds.size.h / 2;
  int spinner_radius = spinner_size / 2;
  s_app.main_ui.spinner_layer = layer_create(GRect(center.x - spinner_radius, center.y - spinner_radius, spinner_size, spinner_size));
  layer_set_update_proc(s_app.main_ui.spinner_layer, prv_spinner_layer_update_proc);
  layer_add_child(window_layer, s_app.main_ui.spinner_layer);

  // Create text layer in the bottom bar with white text
  s_app.main_ui.text_layer = text_layer_create(GRect(0, bounds.size.h - bar_height, bounds.size.w, bar_height));
  text_layer_set_text_alignment(s_app.main_ui.text_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_app.main_ui.text_layer, GColorClear);
  text_layer_set_text_color(s_app.main_ui.text_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(s_app.main_ui.text_layer));

  // Start the spinner animation
  s_app.state.spinner_angle = 0;
  s_app.state.spinner_timer = app_timer_register(50, prv_spinner_timer_callback, NULL);

  prv_request_stations_from_phone();
}

static void prv_window_unload(Window *window) {
  // Stop spinner animation
  if (s_app.state.spinner_timer) {
    app_timer_cancel(s_app.state.spinner_timer);
    s_app.state.spinner_timer = NULL;
  }

  if (s_app.main_ui.spinner_layer) {
    layer_destroy(s_app.main_ui.spinner_layer);
  }

  text_layer_destroy(s_app.main_ui.text_layer);
  layer_destroy(s_app.main_ui.bg_blue_layer);
  layer_destroy(s_app.main_ui.bg_blue_bottom_layer);
  #ifdef PBL_COLOR
    layer_destroy(s_app.main_ui.bg_yellow_layer);
  #endif
}

static void prv_init(void) {
  // Initialize all app data to zero
  memset(&s_app, 0, sizeof(AppData));
  s_app.buffers.letter_str[0] = 'A';
  s_app.buffers.letter_str[1] = '\0';
  s_app.settings.reistijd_enabled = true;
#ifdef PBL_COLOR
  s_mid_band_color = GColorYellow;
#endif
  if (persist_exists(PERSIST_TIJD)) {
    s_app.settings.tijd_mode = persist_read_int(PERSIST_TIJD) ? TIJD_MODE_AANKOMST : TIJD_MODE_VERTREK;
  }
  if (persist_exists(PERSIST_REISTIJD)) {
    s_app.settings.reistijd_enabled = persist_read_bool(PERSIST_REISTIJD);
  }
  if (persist_exists(PERSIST_VERVOER)) {
    s_app.settings.vervoer_mode = persist_read_int(PERSIST_VERVOER) ? VERVOER_FIETS : VERVOER_LOPEN;
  }

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
  if(s_app.windows.countdown_window) window_destroy(s_app.windows.countdown_window);
  if(s_app.windows.journey_details_window) window_destroy(s_app.windows.journey_details_window);
  if(s_app.windows.settings_window) window_destroy(s_app.windows.settings_window);
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

// --- Journey Details Window ---
static uint16_t prv_journey_details_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  int trip_idx = s_app.journey.selected_trip_index;
  return s_app.trip_legs[trip_idx].leg_count;
}

static int16_t prv_journey_details_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  return 52;
}

static void prv_journey_details_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  int trip_idx = s_app.journey.selected_trip_index;
  int leg_idx = cell_index->row;
  LegData *leg = &s_app.trip_legs[trip_idx].legs[leg_idx];

  GRect bounds = layer_get_bounds(cell_layer);

  // Check if this row is selected
  MenuIndex selected = menu_layer_get_selected_index(s_app.journey_details_ui.menu_layer);
  bool is_selected = (selected.row == cell_index->row && selected.section == cell_index->section);

  // Set text colors based on selection state
  GColor main_text_color = is_selected ? GColorWhite : GColorBlack;
  GColor secondary_text_color = is_selected ? GColorWhite : GColorDarkGray;

  graphics_context_set_text_color(ctx, main_text_color);

  // Draw departure time
  graphics_draw_text(ctx, leg->departure_time, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(4, 2, 40, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  // Draw departure station
  graphics_draw_text(ctx, leg->departure_station, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(46, 2, bounds.size.w - 82, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  // Draw platform box
  int platform_x = bounds.size.w - 32;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(platform_x, 4, 24, 18), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorOxfordBlue);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, GRect(platform_x, 4, 24, 18));

  // Draw small blue square in platform box
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, GRect(platform_x + 1, 5, 5, 5), 0, GCornerNone);

  // Draw platform number (always blue on white background)
  graphics_context_set_text_color(ctx, GColorOxfordBlue);
  graphics_draw_text(ctx, leg->departure_platform, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(platform_x, 4, 24, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // Draw arrival time
  graphics_context_set_text_color(ctx, secondary_text_color);
  graphics_draw_text(ctx, leg->arrival_time, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(4, 28, 40, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  // Draw arrival station
  graphics_draw_text(ctx, leg->arrival_station, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(46, 28, bounds.size.w - 82, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  // Draw duration below platform (aligned with left edge of platform box)
  graphics_draw_text(ctx, leg->duration, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(platform_x, 26, 40, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void prv_send_pin_request(int trip_index, int leg_index, bool is_last) {
  LegData *leg = &s_app.trip_legs[trip_index].legs[leg_index];
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    int one = 1;
    int is_last_int = is_last ? 1 : 0;
    dict_write_int(iter, MESSAGE_KEY_REQUEST_PIN, &one, sizeof(int), true);
    dict_write_int(iter, MESSAGE_KEY_PIN_TRIP_INDEX, &trip_index, sizeof(int), true);
    dict_write_int(iter, MESSAGE_KEY_PIN_LEG_INDEX, &leg_index, sizeof(int), true);
    dict_write_int(iter, MESSAGE_KEY_PIN_IS_LAST, &is_last_int, sizeof(int), true);
    dict_write_uint32(iter, MESSAGE_KEY_PIN_DEP_EPOCH, leg->dep_epoch);
    dict_write_uint32(iter, MESSAGE_KEY_PIN_ARR_EPOCH, leg->arr_epoch);
    dict_write_cstring(iter, MESSAGE_KEY_PIN_DEP_STATION, leg->departure_station);
    dict_write_cstring(iter, MESSAGE_KEY_PIN_ARR_STATION, leg->arrival_station);
    dict_write_cstring(iter, MESSAGE_KEY_PIN_PLATFORM, leg->departure_platform);
    app_message_outbox_send();
  }
}

static void prv_send_next_pin(void) {
  if (s_pin_queue_sent < s_pin_queue_count) {
    PinQueueItem *item = &s_pin_queue[s_pin_queue_sent];
    prv_send_pin_request(item->trip_index, item->leg_index, item->is_last);
  }
}

static void prv_pin_leg_callback(int index, void *context) {
  s_pin_queue_count = 1;
  s_pin_queue_sent = 0;
  s_pin_queue[0] = (PinQueueItem){
    .trip_index = s_app.journey.selected_trip_index,
    .leg_index = s_pin_selected_leg_index,
    .is_last = true
  };
  prv_send_next_pin();
}

static void prv_pin_journey_callback(int index, void *context) {
  int trip_idx = s_app.journey.selected_trip_index;
  int leg_count = s_app.trip_legs[trip_idx].leg_count;
  s_pin_queue_count = leg_count;
  s_pin_queue_sent = 0;
  for (int i = 0; i < leg_count; i++) {
    s_pin_queue[i] = (PinQueueItem){
      .trip_index = trip_idx,
      .leg_index = i,
      .is_last = (i == leg_count - 1)
    };
  }
  prv_send_next_pin();
}

static void prv_pin_exit_callback(int index, void *context) {
  window_stack_pop_all(true);
}

static void prv_pin_menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_pin_menu_items[0] = (SimpleMenuItem) { .title = "Pin this leg", .callback = prv_pin_leg_callback };
  s_pin_menu_items[1] = (SimpleMenuItem) { .title = "Pin journey", .callback = prv_pin_journey_callback };
  s_pin_menu_items[2] = (SimpleMenuItem) { .title = "Exit app", .callback = prv_pin_exit_callback };
  s_pin_menu_sections[0] = (SimpleMenuSection) { .num_items = 3, .items = s_pin_menu_items };

  s_pin_simple_menu_layer = simple_menu_layer_create(bounds, window, s_pin_menu_sections, 1, NULL);

  #ifdef PBL_COLOR
  MenuLayer *pin_ml = simple_menu_layer_get_menu_layer(s_pin_simple_menu_layer);
  menu_layer_set_normal_colors(pin_ml, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(pin_ml, GColorOxfordBlue, GColorWhite);
  #endif

  layer_add_child(window_layer, simple_menu_layer_get_layer(s_pin_simple_menu_layer));
}

static void prv_pin_menu_window_unload(Window *window) {
  simple_menu_layer_destroy(s_pin_simple_menu_layer);
  s_pin_simple_menu_layer = NULL;
  window_destroy(s_app.windows.pin_menu_window);
  s_app.windows.pin_menu_window = NULL;
}

static void prv_journey_details_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  s_pin_selected_leg_index = cell_index->row;
  s_app.windows.pin_menu_window = window_create();
  window_set_window_handlers(s_app.windows.pin_menu_window, (WindowHandlers){
    .load = prv_pin_menu_window_load,
    .unload = prv_pin_menu_window_unload,
  });
  window_stack_push(s_app.windows.pin_menu_window, true);
}

static void prv_journey_details_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  const int bar_height = 30;

  #ifdef PBL_COLOR
    s_app.journey_details_ui.bg_blue_layer = layer_create(GRect(0, 0, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.journey_details_ui.bg_blue_layer, prv_bg_blue_update_proc);
    layer_add_child(window_layer, s_app.journey_details_ui.bg_blue_layer);
    s_app.journey_details_ui.bg_yellow_layer = layer_create(GRect(0, bar_height, bounds.size.w, bounds.size.h - bar_height));
    layer_set_update_proc(s_app.journey_details_ui.bg_yellow_layer, prv_bg_yellow_update_proc);
    layer_add_child(window_layer, s_app.journey_details_ui.bg_yellow_layer);
  #else
    s_app.journey_details_ui.bg_blue_layer = layer_create(GRect(0, 0, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.journey_details_ui.bg_blue_layer, prv_bg_black_update_proc);
    layer_add_child(window_layer, s_app.journey_details_ui.bg_blue_layer);
  #endif

  // Create menu layer for scrollable leg list
  GRect menu_bounds = GRect(0, bar_height, bounds.size.w, bounds.size.h - bar_height);
  s_app.journey_details_ui.menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_click_config_onto_window(s_app.journey_details_ui.menu_layer, window);
  menu_layer_set_callbacks(s_app.journey_details_ui.menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_journey_details_get_num_rows_callback,
    .get_cell_height = prv_journey_details_get_cell_height_callback,
    .draw_row = prv_journey_details_draw_row_callback,
    .select_click = prv_journey_details_select_callback,
  });
  #ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_app.journey_details_ui.menu_layer, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(s_app.journey_details_ui.menu_layer, GColorOxfordBlue, GColorWhite);
  #endif
  layer_add_child(window_layer, menu_layer_get_layer(s_app.journey_details_ui.menu_layer));
}

static void prv_journey_details_window_unload(Window *window) {
  menu_layer_destroy(s_app.journey_details_ui.menu_layer);
  layer_destroy(s_app.journey_details_ui.bg_blue_layer);
  #ifdef PBL_COLOR
    layer_destroy(s_app.journey_details_ui.bg_yellow_layer);
  #endif
}

static void prv_countdown_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_stamp_button();
  // Push journey details window instead of popping all
  if (!s_app.windows.journey_details_window) {
    s_app.windows.journey_details_window = window_create();
    window_set_window_handlers(s_app.windows.journey_details_window, (WindowHandlers) {
      .load = prv_journey_details_window_load, .unload = prv_journey_details_window_unload,
    });
  }
  window_stack_push(s_app.windows.journey_details_window, true);
}

static void prv_countdown_select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_stamp_button();
  if (!s_app.windows.settings_window) {
    s_app.windows.settings_window = window_create();
    window_set_window_handlers(s_app.windows.settings_window, (WindowHandlers) {
      .load = prv_settings_window_load, .unload = prv_settings_window_unload,
    });
  }
  window_stack_push(s_app.windows.settings_window, true);
}

static void prv_countdown_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_countdown_select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, prv_countdown_select_long_click_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_UP, prv_countdown_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_countdown_down_click_handler);
}

static void prv_settings_rebuild_items(void) {
  strncpy(s_settings_sub[0], s_app.settings.tijd_mode == TIJD_MODE_AANKOMST ? "Aankomst" : "Vertrek", 19);
  strncpy(s_settings_sub[1], s_app.settings.reistijd_enabled ? "Aan" : "Uit", 19);
  strncpy(s_settings_sub[2], s_app.settings.vervoer_mode == VERVOER_FIETS ? "Fiets" : "Lopen", 19);
  s_settings_items[0].title = "Tijd";
  s_settings_items[0].subtitle = s_settings_sub[0];
  s_settings_items[1].title = "Reistijd";
  s_settings_items[1].subtitle = s_settings_sub[1];
  s_settings_items[2].title = "Vervoer";
  s_settings_items[2].subtitle = s_settings_sub[2];
}

static void prv_settings_select_callback(int index, void *context) {
  if (index == 0) {
    s_app.settings.tijd_mode = (s_app.settings.tijd_mode == TIJD_MODE_VERTREK) ? TIJD_MODE_AANKOMST : TIJD_MODE_VERTREK;
  } else if (index == 1) {
    s_app.settings.reistijd_enabled = !s_app.settings.reistijd_enabled;
  } else {
    s_app.settings.vervoer_mode = (s_app.settings.vervoer_mode == VERVOER_LOPEN) ? VERVOER_FIETS : VERVOER_LOPEN;
  }
  prv_persist_settings();
  prv_send_settings_to_phone();
  prv_settings_rebuild_items();
  s_settings_sections[0] = (SimpleMenuSection){ .num_items = 3, .items = s_settings_items };
  if (s_settings_menu_layer) {
    menu_layer_reload_data(simple_menu_layer_get_menu_layer(s_settings_menu_layer));
  }
}

static void prv_settings_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  prv_settings_rebuild_items();
  s_settings_items[0].callback = prv_settings_select_callback;
  s_settings_items[1].callback = prv_settings_select_callback;
  s_settings_items[2].callback = prv_settings_select_callback;
  s_settings_sections[0] = (SimpleMenuSection){ .num_items = 3, .items = s_settings_items };
  s_settings_menu_layer = simple_menu_layer_create(bounds, window, s_settings_sections, 1, NULL);
#ifdef PBL_COLOR
  MenuLayer *ml = simple_menu_layer_get_menu_layer(s_settings_menu_layer);
  menu_layer_set_normal_colors(ml, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(ml, GColorOxfordBlue, GColorWhite);
#endif
  layer_add_child(window_layer, simple_menu_layer_get_layer(s_settings_menu_layer));
}

static void prv_settings_window_unload(Window *window) {
  simple_menu_layer_destroy(s_settings_menu_layer);
  s_settings_menu_layer = NULL;
  window_destroy(s_app.windows.settings_window);
  s_app.windows.settings_window = NULL;
  if (s_app.countdown_ui.vertrek_time_layer) {
    prv_update_countdown_display();
  }
}

 static void prv_countdown_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  const int bar_height = 40;
  s_app.journey.selected_trip_index = 0;

  #ifdef PBL_COLOR
    s_app.countdown_ui.bg_blue_layer = layer_create(GRect(0, 0, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.countdown_ui.bg_blue_layer, prv_bg_blue_update_proc);
    layer_add_child(window_layer, s_app.countdown_ui.bg_blue_layer);
    s_app.countdown_ui.bg_blue_bottom_layer = layer_create(GRect(0, bounds.size.h - bar_height, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.countdown_ui.bg_blue_bottom_layer, prv_bg_blue_update_proc);
    layer_add_child(window_layer, s_app.countdown_ui.bg_blue_bottom_layer);
    s_app.countdown_ui.bg_yellow_layer = layer_create(GRect(0, bar_height, bounds.size.w, bounds.size.h - (bar_height * 2)));
    layer_set_update_proc(s_app.countdown_ui.bg_yellow_layer, prv_bg_yellow_update_proc);
    layer_add_child(window_layer, s_app.countdown_ui.bg_yellow_layer);
  #else
    s_app.countdown_ui.bg_blue_layer = layer_create(GRect(0, 0, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.countdown_ui.bg_blue_layer, prv_bg_black_update_proc);
    layer_add_child(window_layer, s_app.countdown_ui.bg_blue_layer);
    s_app.countdown_ui.bg_blue_bottom_layer = layer_create(GRect(0, bounds.size.h - bar_height, bounds.size.w, bar_height));
    layer_set_update_proc(s_app.countdown_ui.bg_blue_bottom_layer, prv_bg_black_update_proc);
    layer_add_child(window_layer, s_app.countdown_ui.bg_blue_bottom_layer);
  #endif

  s_app.countdown_ui.trip_leg_layer = layer_create(bounds);
  layer_set_update_proc(s_app.countdown_ui.trip_leg_layer, prv_trip_leg_layer_update_proc);
  layer_add_child(window_layer, s_app.countdown_ui.trip_leg_layer);

  s_app.countdown_ui.start_station_layer = text_layer_create(GRect(0, 10, bounds.size.w, 30));
  text_layer_set_text(s_app.countdown_ui.start_station_layer, s_app.journey.start_station_name);
  text_layer_set_font(s_app.countdown_ui.start_station_layer, fonts_get_system_font((bounds.size.w == 200) ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_app.countdown_ui.start_station_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_app.countdown_ui.start_station_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.start_station_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.start_station_layer));

  s_app.countdown_ui.destination_layer = text_layer_create(GRect(0, bounds.size.h - bar_height, bounds.size.w, 30));
  text_layer_set_text(s_app.countdown_ui.destination_layer, s_app.journey.dest_station_name);
  text_layer_set_font(s_app.countdown_ui.destination_layer, fonts_get_system_font((bounds.size.w == 200) ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_app.countdown_ui.destination_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_app.countdown_ui.destination_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.destination_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.destination_layer));

  const int platform_y  = PBL_IF_ROUND_ELSE(40, 35);
  // Center the countdown in the middle of the screen (between top and bottom bars)
  const int content_height = bounds.size.h - (bar_height * 2); // Height between bars
  const int countdown_y = bar_height + (content_height / 2) - 25; // Center countdown
  const int delay_y = bounds.size.h - bar_height - 35; // Close to bottom bar

  // Scale positions for larger displays (emery has 200px width vs 144px on other rect displays)
  // Note: Pebble Round is excluded as PBL_IF_ROUND_ELSE handles it separately
  const int x_offset = PBL_IF_ROUND_ELSE(17, (bounds.size.w == 200) ? 33 : 5);
  const int platform_x_offset = PBL_IF_ROUND_ELSE(0, (bounds.size.w == 200) ? 40 : 0);
  #ifdef PBL_ROUND
  const bool is_large_display = false;
  #else
  const bool is_large_display = (bounds.size.w == 200);
  #endif

  s_app.countdown_ui.departure_time_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(17, platform_y + 4, 30, 20), GRect(x_offset, platform_y + 2, is_large_display ? 40 : 30, 20)));
  text_layer_set_font(s_app.countdown_ui.departure_time_layer, fonts_get_system_font(is_large_display ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_app.countdown_ui.departure_time_layer, GTextAlignmentLeft);
  text_layer_set_background_color(s_app.countdown_ui.departure_time_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.departure_time_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.departure_time_layer));

  s_app.countdown_ui.time_arrow_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(47, platform_y + 4, 15, 20), GRect(x_offset + (is_large_display ? 40 : 30), platform_y + 2, is_large_display ? 20 : 15, 20)));
  text_layer_set_font(s_app.countdown_ui.time_arrow_layer, fonts_get_system_font(is_large_display ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_app.countdown_ui.time_arrow_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_app.countdown_ui.time_arrow_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.time_arrow_layer, GColorBlack);
  text_layer_set_text(s_app.countdown_ui.time_arrow_layer, ">");
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.time_arrow_layer));

  s_app.countdown_ui.arrival_time_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(62, platform_y + 4, 30, 20), GRect(x_offset + (is_large_display ? 60 : 45), platform_y + 2, is_large_display ? 40 : 30, 20)));
  text_layer_set_font(s_app.countdown_ui.arrival_time_layer, fonts_get_system_font(is_large_display ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_app.countdown_ui.arrival_time_layer, GTextAlignmentLeft);
  text_layer_set_background_color(s_app.countdown_ui.arrival_time_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.arrival_time_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.arrival_time_layer));

  const int platform_size = is_large_display ? 32 : 24;
  s_app.countdown_ui.platform_border_layer = layer_create(PBL_IF_ROUND_ELSE(GRect(118, platform_y + 2, 24, 24), GRect(90 + platform_x_offset, platform_y + 6, platform_size, platform_size)));
  layer_set_update_proc(s_app.countdown_ui.platform_border_layer, prv_platform_border_update_proc);
  layer_add_child(window_layer, s_app.countdown_ui.platform_border_layer);

  s_app.countdown_ui.platform_number_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(120, platform_y + 4, 20, 24), GRect(92 + platform_x_offset, platform_y + 8, is_large_display ? 28 : 20, is_large_display ? 32 : 24)));
  text_layer_set_font(s_app.countdown_ui.platform_number_layer, fonts_get_system_font(is_large_display ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_app.countdown_ui.platform_number_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_app.countdown_ui.platform_number_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.platform_number_layer, GColorOxfordBlue);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.platform_number_layer));

  s_app.countdown_ui.delay_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(0, delay_y, bounds.size.w, 30), GRect(x_offset, delay_y - 2, bounds.size.w - x_offset - 5, is_large_display ? 40 : 30)));
  text_layer_set_font(s_app.countdown_ui.delay_layer, fonts_get_system_font(is_large_display ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_app.countdown_ui.delay_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_background_color(s_app.countdown_ui.delay_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.delay_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.delay_layer));

  s_app.countdown_ui.over_label_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(0, countdown_y - 16, bounds.size.w, 18), GRect(x_offset, countdown_y - 16, bounds.size.w - x_offset - 5, 18)));
  text_layer_set_text(s_app.countdown_ui.over_label_layer, "OVER");
  text_layer_set_font(s_app.countdown_ui.over_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_app.countdown_ui.over_label_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_background_color(s_app.countdown_ui.over_label_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.over_label_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.over_label_layer));

  s_app.countdown_ui.over_time_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(0, countdown_y, bounds.size.w, 50), GRect(x_offset, countdown_y - 2, bounds.size.w - x_offset - 5, is_large_display ? 60 : 50)));
  text_layer_set_text(s_app.countdown_ui.over_time_layer, "...");
  text_layer_set_font(s_app.countdown_ui.over_time_layer, fonts_get_system_font(is_large_display ? FONT_KEY_LECO_42_NUMBERS : FONT_KEY_LECO_36_BOLD_NUMBERS));
  text_layer_set_text_alignment(s_app.countdown_ui.over_time_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_background_color(s_app.countdown_ui.over_time_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.over_time_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.over_time_layer));

  s_app.countdown_ui.vertrek_label_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(0, delay_y - 36, bounds.size.w, 16), GRect(x_offset, delay_y - 36, 70, 16)));
  text_layer_set_text(s_app.countdown_ui.vertrek_label_layer, "VERTREK");
  text_layer_set_font(s_app.countdown_ui.vertrek_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_app.countdown_ui.vertrek_label_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_background_color(s_app.countdown_ui.vertrek_label_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.vertrek_label_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.vertrek_label_layer));

  s_app.countdown_ui.vertrek_time_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(0, delay_y - 22, bounds.size.w, 22), GRect(x_offset + 70, delay_y - 36, bounds.size.w - x_offset - 75, 22)));
  text_layer_set_font(s_app.countdown_ui.vertrek_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_app.countdown_ui.vertrek_time_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_background_color(s_app.countdown_ui.vertrek_time_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.vertrek_time_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.vertrek_time_layer));

  s_app.countdown_ui.clock_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(0, 0, bounds.size.w, 16), GRect(0, 0, bounds.size.w, is_large_display ? 20 : 16)));
  text_layer_set_font(s_app.countdown_ui.clock_layer, fonts_get_system_font(is_large_display ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_app.countdown_ui.clock_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_app.countdown_ui.clock_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.clock_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.clock_layer));

#ifdef PBL_COLOR
  s_mid_band_color = GColorYellow;
#endif
  if (s_app.state.refresh_timer) {
    app_timer_cancel(s_app.state.refresh_timer);
  }
  s_app.state.refresh_in_flight = false;
  s_app.state.refresh_timer = app_timer_register(30000, prv_refresh_timer_callback, NULL);
  prv_send_route_request();

  prv_clock_timer_callback(NULL);

  prv_update_countdown_display();
}

static void prv_countdown_window_unload(Window *window) {
  if (s_app.state.countdown_timer) {
    app_timer_cancel(s_app.state.countdown_timer);
    s_app.state.countdown_timer = NULL;
  }

  if (s_app.state.clock_timer) {
    app_timer_cancel(s_app.state.clock_timer);
    s_app.state.clock_timer = NULL;
  }
  if (s_app.state.refresh_timer) {
    app_timer_cancel(s_app.state.refresh_timer);
    s_app.state.refresh_timer = NULL;
  }
  s_app.state.refresh_in_flight = false;

  // Unschedule animation if running, but don't destroy (animations auto-free when complete)
  if (s_app.state.content_animation) {
    animation_unschedule((Animation*)s_app.state.content_animation);
    property_animation_destroy(s_app.state.content_animation);
    s_app.state.content_animation = NULL;
  }
  s_app.state.is_animating = false;

  text_layer_destroy(s_app.countdown_ui.destination_layer);
  text_layer_destroy(s_app.countdown_ui.start_station_layer);
  layer_destroy(s_app.countdown_ui.platform_border_layer);
  text_layer_destroy(s_app.countdown_ui.platform_number_layer);
  text_layer_destroy(s_app.countdown_ui.over_label_layer);
  text_layer_destroy(s_app.countdown_ui.over_time_layer);
  text_layer_destroy(s_app.countdown_ui.vertrek_label_layer);
  text_layer_destroy(s_app.countdown_ui.vertrek_time_layer);
  s_app.countdown_ui.over_label_layer = NULL;
  s_app.countdown_ui.over_time_layer = NULL;
  s_app.countdown_ui.vertrek_label_layer = NULL;
  s_app.countdown_ui.vertrek_time_layer = NULL;
  text_layer_destroy(s_app.countdown_ui.clock_layer);
  text_layer_destroy(s_app.countdown_ui.departure_time_layer);
  text_layer_destroy(s_app.countdown_ui.time_arrow_layer);
  text_layer_destroy(s_app.countdown_ui.arrival_time_layer);
  text_layer_destroy(s_app.countdown_ui.delay_layer);

  layer_destroy(s_app.countdown_ui.trip_leg_layer);

  layer_destroy(s_app.countdown_ui.bg_blue_layer);
  layer_destroy(s_app.countdown_ui.bg_blue_bottom_layer);
  #ifdef PBL_COLOR
    layer_destroy(s_app.countdown_ui.bg_yellow_layer);
  #endif
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}