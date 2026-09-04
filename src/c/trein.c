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
static void prv_push_trips_loading(void);
static void prv_dismiss_trips_loading(void);
static void prv_destroy_loading_ui(void);
static void prv_dest_menu_attach(Window *window);
static void prv_restore_dest_menu(void);
static void prv_loading_fail_timeout(void *data);
static void prv_loading_show_timeout(void *data);
static void prv_arm_loading_fail(void);
static void prv_pop_alpha_timeout(void *data);
static void prv_pop_stations_timeout(void *data);
static void prv_destroy_dest_menu_layer(void);
static void prv_noop_click_config(void *context);
static void prv_present_countdown(void);
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

static MenuLayer *s_settings_menu_layer;
static char s_settings_sub[4][24];

// --- Pin Queue ---
#define MAX_PIN_QUEUE 4
typedef struct { int trip_index; int leg_index; bool is_last; } PinQueueItem;
static PinQueueItem s_pin_queue[MAX_PIN_QUEUE];
static int s_pin_queue_count = 0;
static int s_pin_queue_sent = 0;
static void prv_send_next_pin(void);
static AppTimer *s_pop_alpha_timer;
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

  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorPictonBlue);
  #else
  graphics_context_set_fill_color(ctx, GColorWhite);
  #endif
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_rect(ctx, bounds);
}

// Draw rotating arc spinner (white on OxfordBlue background)
static void prv_spinner_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  
  const int arc_radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 4;
  const int arc_thickness = 3;
  
  // Draw rotating arc (270 degrees, rotates with spinner_angle)
  int32_t start_angle = TRIG_MAX_ANGLE * s_app.state.spinner_angle / 360;
  int32_t end_angle = start_angle + (TRIG_MAX_ANGLE * 270 / 360);
  
  #ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorWhite);
  #else
  graphics_context_set_stroke_color(ctx, GColorBlack);
  #endif
  graphics_context_set_stroke_width(ctx, arc_thickness);
  graphics_draw_arc(ctx, GRect(center.x - arc_radius, center.y - arc_radius, 
                                arc_radius * 2, arc_radius * 2),
                    GOvalScaleModeFitCircle, start_angle, end_angle);
}

// Draw full-screen OxfordBlue overlay background
static void prv_loading_bg_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  #else
  graphics_context_set_fill_color(ctx, GColorBlack);
  #endif
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

// Spinner animation timer callback
static void prv_noop_click_config(void *context) {
  (void)context;
}

static void prv_spinner_timer_callback(void *data) {
  (void)data;
  s_app.state.spinner_angle = (s_app.state.spinner_angle + 6) % 360;
  bool keep = false;
  if (s_app.main_ui.spinner_layer) {
    layer_mark_dirty(s_app.main_ui.spinner_layer);
    keep = true;
  }
  /* Never mark_dirty a spinner after loading_ui was torn down. */
  if (s_app.loading_ui.spinner_layer) {
    layer_mark_dirty(s_app.loading_ui.spinner_layer);
    keep = true;
  }
  if (keep) {
    s_app.state.spinner_timer = app_timer_register(50, prv_spinner_timer_callback, NULL);
  } else {
    s_app.state.spinner_timer = NULL;
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
  if (!code) code = "";
  if (!name) name = "";
  if (s_app.state.selecting_start_station) {
    strncpy(s_app.journey.start_station_code, code, sizeof(s_app.journey.start_station_code) - 1);
    s_app.journey.start_station_code[sizeof(s_app.journey.start_station_code) - 1] = '\0';
    strncpy(s_app.journey.start_station_name, name, sizeof(s_app.journey.start_station_name) - 1);
    s_app.journey.start_station_name[sizeof(s_app.journey.start_station_name) - 1] = '\0';
    s_app.state.selecting_start_station = false;
    if (s_app.windows.alpha_menu_window &&
        window_stack_contains_window(s_app.windows.alpha_menu_window)) {
      if (s_pop_alpha_timer) app_timer_cancel(s_pop_alpha_timer);
      s_pop_alpha_timer = app_timer_register(10, prv_pop_alpha_timeout, NULL);
    }
    if (s_app.menu_layers.dest_menu_layer) {
      menu_layer_reload_data(s_app.menu_layers.dest_menu_layer);
    }
    return;
  }
  strncpy(s_app.journey.dest_station_code, code, sizeof(s_app.journey.dest_station_code) - 1);
  s_app.journey.dest_station_code[sizeof(s_app.journey.dest_station_code) - 1] = '\0';
  strncpy(s_app.journey.dest_station_name, name, sizeof(s_app.journey.dest_station_name) - 1);
  s_app.journey.dest_station_name[sizeof(s_app.journey.dest_station_name) - 1] = '\0';
  s_app.trips.loaded = false;
  s_app.trips.count = 0;
  s_app.trips.received_mask = 0;
  s_app.routing.have_duration = false;
  s_app.routing.at_station = false;
  s_app.routing.travel_duration_min = 0;
  s_app.routing.route_error = 0;
  /* Do not destroy dest_menu_layer here (still inside MenuLayer select). */
  prv_send_trip_request();
  if (s_app.state.loading_show_timer) {
    app_timer_cancel(s_app.state.loading_show_timer);
  }
  s_app.state.loading_show_timer = app_timer_register(10, prv_loading_show_timeout, NULL);
}

static AppTimer *s_route_retry_timer;
static int s_route_retry_left;

static void prv_route_retry_cb(void *data) {
  (void)data;
  s_route_retry_timer = NULL;
  prv_send_route_request();
}

static void prv_write_route_payload(DictionaryIterator *iter) {
  dict_write_uint8(iter, MESSAGE_KEY_REQUEST_ROUTE, 1);
  dict_write_int32(iter, MESSAGE_KEY_ROUTE_MODE, (int32_t)s_app.settings.vervoer_mode);
  dict_write_int32(iter, MESSAGE_KEY_SETTINGS_TIJD_MODE, (int32_t)s_app.settings.tijd_mode);
  dict_write_int32(iter, MESSAGE_KEY_SETTINGS_REISTIJD, s_app.settings.reistijd_enabled ? 1 : 0);
  dict_write_int32(iter, MESSAGE_KEY_SETTINGS_VERVOER, (int32_t)s_app.settings.vervoer_mode);
}

static void prv_send_route_request(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    if (!s_route_retry_timer && s_route_retry_left > 0) {
      s_route_retry_left--;
      s_route_retry_timer = app_timer_register(250, prv_route_retry_cb, NULL);
    }
    return;
  }
  s_route_retry_left = 4;
  prv_write_route_payload(iter);
  app_message_outbox_send();
}

static void prv_send_settings_to_phone(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    if (!s_route_retry_timer && s_route_retry_left > 0) {
      s_route_retry_left--;
      s_route_retry_timer = app_timer_register(250, prv_route_retry_cb, NULL);
    }
    return;
  }
  s_route_retry_left = 4;
  prv_write_route_payload(iter);
  app_message_outbox_send();
}

static void prv_send_offset_to_phone(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_cstring(iter, MESSAGE_KEY_START_STATION_CODE, s_app.journey.start_station_code);
  dict_write_int32(iter, MESSAGE_KEY_STATION_OFFSET, s_app.routing.station_offset_min);
  app_message_outbox_send();
}

static void prv_add_favourite(const char *code, const char *name) {
  if (!code || !code[0]) return;
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_uint8(iter, MESSAGE_KEY_REQUEST_FAVOURITE, 1);
  dict_write_cstring(iter, MESSAGE_KEY_FAVOURITE_CODE, code);
  dict_write_cstring(iter, MESSAGE_KEY_FAVOURITE_NAME, name ? name : code);
  app_message_outbox_send();
  vibes_short_pulse();
}

static int prv_clamp_offset(int v) {
  if (v < -15) return -15;
  if (v > 30) return 30;
  return v;
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

static bool prv_is_large_display(GRect bounds) {
#ifdef PBL_ROUND
  (void)bounds;
  return false;
#else
#ifdef PBL_PLATFORM_FLINT
  (void)bounds;
  /* Time 2 / flint: always large type, even when qemu reports 144x168. */
  return true;
#else
  return (bounds.size.w >= 200 || bounds.size.h >= 200);
#endif
#endif
}

static bool prv_countdown_is_large(void) {
  if (!s_app.windows.countdown_window) return false;
  return prv_is_large_display(layer_get_bounds(window_get_root_layer(s_app.windows.countdown_window)));
}

static bool prv_leco_safe(const char *s) {
  if (!s || !*s) return false;
  for (; *s; s++) {
    if (*s != ':' && (*s < '0' || *s > '9')) return false;
  }
  return true;
}

static GFont prv_over_numeric_font(void) {
  return fonts_get_system_font(prv_countdown_is_large() ? FONT_KEY_LECO_42_NUMBERS : FONT_KEY_LECO_20_BOLD_NUMBERS);
}

static GFont prv_vertrek_numeric_font(void) {
  /* Emery/Time 2: VERTREK smaller than OVER, fully contained in cream */
  return fonts_get_system_font(prv_countdown_is_large() ? FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM : FONT_KEY_LECO_20_BOLD_NUMBERS);
}

static GFont prv_over_text_font(void) {
  /* For negative OVER (LECO has no minus): use ROBOTO_BOLD_SUBSET_49 on large displays */
  return fonts_get_system_font(prv_countdown_is_large() ? FONT_KEY_ROBOTO_BOLD_SUBSET_49 : FONT_KEY_GOTHIC_24_BOLD);
}

static void prv_set_sized_clock(TextLayer *layer, const char *text, bool hero) {
  if (!layer || !text) return;
  if (prv_leco_safe(text)) {
    text_layer_set_font(layer, hero ? prv_over_numeric_font() : prv_vertrek_numeric_font());
  } else {
    text_layer_set_font(layer, hero ? prv_over_text_font()
        : fonts_get_system_font(prv_countdown_is_large() ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD));
  }
  text_layer_set_text(layer, text);
}

static void prv_set_clock_text(TextLayer *layer, const char *text) {
  prv_set_sized_clock(layer, text, false);
}

static void prv_set_over_text(const char *text) {
  prv_set_sized_clock(s_app.countdown_ui.over_time_layer, text, true);
}

static void prv_refresh_bar_clock(void) {
  if (!s_app.countdown_ui.clock_layer) return;
  time_t now = time(NULL);
  strftime(s_app.buffers.clock_buffer, sizeof(s_app.buffers.clock_buffer), "%H:%M", localtime(&now));
  text_layer_set_text(s_app.countdown_ui.clock_layer, s_app.buffers.clock_buffer);
}

static int prv_chrome_bar(bool large) {
  return large ? 38 : 36;
}

static void prv_fmt_trip_duration(char *buf, size_t n, int dep_epoch, int arr_epoch) {
  buf[0] = '\0';
  if (!dep_epoch || !arr_epoch) return;
  int mins = (arr_epoch - dep_epoch) / 60;
  if (mins < 0) mins += 24 * 60;
  if (mins >= 60) snprintf(buf, n, "%du%02d", mins / 60, mins % 60);
  else snprintf(buf, n, "%dm", mins);
}

static const char *prv_tuple_str(const Tuple *t) {
  if (!t || t->type != TUPLE_CSTRING) return "";
  /* value/cstring are FAM members (never NULL pointers). Empty tuple => "". */
  return t->length ? t->value->cstring : "";
}

static void prv_copy_cstr(char *dst, size_t dst_sz, const char *src) {
  if (!dst || dst_sz == 0) return;
  if (!src) src = "";
  strncpy(dst, src, dst_sz - 1);
  dst[dst_sz - 1] = '\0';
}

static void prv_copy_hhmm(char *dst, size_t n, const char *src) {
  if (!dst || n == 0) return;
  dst[0] = '\0';
  if (!src || !src[0]) return;
  if (src[2] == ':' || src[0] == '-') {
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
    return;
  }
  if (strlen(src) >= 16) {
    strncpy(dst, src + 11, n > 5 ? 5 : n - 1);
    if (n > 5) dst[5] = '\0';
    else dst[n - 1] = '\0';
  }
}

static int prv_delay_minutes(int idx, time_t planned, time_t actual) {
  if (actual && planned && actual > planned) {
    return (int)((actual - planned + 30) / 60);
  }
  const char *d = s_app.trips.delay[idx];
  if (d[0] == '+') return prv_atoi(d + 1);
  return 0;
}

static void prv_place_delay_slot(bool show, bool large, TextLayer *clock) {
  if (!s_app.countdown_ui.delay_layer || !s_app.windows.countdown_window) return;
  Layer *root = window_get_root_layer(s_app.windows.countdown_window);
  GRect bounds = layer_get_bounds(root);
  Layer *dl = text_layer_get_layer(s_app.countdown_ui.delay_layer);
  layer_set_hidden(dl, !show);
  if (!clock || !show) return;
  GRect vtk = layer_get_frame(text_layer_get_layer(clock));
  int delay_w = large ? 58 : 48;
  int right = bounds.size.w - 14; /* leave the leg line */
  int cw = (right - delay_w - 2) - vtk.origin.x;
  if (cw < 48) cw = 48;
  vtk.size.w = cw;
  layer_set_frame(text_layer_get_layer(clock), vtk);
  GRect df = layer_get_frame(dl);
  df.origin.x = vtk.origin.x + vtk.size.w;
  df.origin.y = vtk.origin.y + (large ? 4 : 0);
  df.size.w = delay_w;
  df.size.h = vtk.size.h;
  layer_set_frame(dl, df);
}

static void prv_layout_countdown_clocks(bool hero) {
  if (!s_app.windows.countdown_window || !s_app.countdown_ui.over_label_layer) return;
  GRect bounds = layer_get_bounds(window_get_root_layer(s_app.windows.countdown_window));
  const bool large = prv_is_large_display(bounds);
  const int top_bar = prv_chrome_bar(large);
  const int bot_bar = prv_chrome_bar(large);
  const int cream_top = top_bar;
  const int cream_bottom = bounds.size.h - bot_bar;
  const int x_pad = 4;
  const int times_h = 18;
  const int lab_h = large ? 14 : 10;
  const int lab_w = large ? 80 : 64;
  const int line_pad = 14;
  const int clock_w = bounds.size.w - x_pad - line_pad;
  GFont lab_font = fonts_get_system_font(large ? FONT_KEY_GOTHIC_14 : FONT_KEY_GOTHIC_09);
  text_layer_set_font(s_app.countdown_ui.over_label_layer, lab_font);
  text_layer_set_text_alignment(s_app.countdown_ui.over_label_layer, GTextAlignmentLeft);
  if (s_app.countdown_ui.vertrek_label_layer) {
    text_layer_set_font(s_app.countdown_ui.vertrek_label_layer, lab_font);
    text_layer_set_text_alignment(s_app.countdown_ui.vertrek_label_layer, GTextAlignmentLeft);
  }
  text_layer_set_text_alignment(s_app.countdown_ui.over_time_layer, GTextAlignmentCenter);

  int over_lab_y = cream_top + times_h;
  int over_clock_y, over_clock_h;
  if (hero) {
    over_clock_y = over_lab_y + lab_h + (large ? 2 : 1);
    over_clock_h = cream_bottom - over_clock_y - 2;
    if (over_clock_h < (large ? 48 : 28)) over_clock_h = large ? 48 : 28;
    layer_set_frame(text_layer_get_layer(s_app.countdown_ui.over_label_layer),
                    GRect(x_pad, over_lab_y, clock_w, lab_h));
    layer_set_frame(text_layer_get_layer(s_app.countdown_ui.over_time_layer),
                    GRect(x_pad, over_clock_y, clock_w, over_clock_h));
    if (s_app.countdown_ui.vertrek_label_layer)
      layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.vertrek_label_layer), true);
    if (s_app.countdown_ui.vertrek_time_layer)
      layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.vertrek_time_layer), true);
    if (s_app.countdown_ui.delay_layer)
      layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.delay_layer), true);
    return;
  }

  int remain = cream_bottom - over_lab_y - 2 * lab_h - 4;
  if (remain < 40) remain = 40;
  
  /* Pin VERTREK from cream_bottom, OVER gets remaining height above it */
  /* LECO_26 needs exactly 34px height to avoid clipping footer */
  const int min_vtk_h = large ? 34 : 24;
  const int min_over_h = large ? 52 : 32;
  int vtk_clock_h = large ? 34 : 24;
  
  /* VERTREK clock sits just above the blue footer */
  int vtk_clock_y = cream_bottom - vtk_clock_h;
  if (vtk_clock_y < cream_top) vtk_clock_y = cream_top;
  
  /* VERTREK label sits above the VERTREK clock */
  int vtk_lab_y = vtk_clock_y - lab_h - (large ? 2 : 1);
  
  /* OVER clock fills the space from over_lab_y to vtk_lab_y */
  over_clock_y = over_lab_y + lab_h + (large ? 2 : 1);
  over_clock_h = vtk_lab_y - over_clock_y - (large ? 2 : 1);
  
  /* Enforce minimum heights */
  if (over_clock_h < min_over_h) {
    over_clock_h = min_over_h;
    /* Recalculate VERTREK position if OVER needs more space */
    vtk_lab_y = over_clock_y + over_clock_h + (large ? 2 : 1);
    vtk_clock_y = vtk_lab_y + lab_h + (large ? 2 : 1);
    vtk_clock_h = cream_bottom - vtk_clock_y;
    if (vtk_clock_h < min_vtk_h) vtk_clock_h = min_vtk_h;
  }
  
  if (vtk_clock_h < min_vtk_h) vtk_clock_h = min_vtk_h;

  if (s_app.countdown_ui.vertrek_time_layer) {
    text_layer_set_text_alignment(s_app.countdown_ui.vertrek_time_layer, GTextAlignmentLeft);
  }
  layer_set_frame(text_layer_get_layer(s_app.countdown_ui.over_label_layer),
                  GRect(x_pad, over_lab_y, lab_w, lab_h));
  layer_set_frame(text_layer_get_layer(s_app.countdown_ui.over_time_layer),
                  GRect(x_pad, over_clock_y, clock_w, over_clock_h));
  layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.vertrek_label_layer), false);
  layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.vertrek_time_layer), false);
  layer_set_frame(text_layer_get_layer(s_app.countdown_ui.vertrek_label_layer),
                  GRect(x_pad, vtk_lab_y, lab_w, lab_h));
  layer_set_frame(text_layer_get_layer(s_app.countdown_ui.vertrek_time_layer),
                  GRect(x_pad, vtk_clock_y, clock_w - 58, vtk_clock_h));
  if (s_app.countdown_ui.delay_layer) {
    layer_set_frame(text_layer_get_layer(s_app.countdown_ui.delay_layer),
                    GRect(x_pad + clock_w - 56, vtk_clock_y + (large ? 2 : 0), 56, vtk_clock_h));
  }
}

static void prv_countdown_timer_callback(void *data) {
  time_t now = time(NULL);
  int idx = s_app.journey.selected_trip_index;
  if (idx < 0) idx = 0;
  if (s_app.trips.count > 0 && idx >= s_app.trips.count) idx = s_app.trips.count - 1;
  if (idx >= MAX_TRIPS) idx = MAX_TRIPS - 1;
  s_app.journey.selected_trip_index = idx;
  if (!s_app.countdown_ui.over_time_layer || !s_app.countdown_ui.over_label_layer ||
      !s_app.countdown_ui.vertrek_time_layer || !s_app.countdown_ui.vertrek_label_layer) {
    return;
  }
  time_t actual_dep = s_app.state.departure_time;
  time_t planned_dep = (time_t)s_app.trips.planned_departures_epoch[idx];
  if (planned_dep == 0) planned_dep = actual_dep;
  int actual_remain = (int)(actual_dep - now);
  int planned_remain = (int)(planned_dep - now);
  if (planned_remain < 0) planned_remain = 0;
  bool cancelled = (strncmp(s_app.trips.delay[idx], "Cancelled", 9) == 0);
  int delay_min = cancelled ? 0 : prv_delay_minutes(idx, planned_dep, actual_dep);
  bool ors_on = s_app.settings.reistijd_enabled;
  bool at_station = s_app.routing.at_station;
  bool aankomst = (s_app.settings.tijd_mode == TIJD_MODE_AANKOMST);
  bool waiting_ors = ors_on && !at_station && !s_app.routing.have_duration && !s_app.routing.route_error;
  bool over_uses_slack = ors_on && !at_station && s_app.routing.have_duration;
  int slack = (s_app.routing.travel_duration_min + s_app.routing.station_offset_min) * 60;
  int over_remain;
  if (ors_on && !at_station) {
    /* ORS on and not at station: ALWAYS subtract at least station_offset */
    int offset_sec = s_app.routing.station_offset_min * 60;
    if (s_app.routing.have_duration) {
      /* Have ORS duration: subtract offset + travel time */
      over_remain = actual_remain - slack;
    } else {
      /* Waiting for ORS or have error but no duration yet: subtract offset only */
      over_remain = actual_remain - offset_sec;
    }
  } else if (aankomst && !ors_on) {
    time_t oa = (time_t)s_app.trips.origin_arrivals_epoch[idx];
    if (!oa) oa = planned_dep;
    over_remain = (int)(oa - now);
  } else {
    over_remain = actual_remain;
  }
  prv_layout_countdown_clocks(!ors_on);
  prv_refresh_bar_clock();

#ifdef PBL_COLOR
  GColor band = GColorYellow;
  GColor over_fg = GColorBlack;
  if (cancelled) {
    band = GColorRed;
    over_fg = GColorWhite;
  } else if (waiting_ors) {
    band = GColorYellow;
    over_fg = GColorDarkGray;
  } else if (over_uses_slack) {
    if (over_remain < 0) {
      band = GColorRed;
      over_fg = GColorWhite;
    } else if (over_remain <= 120) {
      band = GColorYellow;
      over_fg = GColorYellow;
    } else {
      band = GColorYellow;
      over_fg = GColorIslamicGreen;
    }
  } else if (at_station) {
    band = GColorYellow;
    over_fg = GColorBlack;
  }
  if (s_app.countdown_ui.bg_yellow_layer) {
    prv_set_mid_band(band);
  }
  GColor label_fg = gcolor_equal(band, GColorRed) ? GColorWhite : GColorBlack;
  GColor body_fg = gcolor_equal(band, GColorRed) ? GColorWhite : GColorBlack;
  if (s_app.countdown_ui.over_time_layer) {
    text_layer_set_text_color(s_app.countdown_ui.over_time_layer, over_fg);
  }
  if (s_app.countdown_ui.over_label_layer) {
    text_layer_set_text_color(s_app.countdown_ui.over_label_layer, label_fg);
  }
  if (s_app.countdown_ui.duration_layer) {
    text_layer_set_text_color(s_app.countdown_ui.duration_layer, body_fg);
  }
  if (s_app.countdown_ui.vertrek_time_layer) {
    text_layer_set_text_color(s_app.countdown_ui.vertrek_time_layer, body_fg);
  }
  if (s_app.countdown_ui.vertrek_label_layer) {
    text_layer_set_text_color(s_app.countdown_ui.vertrek_label_layer, label_fg);
  }
#endif

  layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.over_label_layer), false);
  layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.over_time_layer), false);
  if (ors_on) {
    layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.vertrek_label_layer), false);
    layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.vertrek_time_layer), false);
    text_layer_set_text(s_app.countdown_ui.over_label_layer, "OVER");
    text_layer_set_text(s_app.countdown_ui.vertrek_label_layer, "VERTREK");
  } else {
    layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.vertrek_label_layer), true);
    layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.vertrek_time_layer), true);
    if (s_app.countdown_ui.delay_layer)
      layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.delay_layer), true);
    text_layer_set_text(s_app.countdown_ui.over_label_layer,
                        aankomst ? "AANKOMST" : "VERTREK");
  }

  if (cancelled) {
    prv_set_over_text("GEANNULEERD");
  } else if (waiting_ors) {
    prv_set_over_text("...");
  } else {
    prv_fmt_remain(s_app.buffers.over_buffer, sizeof(s_app.buffers.over_buffer), over_remain, true);
    prv_set_over_text(s_app.buffers.over_buffer);
  }

  bool actual_passed = (actual_dep && actual_remain <= 0);
  if (cancelled) {
    snprintf(s_app.buffers.vertrek_buffer, sizeof(s_app.buffers.vertrek_buffer), "--:--");
  } else if (actual_passed) {
    snprintf(s_app.buffers.vertrek_buffer, sizeof(s_app.buffers.vertrek_buffer), "Departed");
    time_t idle_since = s_app.state.last_button_time > actual_dep
        ? s_app.state.last_button_time : actual_dep;
    if ((now - idle_since) >= 180) {
      window_stack_pop_all(true);
      return;
    }
  } else {
    prv_fmt_remain(s_app.buffers.vertrek_buffer, sizeof(s_app.buffers.vertrek_buffer), planned_remain, false);
  }
  if (ors_on) {
  prv_set_clock_text(s_app.countdown_ui.vertrek_time_layer, s_app.buffers.vertrek_buffer);

  bool show_delay = (delay_min > 0 && !actual_passed && !cancelled);
  if (show_delay) {
    if (planned_dep > now) {
      snprintf(s_app.buffers.delay_buffer, sizeof(s_app.buffers.delay_buffer), "+%d", delay_min);
    } else {
      int delay_remain = actual_remain > 0 ? actual_remain : 0;
      prv_fmt_remain(s_app.buffers.delay_buffer, sizeof(s_app.buffers.delay_buffer), delay_remain, false);
    }
  } else {
    s_app.buffers.delay_buffer[0] = '\0';
  }
  if (show_delay) {
    text_layer_set_text(s_app.countdown_ui.delay_layer, s_app.buffers.delay_buffer);
  } else {
    text_layer_set_text(s_app.countdown_ui.delay_layer, "");
  }
#ifdef PBL_COLOR
  text_layer_set_text_color(s_app.countdown_ui.delay_layer, GColorRed);
#endif
  text_layer_set_font(s_app.countdown_ui.delay_layer,
      fonts_get_system_font(prv_countdown_is_large() ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD));
  layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.delay_layer), !show_delay);
  prv_place_delay_slot(show_delay, prv_countdown_is_large(), s_app.countdown_ui.vertrek_time_layer);
  }

  s_app.state.countdown_timer = app_timer_register(1000, prv_countdown_timer_callback, NULL);
}

static void prv_parse_time_and_start_timer() {
  if (s_app.state.countdown_timer) {
    app_timer_cancel(s_app.state.countdown_timer);
    s_app.state.countdown_timer = NULL;
  }
  if (s_app.trips.count == 0 || s_app.trips.departures[s_app.journey.selected_trip_index] == 0) {
    if (s_app.countdown_ui.vertrek_time_layer) {
      text_layer_set_text(s_app.countdown_ui.vertrek_time_layer, "--:--");
    }
    return;
  }
  s_app.state.departure_time = s_app.trips.departures[s_app.journey.selected_trip_index];
  prv_countdown_timer_callback(NULL);
}


static void prv_trip_leg_layer_update_proc(Layer *layer, GContext *ctx) {
  int transfers = 0;
  if (s_app.trips.count > 0 && s_app.trips.transfers[s_app.journey.selected_trip_index][0] != '\0') {
    transfers = prv_atoi(s_app.trips.transfers[s_app.journey.selected_trip_index]);
  }
  int num_legs = transfers + 1;

  const int max_legs = MAX_LEGS;
  if (num_legs < 1) num_legs = 1;
  if (num_legs > max_legs) num_legs = max_legs;

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

  int tidx = s_app.journey.selected_trip_index;
  if (s_app.countdown_ui.duration_layer) {
    int dep_e = s_app.trips.planned_departures_epoch[tidx];
    int arr_e = s_app.trips.arrivals_epoch[tidx];
    prv_fmt_trip_duration(s_app.buffers.duration_buffer, sizeof(s_app.buffers.duration_buffer), dep_e, arr_e);
    text_layer_set_text(s_app.countdown_ui.duration_layer, s_app.buffers.duration_buffer);
  }

  prv_copy_hhmm(s_app.buffers.departure_time_buffer, sizeof(s_app.buffers.departure_time_buffer),
                s_app.trips.planned_departures[tidx]);
  if (s_app.countdown_ui.departure_time_layer && s_app.buffers.departure_time_buffer[0]) {
    text_layer_set_text(s_app.countdown_ui.departure_time_layer, s_app.buffers.departure_time_buffer);
  }

  {
    const char *arr_src = s_app.trips.planned_arrivals[tidx];
    if (!arr_src || !arr_src[0] || arr_src[0] == '-') {
      arr_src = s_app.trips.arrivals[tidx];
    }
    prv_copy_hhmm(s_app.buffers.arrival_time_buffer, sizeof(s_app.buffers.arrival_time_buffer),
                  arr_src);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "footer arr planned=%s actual=%s",
            s_app.trips.planned_arrivals[tidx], s_app.trips.arrivals[tidx]);
  }
  if (s_app.countdown_ui.arrival_time_layer && s_app.buffers.arrival_time_buffer[0]) {
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

static void prv_menu_select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  int row = cell_index->row;
  if (!s_app.stations.loaded || row < 0 || row >= s_app.stations.count) return;
  prv_add_favourite(s_app.stations.codes[row], s_app.stations.names[row]);
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
    .select_long_click = prv_menu_select_long_callback,
  });
  #ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_app.menu_layers.menu_layer, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(s_app.menu_layers.menu_layer, GColorOxfordBlue, GColorWhite);
  #endif
  layer_add_child(window_layer, menu_layer_get_layer(s_app.menu_layers.menu_layer));
  MenuIndex index = MenuIndex(0, s_app.state.last_selected_index);
  menu_layer_set_selected_index(s_app.menu_layers.menu_layer, index, MenuRowAlignCenter, false);
}

static void prv_menu_window_unload(Window *window) {
  (void)window;
  if (s_app.menu_layers.menu_layer) {
    menu_layer_destroy(s_app.menu_layers.menu_layer);
    s_app.menu_layers.menu_layer = NULL;
  }
}

static uint16_t prv_alpha_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return alphabet_index[s_app.state.selected_alphabet_index].count;
}

static void prv_alpha_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  int station_index = alphabet_index[s_app.state.selected_alphabet_index].start_index + cell_index->row;
  const Station *station = &all_stations[station_index];
  menu_cell_basic_draw(ctx, cell_layer, station->name, NULL, NULL);
}

static void prv_alpha_menu_select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  int station_index = alphabet_index[s_app.state.selected_alphabet_index].start_index + cell_index->row;
  const Station *station = &all_stations[station_index];
  prv_add_favourite(station->code, station->name);
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
    .select_long_click = prv_alpha_menu_select_long_callback,
  });
  #ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_app.menu_layers.alpha_menu_layer, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(s_app.menu_layers.alpha_menu_layer, GColorOxfordBlue, GColorWhite);
  #endif
  layer_add_child(window_layer, menu_layer_get_layer(s_app.menu_layers.alpha_menu_layer));
}

static void prv_alpha_menu_window_unload(Window *window) {
  (void)window;
  if (s_app.menu_layers.alpha_menu_layer) {
    menu_layer_destroy(s_app.menu_layers.alpha_menu_layer);
    s_app.menu_layers.alpha_menu_layer = NULL;
  }
}

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
  if (section_index == 0) {
    header = s_app.state.selecting_start_station ? "Van" : "Naar";
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

static void prv_dest_menu_select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  int section = cell_index->section;
  int row = cell_index->row;
  if (s_app.favourites.count > 0) {
    if (section == 0) {
      prv_add_favourite(s_app.favourites.codes[row], s_app.favourites.names[row]);
      return;
    }
    section--;
  }
  if (section == 0) {
    const Station *station = &top_stations[row];
    prv_add_favourite(station->code, station->name);
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

static void prv_dest_menu_attach(Window *window) {
  if (!window) return;
  if (s_app.menu_layers.dest_menu_layer) {
    layer_set_hidden(menu_layer_get_layer(s_app.menu_layers.dest_menu_layer), false);
    menu_layer_set_click_config_onto_window(s_app.menu_layers.dest_menu_layer, window);
    return;
  }
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_app.menu_layers.dest_menu_layer = menu_layer_create(bounds);
  if (!s_app.menu_layers.dest_menu_layer) return;
  menu_layer_set_click_config_onto_window(s_app.menu_layers.dest_menu_layer, window);
  menu_layer_set_callbacks(s_app.menu_layers.dest_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_sections = prv_dest_menu_get_num_sections_callback,
    .get_num_rows = prv_dest_menu_get_num_rows_callback,
    .get_header_height = prv_dest_menu_get_header_height_callback,
    .draw_header = prv_dest_menu_draw_header_callback,
    .draw_row = prv_dest_menu_draw_row_callback,
    .select_click = prv_dest_menu_select_callback,
    .select_long_click = prv_dest_menu_select_long_callback,
  });
  #ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_app.menu_layers.dest_menu_layer, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(s_app.menu_layers.dest_menu_layer, GColorOxfordBlue, GColorWhite);
  #endif
  layer_add_child(window_layer, menu_layer_get_layer(s_app.menu_layers.dest_menu_layer));
}

static void prv_dest_menu_window_load(Window *window) {
  prv_dest_menu_attach(window);
}

static void prv_dest_menu_window_unload(Window *window) {
  (void)window;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "dest_menu_window_unload: heap=%d has_layer=%d",
          (int)heap_bytes_free(), s_app.menu_layers.dest_menu_layer ? 1 : 0);
  if (s_app.state.loading_show_timer) {
    app_timer_cancel(s_app.state.loading_show_timer);
    s_app.state.loading_show_timer = NULL;
  }
  prv_destroy_loading_ui();

  /* Countdown is on the stack: dest_menu_layer must already have been destroyed
   * in prv_present_countdown. Destroying a MenuLayer after this window unload
   * crashes Time 2 firmware (qemu often does not). */
  if (s_app.windows.countdown_window &&
      window_stack_contains_window(s_app.windows.countdown_window)) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "dest_menu_window_unload: countdown active, not destroying layer");
    return;
  }

  prv_destroy_dest_menu_layer();
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *station_index_tuple = dict_find(iter, MESSAGE_KEY_STATION_INDEX);
  Tuple *station_name_tuple = dict_find(iter, MESSAGE_KEY_STATION_NAME);
  Tuple *station_code_tuple = dict_find(iter, MESSAGE_KEY_STATION_CODE);
  Tuple *station_count_tuple = dict_find(iter, MESSAGE_KEY_STATION_COUNT);
  Tuple *trip_index_tuple = dict_find(iter, MESSAGE_KEY_TRIP_INDEX);
  Tuple *trip_planned_departure_time_tuple = dict_find(iter, MESSAGE_KEY_TRIP_PLANNED_DEPARTURE_TIME);
  Tuple *trip_planned_departure_epoch_tuple = dict_find(iter, MESSAGE_KEY_TRIP_PLANNED_DEPARTURE_TIME_EPOCH);
  Tuple *trip_departure_time_epoch_tuple = dict_find(iter, MESSAGE_KEY_TRIP_DEPARTURE_TIME_EPOCH);
  Tuple *trip_planned_arrival_time_tuple = dict_find(iter, MESSAGE_KEY_TRIP_PLANNED_ARRIVAL_TIME);
  Tuple *trip_arrival_time_tuple = dict_find(iter, MESSAGE_KEY_TRIP_ARRIVAL_TIME);
  Tuple *trip_arrival_epoch_tuple = dict_find(iter, MESSAGE_KEY_TRIP_ARRIVAL_TIME_EPOCH);
  Tuple *trip_origin_arrival_epoch_tuple = dict_find(iter, MESSAGE_KEY_TRIP_ORIGIN_ARRIVAL_EPOCH);
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
  Tuple *route_error_tuple = dict_find(iter, MESSAGE_KEY_ROUTE_ERROR);
  Tuple *station_offset_tuple = dict_find(iter, MESSAGE_KEY_STATION_OFFSET);
  Tuple *trips_failed_tuple = dict_find(iter, MESSAGE_KEY_TRIPS_FAILED);
  Tuple *settings_tijd_tuple = dict_find(iter, MESSAGE_KEY_SETTINGS_TIJD_MODE);
  Tuple *settings_reistijd_tuple = dict_find(iter, MESSAGE_KEY_SETTINGS_REISTIJD);
  Tuple *settings_vervoer_tuple = dict_find(iter, MESSAGE_KEY_SETTINGS_VERVOER);
  
  if (error_tuple) {
    s_app.state.refresh_in_flight = false;
    if (s_app.trips.loaded) {
      return;
    }
    if (s_app.main_ui.text_layer && !s_app.windows.dest_menu_window) {
      text_layer_set_text(s_app.main_ui.text_layer, "Add API key in settings...");
    }
    prv_arm_loading_fail();
    return;
  }

  if (station_count_tuple && station_count_tuple->value->int32 <= 0) {
    prv_open_start_station_picker();
    return;
  }

  if (station_index_tuple && station_name_tuple && station_count_tuple && station_code_tuple) {
    int index = station_index_tuple->value->int32;
    const char *name = prv_tuple_str(station_name_tuple);
    const char *code = prv_tuple_str(station_code_tuple);
    int count = station_count_tuple->value->int32;
    if (index >= 0 && index < MAX_STATIONS) {
      prv_copy_cstr(s_app.stations.names[index], MAX_STATION_NAME_LENGTH, name);
      prv_copy_cstr(s_app.stations.codes[index], MAX_STATION_CODE_LENGTH, code);
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
    const char *planned_departure_time = prv_tuple_str(trip_planned_departure_time_tuple);
    int departure_time = trip_departure_time_epoch_tuple->value->int32;
    const char *planned_arrival_time = prv_tuple_str(trip_planned_arrival_time_tuple);
    const char *arrival_time = prv_tuple_str(trip_arrival_time_tuple);
    int count = trip_count_tuple->value->int32;
    int transfers_val = trip_transfers_tuple->value->int32;
    const char *platform = prv_tuple_str(trip_platform_tuple);
    const char *delay = prv_tuple_str(trip_delay_tuple);

    if (index >= 0 && index < MAX_TRIPS) {
      prv_copy_hhmm(s_app.trips.planned_departures[index], MAX_DATE_TIME_LENGTH, planned_departure_time);
      int prev_dep = s_app.trips.departures[index];
      s_app.trips.departures[index] = departure_time;
      if (s_app.trips.loaded && index == s_app.journey.selected_trip_index &&
          prev_dep != 0 && prev_dep != departure_time) {
        vibes_short_pulse();
      }

      prv_copy_hhmm(s_app.trips.planned_arrivals[index], MAX_DATE_TIME_LENGTH, planned_arrival_time);
      prv_copy_hhmm(s_app.trips.arrivals[index], MAX_DATE_TIME_LENGTH, arrival_time);
      s_app.trips.arrivals_epoch[index] = trip_arrival_epoch_tuple
          ? trip_arrival_epoch_tuple->value->int32 : 0;
      s_app.trips.planned_departures_epoch[index] = trip_planned_departure_epoch_tuple
          ? trip_planned_departure_epoch_tuple->value->int32 : 0;
      s_app.trips.origin_arrivals_epoch[index] = trip_origin_arrival_epoch_tuple
          ? trip_origin_arrival_epoch_tuple->value->int32 : 0;

      snprintf(s_app.trips.transfers[index], MAX_TRANSFERS_LENGTH, "%d", transfers_val);
      prv_copy_cstr(s_app.trips.platform[index], MAX_PLATFORM_LENGTH, platform);
      prv_copy_cstr(s_app.trips.delay[index], MAX_DELAY_LENGTH, delay);

      if (index + 1 > s_app.trips.count) { s_app.trips.count = index + 1; }
      if (index < 8) s_app.trips.received_mask |= (uint8_t)(1u << index);
      int need = count;
      if (need > MAX_TRIPS) need = MAX_TRIPS;
      uint8_t want = (uint8_t)((1u << need) - 1u);
      if (need > 0 && (s_app.trips.received_mask & want) == want) {
        s_app.trips.count = need;
        s_app.trips.loaded = true;
        s_app.state.refresh_in_flight = false;
        prv_present_countdown();
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
    if (trip_idx < 0) trip_idx = 0;
    if (trip_idx >= MAX_TRIPS) trip_idx = MAX_TRIPS - 1;
    if (leg_idx < 0) leg_idx = 0;
    if (leg_idx >= MAX_LEGS) leg_idx = MAX_LEGS - 1;
    const char *departure_station = prv_tuple_str(leg_departure_station_tuple);
    const char *departure_platform = prv_tuple_str(leg_departure_platform_tuple);
    const char *departure_time = prv_tuple_str(leg_departure_time_tuple);
    const char *arrival_station = prv_tuple_str(leg_arrival_station_tuple);
    const char *arrival_time = prv_tuple_str(leg_arrival_time_tuple);
    const char *duration = prv_tuple_str(leg_duration_tuple);

    if (trip_idx >= 0 && trip_idx < MAX_TRIPS && leg_idx >= 0 && leg_idx < MAX_LEGS) {
      prv_copy_cstr(s_app.trip_legs[trip_idx].legs[leg_idx].departure_station, MAX_LEG_STATION_LENGTH, departure_station);
      prv_copy_cstr(s_app.trip_legs[trip_idx].legs[leg_idx].departure_platform, MAX_PLATFORM_LENGTH, departure_platform);
      prv_copy_cstr(s_app.trip_legs[trip_idx].legs[leg_idx].departure_time, MAX_LEG_TIME_LENGTH, departure_time);
      prv_copy_cstr(s_app.trip_legs[trip_idx].legs[leg_idx].arrival_station, MAX_LEG_STATION_LENGTH, arrival_station);
      prv_copy_cstr(s_app.trip_legs[trip_idx].legs[leg_idx].arrival_time, MAX_LEG_TIME_LENGTH, arrival_time);
      prv_copy_cstr(s_app.trip_legs[trip_idx].legs[leg_idx].duration, MAX_LEG_DURATION_LENGTH, duration);

      Tuple *leg_dep_epoch_tuple = dict_find(iter, MESSAGE_KEY_LEG_DEPARTURE_EPOCH);
      Tuple *leg_arr_epoch_tuple = dict_find(iter, MESSAGE_KEY_LEG_ARRIVAL_EPOCH);
      if (leg_dep_epoch_tuple) {
        s_app.trip_legs[trip_idx].legs[leg_idx].dep_epoch = (uint32_t)leg_dep_epoch_tuple->value->int32;
      }
      if (leg_arr_epoch_tuple) {
        s_app.trip_legs[trip_idx].legs[leg_idx].arr_epoch = (uint32_t)leg_arr_epoch_tuple->value->int32;
      }

      if (leg_count > MAX_LEGS) leg_count = MAX_LEGS;
      if (leg_count < 0) leg_count = 0;
      s_app.trip_legs[trip_idx].leg_count = leg_count;
    }
  }

  // Handle favourite station messages
  if (favourite_index_tuple && favourite_code_tuple && favourite_name_tuple && favourite_count_tuple) {
    int index = favourite_index_tuple->value->int32;
    const char *code = prv_tuple_str(favourite_code_tuple);
    const char *name = prv_tuple_str(favourite_name_tuple);
    int count = favourite_count_tuple->value->int32;

    if (index >= 0 && index < MAX_FAVOURITES) {
      prv_copy_cstr(s_app.favourites.codes[index], MAX_STATION_CODE_LENGTH, code);
      prv_copy_cstr(s_app.favourites.names[index], MAX_STATION_NAME_LENGTH, name);

      if (index + 1 > s_app.favourites.count) {
        s_app.favourites.count = index + 1;
      }
      if (s_app.favourites.count >= count) {
        s_app.favourites.loaded = true;
        // Reload start menu if it exists to show favourites section
        if (s_app.menu_layers.menu_layer) {
          menu_layer_reload_data(s_app.menu_layers.menu_layer);
        }
        if (s_app.menu_layers.dest_menu_layer) {
          menu_layer_reload_data(s_app.menu_layers.dest_menu_layer);
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
  if (trips_failed_tuple) {
    s_app.state.refresh_in_flight = false;
    prv_arm_loading_fail();
  }
  if (route_error_tuple) {
    s_app.routing.route_error = (uint8_t)route_error_tuple->value->int32;
    /* Keep last duration on error - do not reset have_duration if we already had one */
    s_app.routing.at_station = false;
    s_app.state.refresh_in_flight = false;
    if (s_app.countdown_ui.vertrek_time_layer) {
      prv_update_countdown_display();
    }
  }
  if (route_duration_tuple) {
    int d = (int)route_duration_tuple->value->int32;
    s_app.routing.route_error = 0;
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
static void prv_outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", (int)reason);
  if (iter && dict_find(iter, MESSAGE_KEY_DEST_STATION_CODE) && !s_app.trips.loaded) {
    prv_arm_loading_fail();
  }
}
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
  (void)window;
  if (s_app.main_ui.spinner_layer) {
    layer_destroy(s_app.main_ui.spinner_layer);
    s_app.main_ui.spinner_layer = NULL;
  }
  if (!s_app.loading_ui.spinner_layer && s_app.state.spinner_timer) {
    app_timer_cancel(s_app.state.spinner_timer);
    s_app.state.spinner_timer = NULL;
  }

  if (s_app.main_ui.text_layer) {
    text_layer_destroy(s_app.main_ui.text_layer);
    s_app.main_ui.text_layer = NULL;
  }
  if (s_app.main_ui.bg_blue_layer) {
    layer_destroy(s_app.main_ui.bg_blue_layer);
    s_app.main_ui.bg_blue_layer = NULL;
  }
  if (s_app.main_ui.bg_blue_bottom_layer) {
    layer_destroy(s_app.main_ui.bg_blue_bottom_layer);
    s_app.main_ui.bg_blue_bottom_layer = NULL;
  }
  #ifdef PBL_COLOR
  if (s_app.main_ui.bg_yellow_layer) {
    layer_destroy(s_app.main_ui.bg_yellow_layer);
    s_app.main_ui.bg_yellow_layer = NULL;
  }
  #endif
}

static void prv_init(void) {
  // Initialize all app data to zero
  memset(&s_app, 0, sizeof(AppData));
  s_app.buffers.letter_str[0] = 'A';
  s_app.buffers.letter_str[1] = '\0';
  s_app.settings.reistijd_enabled = true;
  s_app.routing.station_offset_min = 2;
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
  app_message_open(1024, 256);
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


static void prv_loading_fail_timeout(void *data) {
  (void)data;
  s_app.state.loading_fail_timer = NULL;
  prv_dismiss_trips_loading();
}

static void prv_arm_loading_fail(void) {
  if (s_app.trips.loaded) return;
  if (s_app.loading_ui.status_layer) {
    text_layer_set_text(s_app.loading_ui.status_layer, "geen ritten");
  }
  if (s_app.state.loading_fail_timer) {
    app_timer_cancel(s_app.state.loading_fail_timer);
  }
  s_app.state.loading_fail_timer = app_timer_register(1500, prv_loading_fail_timeout, NULL);
}

static void prv_pop_alpha_timeout(void *data) {
  (void)data;
  s_pop_alpha_timer = NULL;
  if (s_app.windows.alpha_menu_window &&
      window_stack_contains_window(s_app.windows.alpha_menu_window)) {
    window_stack_remove(s_app.windows.alpha_menu_window, false);
  }
}

static void prv_loading_show_timeout(void *data) {
  (void)data;
  s_app.state.loading_show_timer = NULL;
  prv_push_trips_loading();
}

static void prv_destroy_loading_ui(void) {
  if (s_app.state.loading_fail_timer) {
    app_timer_cancel(s_app.state.loading_fail_timer);
    s_app.state.loading_fail_timer = NULL;
  }
  if (s_app.state.spinner_timer && !s_app.main_ui.spinner_layer) {
    app_timer_cancel(s_app.state.spinner_timer);
    s_app.state.spinner_timer = NULL;
  }
  if (s_app.loading_ui.title_layer) {
    text_layer_destroy(s_app.loading_ui.title_layer);
    s_app.loading_ui.title_layer = NULL;
  }
  if (s_app.loading_ui.journey_layer) {
    text_layer_destroy(s_app.loading_ui.journey_layer);
    s_app.loading_ui.journey_layer = NULL;
  }
  if (s_app.loading_ui.status_layer) {
    text_layer_destroy(s_app.loading_ui.status_layer);
    s_app.loading_ui.status_layer = NULL;
  }
  if (s_app.loading_ui.spinner_layer) {
    layer_destroy(s_app.loading_ui.spinner_layer);
    s_app.loading_ui.spinner_layer = NULL;
  }
  if (s_app.loading_ui.bg_layer) {
    layer_destroy(s_app.loading_ui.bg_layer);
    s_app.loading_ui.bg_layer = NULL;
  }
}

static void prv_push_trips_loading(void) {
  if (s_app.windows.alpha_menu_window &&
      window_stack_contains_window(s_app.windows.alpha_menu_window)) {
    window_stack_remove(s_app.windows.alpha_menu_window, false);
  }
  Window *w = s_app.windows.dest_menu_window;
  if (!w) {
    prv_push_dest_menu();
    w = s_app.windows.dest_menu_window;
  }
  if (!w) return;
  Layer *root = window_get_root_layer(w);
  GRect bounds = layer_get_bounds(root);

  /* Hide dest menu in-place; never destroy it from the select callback path. */
  if (s_app.menu_layers.dest_menu_layer) {
    layer_set_hidden(menu_layer_get_layer(s_app.menu_layers.dest_menu_layer), true);
    window_set_click_config_provider(w, prv_noop_click_config);
  }
  prv_destroy_loading_ui();

  /* Full OxfordBlue overlay covering entire screen */
  s_app.loading_ui.bg_layer = layer_create(bounds);
  if (s_app.loading_ui.bg_layer) {
    layer_set_update_proc(s_app.loading_ui.bg_layer, prv_loading_bg_update_proc);
    layer_add_child(root, s_app.loading_ui.bg_layer);
  }

  const int top_margin = 40;
  const int line_height = 24;
  int y = top_margin;

  /* Title "Trein" */
  s_app.loading_ui.title_layer = text_layer_create(GRect(0, y, bounds.size.w, line_height));
  if (s_app.loading_ui.title_layer) {
    text_layer_set_font(s_app.loading_ui.title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(s_app.loading_ui.title_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_app.loading_ui.title_layer, GColorClear);
    text_layer_set_text_color(s_app.loading_ui.title_layer, GColorWhite);
    text_layer_set_text(s_app.loading_ui.title_layer, "Trein");
    layer_add_child(root, text_layer_get_layer(s_app.loading_ui.title_layer));
  }
  y += line_height + 4;

  /* Journey: "Start → Dest" */
  static char journey_text[MAX_STATION_NAME_LENGTH * 2 + 4];
  snprintf(journey_text, sizeof(journey_text), "%s → %s",
           s_app.journey.start_station_name[0] ? s_app.journey.start_station_name : "?",
           s_app.journey.dest_station_name[0] ? s_app.journey.dest_station_name : "?");
  s_app.loading_ui.journey_layer = text_layer_create(GRect(4, y, bounds.size.w - 8, line_height));
  if (s_app.loading_ui.journey_layer) {
    text_layer_set_font(s_app.loading_ui.journey_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_app.loading_ui.journey_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_app.loading_ui.journey_layer, GColorClear);
    text_layer_set_text_color(s_app.loading_ui.journey_layer, GColorWhite);
    text_layer_set_text(s_app.loading_ui.journey_layer, journey_text);
    layer_add_child(root, text_layer_get_layer(s_app.loading_ui.journey_layer));
  }
  y += line_height + 16;

  /* Rotating arc spinner */
  const int spinner_size = 48;
  s_app.loading_ui.spinner_layer = layer_create(
      GRect((bounds.size.w - spinner_size) / 2, y, spinner_size, spinner_size));
  if (s_app.loading_ui.spinner_layer) {
    layer_set_update_proc(s_app.loading_ui.spinner_layer, prv_spinner_layer_update_proc);
    layer_add_child(root, s_app.loading_ui.spinner_layer);
  }
  y += spinner_size + 8;

  /* Status "Ritten laden" */
  s_app.loading_ui.status_layer = text_layer_create(GRect(0, y, bounds.size.w, line_height));
  if (s_app.loading_ui.status_layer) {
    text_layer_set_font(s_app.loading_ui.status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_app.loading_ui.status_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_app.loading_ui.status_layer, GColorClear);
    text_layer_set_text_color(s_app.loading_ui.status_layer, GColorWhite);
    text_layer_set_text(s_app.loading_ui.status_layer, "Ritten laden");
    layer_add_child(root, text_layer_get_layer(s_app.loading_ui.status_layer));
  }

  if (!s_app.state.spinner_timer) {
    s_app.state.spinner_angle = 0;
    s_app.state.spinner_timer = app_timer_register(50, prv_spinner_timer_callback, NULL);
  }
}

static void prv_restore_dest_menu(void) {
  if (!s_app.windows.dest_menu_window) return;
  if (s_app.windows.countdown_window &&
      window_stack_contains_window(s_app.windows.countdown_window)) {
    return;
  }
  prv_dest_menu_attach(s_app.windows.dest_menu_window);
}

static void prv_dismiss_trips_loading(void) {
  prv_destroy_loading_ui();
  prv_restore_dest_menu();
}

static void prv_pop_station_windows(void) {
  if (s_app.windows.alpha_menu_window &&
      window_stack_contains_window(s_app.windows.alpha_menu_window)) {
    window_stack_remove(s_app.windows.alpha_menu_window, false);
  }
  if (s_app.windows.dest_menu_window &&
      window_stack_contains_window(s_app.windows.dest_menu_window)) {
    window_stack_remove(s_app.windows.dest_menu_window, false);
  }
  if (s_app.windows.menu_window &&
      window_stack_contains_window(s_app.windows.menu_window)) {
    window_stack_remove(s_app.windows.menu_window, false);
  }
}

static void prv_pop_stations_timeout(void *data) {
  (void)data;
  s_app.state.pop_stations_timer = NULL;
  prv_pop_station_windows();
}

static void prv_destroy_dest_menu_layer(void) {
  if (s_app.state.deferred_menu_destroy_timer) {
    app_timer_cancel(s_app.state.deferred_menu_destroy_timer);
    s_app.state.deferred_menu_destroy_timer = NULL;
  }
  if (!s_app.menu_layers.dest_menu_layer) return;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "destroy dest_menu_layer heap=%d", (int)heap_bytes_free());
  if (s_app.windows.dest_menu_window) {
    window_set_click_config_provider(s_app.windows.dest_menu_window, prv_noop_click_config);
  }
  menu_layer_destroy(s_app.menu_layers.dest_menu_layer);
  s_app.menu_layers.dest_menu_layer = NULL;
}

static void prv_present_countdown(void) {
  if (s_app.state.loading_show_timer) {
    app_timer_cancel(s_app.state.loading_show_timer);
    s_app.state.loading_show_timer = NULL;
  }

  /* Inbox path, not MenuLayer select: drop other station menus and dest MenuLayer
   * before countdown alloc. Time 2 crashes if we menu_layer_destroy after the
   * parent window has already unloaded. Keep the loading overlay until countdown
   * is on top so dest does not flash empty. */
  if (s_app.windows.alpha_menu_window &&
      window_stack_contains_window(s_app.windows.alpha_menu_window)) {
    window_stack_remove(s_app.windows.alpha_menu_window, false);
  }
  if (s_app.windows.menu_window &&
      window_stack_contains_window(s_app.windows.menu_window)) {
    window_stack_remove(s_app.windows.menu_window, false);
  }
  prv_destroy_dest_menu_layer();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "present_countdown after menu free heap=%d", (int)heap_bytes_free());

  if (!s_app.windows.countdown_window) {
    s_app.windows.countdown_window = window_create();
    if (!s_app.windows.countdown_window) return; /* Abort on NULL */
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

  prv_destroy_loading_ui();

  if (s_app.state.pop_stations_timer) {
    app_timer_cancel(s_app.state.pop_stations_timer);
  }
  s_app.state.pop_stations_timer = app_timer_register(60, prv_pop_stations_timeout, NULL);
}

static void prv_send_trip_request(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_cstring(iter, MESSAGE_KEY_START_STATION_CODE, s_app.journey.start_station_code);
    dict_write_cstring(iter, MESSAGE_KEY_DEST_STATION_CODE, s_app.journey.dest_station_code);
    if (s_app.settings.reistijd_enabled) {
      prv_write_route_payload(iter);
    }
    if (app_message_outbox_send() != APP_MSG_OK) {
      prv_arm_loading_fail();
    }
  } else {
    prv_arm_loading_fail();
  }
}

// --- Journey Details Window ---
static int prv_clamp_trip_idx(int trip_idx) {
  if (trip_idx < 0) return 0;
  if (trip_idx >= MAX_TRIPS) return MAX_TRIPS - 1;
  return trip_idx;
}

static uint16_t prv_journey_details_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  int trip_idx = prv_clamp_trip_idx(s_app.journey.selected_trip_index);
  int n = s_app.trip_legs[trip_idx].leg_count;
  if (n < 0) n = 0;
  if (n > MAX_LEGS) n = MAX_LEGS;
  return (uint16_t)n;
}

static int16_t prv_journey_details_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  return 52;
}

static void prv_journey_details_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  int trip_idx = prv_clamp_trip_idx(s_app.journey.selected_trip_index);
  int leg_idx = cell_index->row;
  int n = s_app.trip_legs[trip_idx].leg_count;
  if (n > MAX_LEGS) n = MAX_LEGS;
  if (leg_idx < 0 || leg_idx >= n) return;
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
  int trip_idx = prv_clamp_trip_idx(s_app.journey.selected_trip_index);
  int leg_count = s_app.trip_legs[trip_idx].leg_count;
  if (leg_count > MAX_LEGS) leg_count = MAX_LEGS;
  if (leg_count > MAX_PIN_QUEUE) leg_count = MAX_PIN_QUEUE;
  if (leg_count < 0) leg_count = 0;
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
  (void)window;
  simple_menu_layer_destroy(s_pin_simple_menu_layer);
  s_pin_simple_menu_layer = NULL;
}

static void prv_journey_details_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  s_pin_selected_leg_index = cell_index->row;
  if (!s_app.windows.pin_menu_window) {
    s_app.windows.pin_menu_window = window_create();
    window_set_window_handlers(s_app.windows.pin_menu_window, (WindowHandlers){
      .load = prv_pin_menu_window_load,
      .unload = prv_pin_menu_window_unload,
    });
  }
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
  strncpy(s_settings_sub[0], s_app.settings.reistijd_enabled ? "Ja" : "Nee", 23);
  strncpy(s_settings_sub[1], s_app.settings.tijd_mode == TIJD_MODE_AANKOMST ? "Aankomst" : "Vertrek", 23);
  strncpy(s_settings_sub[2], s_app.settings.vervoer_mode == VERVOER_FIETS ? "Fiets" : "Lopen", 23);
  snprintf(s_settings_sub[3], sizeof(s_settings_sub[3]), "%+d min", s_app.routing.station_offset_min);
}

static uint16_t prv_settings_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  (void)menu_layer; (void)section_index; (void)context;
  return 4;
}

static void prv_settings_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  (void)context;
  const char *title = "ORS";
  if (cell_index->row == 1) title = "Tijd";
  else if (cell_index->row == 2) title = "Vervoer";
  else if (cell_index->row == 3) title = "Slack";
  menu_cell_basic_draw(ctx, cell_layer, title, s_settings_sub[cell_index->row], NULL);
}

static void prv_settings_apply(void) {
  prv_persist_settings();
  prv_send_settings_to_phone();
  prv_settings_rebuild_items();
  if (s_settings_menu_layer) menu_layer_reload_data(s_settings_menu_layer);
}

static void prv_settings_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  (void)menu_layer; (void)context;
  int row = cell_index->row;
  if (row == 0) {
    s_app.settings.reistijd_enabled = !s_app.settings.reistijd_enabled;
    prv_settings_apply();
  } else if (row == 1) {
    s_app.settings.tijd_mode = (s_app.settings.tijd_mode == TIJD_MODE_VERTREK) ? TIJD_MODE_AANKOMST : TIJD_MODE_VERTREK;
    prv_settings_apply();
  } else if (row == 2) {
    s_app.settings.vervoer_mode = (s_app.settings.vervoer_mode == VERVOER_LOPEN) ? VERVOER_FIETS : VERVOER_LOPEN;
    prv_settings_apply();
  } else if (row == 3) {
    s_app.routing.station_offset_min = prv_clamp_offset(s_app.routing.station_offset_min + 1);
    prv_settings_rebuild_items();
    if (s_settings_menu_layer) menu_layer_reload_data(s_settings_menu_layer);
    prv_send_offset_to_phone();
  }
}

static void prv_settings_select_long(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  (void)menu_layer; (void)context;
  if (cell_index->row != 3) return;
  s_app.routing.station_offset_min = prv_clamp_offset(s_app.routing.station_offset_min - 1);
  prv_settings_rebuild_items();
  if (s_settings_menu_layer) menu_layer_reload_data(s_settings_menu_layer);
  prv_send_offset_to_phone();
}



static void prv_settings_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  prv_settings_rebuild_items();
  s_settings_menu_layer = menu_layer_create(bounds);
  menu_layer_set_click_config_onto_window(s_settings_menu_layer, window);
  menu_layer_set_callbacks(s_settings_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_settings_num_rows,
    .draw_row = prv_settings_draw_row,
    .select_click = prv_settings_select,
    .select_long_click = prv_settings_select_long,
  });
#ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_settings_menu_layer, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(s_settings_menu_layer, GColorOxfordBlue, GColorWhite);
#endif
  layer_add_child(window_layer, menu_layer_get_layer(s_settings_menu_layer));
}

static void prv_settings_window_unload(Window *window) {
  (void)window;
  menu_layer_destroy(s_settings_menu_layer);
  s_settings_menu_layer = NULL;
  if (s_app.countdown_ui.over_time_layer) {
    prv_update_countdown_display();
  }
}

static void prv_cd_oom_pop(void *data) {
  (void)data;
  if (s_app.windows.countdown_window &&
      window_stack_contains_window(s_app.windows.countdown_window)) {
    window_stack_remove(s_app.windows.countdown_window, false);
  }
}

static bool prv_cd_alive(void *p) {
  if (p) return true;
  APP_LOG(APP_LOG_LEVEL_ERROR, "countdown load OOM heap=%d", (int)heap_bytes_free());
  app_timer_register(10, prv_cd_oom_pop, NULL);
  return false;
}

static void prv_countdown_window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "countdown_window_load: heap=%d", (int)heap_bytes_free());
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  const bool is_large_display = prv_is_large_display(bounds);
  const bool is_emery = (bounds.size.w == 200 && bounds.size.h == 228);
  const int top_bar = prv_chrome_bar(is_large_display);
  const int bot_bar = prv_chrome_bar(is_large_display);
  s_app.journey.selected_trip_index = 0;

  #ifdef PBL_COLOR
    s_app.countdown_ui.bg_blue_layer = layer_create(GRect(0, 0, bounds.size.w, top_bar));
    if (!prv_cd_alive(s_app.countdown_ui.bg_blue_layer)) return;
    if (s_app.countdown_ui.bg_blue_layer) layer_set_update_proc(s_app.countdown_ui.bg_blue_layer, prv_bg_blue_update_proc);
    if (s_app.countdown_ui.bg_blue_layer) layer_add_child(window_layer, s_app.countdown_ui.bg_blue_layer);
    s_app.countdown_ui.bg_blue_bottom_layer = layer_create(GRect(0, bounds.size.h - bot_bar, bounds.size.w, bot_bar));
    if (!prv_cd_alive(s_app.countdown_ui.bg_blue_bottom_layer)) return;
    if (s_app.countdown_ui.bg_blue_bottom_layer) layer_set_update_proc(s_app.countdown_ui.bg_blue_bottom_layer, prv_bg_blue_update_proc);
    if (s_app.countdown_ui.bg_blue_bottom_layer) layer_add_child(window_layer, s_app.countdown_ui.bg_blue_bottom_layer);
    s_app.countdown_ui.bg_yellow_layer = layer_create(GRect(0, top_bar, bounds.size.w, bounds.size.h - top_bar - bot_bar));
    if (!prv_cd_alive(s_app.countdown_ui.bg_yellow_layer)) return;
    if (s_app.countdown_ui.bg_yellow_layer) layer_set_update_proc(s_app.countdown_ui.bg_yellow_layer, prv_bg_yellow_update_proc);
    if (s_app.countdown_ui.bg_yellow_layer) layer_add_child(window_layer, s_app.countdown_ui.bg_yellow_layer);
  #else
    s_app.countdown_ui.bg_blue_layer = layer_create(GRect(0, 0, bounds.size.w, top_bar));
    if (!prv_cd_alive(s_app.countdown_ui.bg_blue_layer)) return;
    if (s_app.countdown_ui.bg_blue_layer) layer_set_update_proc(s_app.countdown_ui.bg_blue_layer, prv_bg_black_update_proc);
    if (s_app.countdown_ui.bg_blue_layer) layer_add_child(window_layer, s_app.countdown_ui.bg_blue_layer);
    s_app.countdown_ui.bg_blue_bottom_layer = layer_create(GRect(0, bounds.size.h - bot_bar, bounds.size.w, bot_bar));
    if (!prv_cd_alive(s_app.countdown_ui.bg_blue_bottom_layer)) return;
    if (s_app.countdown_ui.bg_blue_bottom_layer) layer_set_update_proc(s_app.countdown_ui.bg_blue_bottom_layer, prv_bg_black_update_proc);
    if (s_app.countdown_ui.bg_blue_bottom_layer) layer_add_child(window_layer, s_app.countdown_ui.bg_blue_bottom_layer);
  #endif

  s_app.countdown_ui.trip_leg_layer = layer_create(bounds);
  if (!prv_cd_alive(s_app.countdown_ui.trip_leg_layer)) return;
  if (s_app.countdown_ui.trip_leg_layer) layer_set_update_proc(s_app.countdown_ui.trip_leg_layer, prv_trip_leg_layer_update_proc);
  if (s_app.countdown_ui.trip_leg_layer) layer_add_child(window_layer, s_app.countdown_ui.trip_leg_layer);

  const int cream_top = top_bar;
  const int cream_h = bounds.size.h - top_bar - bot_bar;
  const int x_pad = PBL_IF_ROUND_ELSE(18, 4);
  
  int side_slot, time_w, bar_name_h, bar_name_y, bot_name_y, name_x, name_w;
  int bar_time_h, bar_time_y, bot_time_y;
  GFont chrome_time_font, chrome_name_font;
  
  if (is_emery) {
    bar_name_h = 20;
    bar_name_y = 1;
    bar_time_h = 18;
    bar_time_y = 20;
    bot_time_y = bounds.size.h - bot_bar;
    bot_name_y = bounds.size.h - bot_bar + 18;
    side_slot = 52;
    time_w = side_slot;
    name_x = 0;
    name_w = bounds.size.w;
    chrome_time_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    chrome_name_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  } else {
    side_slot = is_large_display ? ((bounds.size.w >= 180) ? 56 : 50) : 42;
    time_w = side_slot;
    bar_name_h = is_large_display ? 24 : 20;
    bar_name_y = (top_bar - bar_name_h) / 2 - (is_large_display ? 1 : 0);
    bot_name_y = bounds.size.h - bot_bar + ((bot_bar - bar_name_h) / 2) - 1;
    name_x = side_slot;
    name_w = bounds.size.w - 2 * side_slot;
    chrome_time_font = fonts_get_system_font(is_large_display ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14);
    chrome_name_font = fonts_get_system_font(
        (is_large_display && bounds.size.w >= 180) ? FONT_KEY_GOTHIC_24_BOLD :
        (is_large_display ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD));
    bar_time_h = bar_name_h;
    bar_time_y = bar_name_y;
    bot_time_y = bot_name_y;
  }

  const int plat_size = is_large_display ? 26 : 22;
  const int line_x = bounds.size.w - 12;
  const int plat_x = line_x - 6 - plat_size;
  const int plat_y = cream_top + 2;
  const int dur_h = is_emery ? 16 : 18;
  const int dur_y = cream_top;
  const int lab_h = is_large_display ? 14 : 10;
  const int lab_w = is_large_display ? 80 : 64;
  const int line_pad = 14;
  const int clock_w = bounds.size.w - x_pad - line_pad;
  int over_lab_y = dur_y + dur_h + (is_emery ? 4 : 0);
  int remain = cream_h - dur_h - 2 * lab_h - (is_emery ? 8 : 4);
  int over_clock_h = remain * 2 / 3;
  int vtk_clock_h = remain - over_clock_h;
  if (over_clock_h < (is_large_display ? 48 : 28)) over_clock_h = is_large_display ? 48 : 28;
  if (vtk_clock_h < (is_large_display ? 24 : 20)) vtk_clock_h = is_large_display ? 24 : 20;
  int over_clock_y = over_lab_y + lab_h + (is_large_display ? 2 : 1);
  int vtk_lab_y = over_clock_y + over_clock_h + 2;
  int vtk_clock_y = vtk_lab_y + lab_h;
  GFont lab_font = fonts_get_system_font(is_large_display ? FONT_KEY_GOTHIC_14 : FONT_KEY_GOTHIC_09);

  s_app.countdown_ui.clock_layer = text_layer_create(
      is_emery ? GRect(2, bar_time_y, time_w, bar_time_h) :
                 GRect(1, is_large_display ? -2 : 0, side_slot - 1, is_large_display ? 24 : 16));
  if (!prv_cd_alive(s_app.countdown_ui.clock_layer)) return;
  text_layer_set_font(s_app.countdown_ui.clock_layer, chrome_time_font);
  text_layer_set_text_alignment(s_app.countdown_ui.clock_layer, GTextAlignmentLeft);
  text_layer_set_background_color(s_app.countdown_ui.clock_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.clock_layer, GColorWhite);
  if (s_app.countdown_ui.clock_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.clock_layer));
  prv_refresh_bar_clock();

  s_app.countdown_ui.start_station_layer = text_layer_create(GRect(name_x, bar_name_y, name_w, bar_name_h));
  if (!prv_cd_alive(s_app.countdown_ui.start_station_layer)) return;
  text_layer_set_text(s_app.countdown_ui.start_station_layer, s_app.journey.start_station_name);
  text_layer_set_font(s_app.countdown_ui.start_station_layer, chrome_name_font);
  text_layer_set_text_alignment(s_app.countdown_ui.start_station_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_app.countdown_ui.start_station_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(s_app.countdown_ui.start_station_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.start_station_layer, GColorWhite);
  if (s_app.countdown_ui.start_station_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.start_station_layer));

  s_app.countdown_ui.departure_time_layer = text_layer_create(
      is_emery ? GRect(bounds.size.w - time_w - 2, bar_time_y, time_w, bar_time_h) :
                 GRect(bounds.size.w - time_w - 2, bar_name_y, time_w, bar_name_h));
  if (!prv_cd_alive(s_app.countdown_ui.departure_time_layer)) return;
  text_layer_set_font(s_app.countdown_ui.departure_time_layer, chrome_time_font);
  text_layer_set_text_alignment(s_app.countdown_ui.departure_time_layer, GTextAlignmentRight);
  text_layer_set_background_color(s_app.countdown_ui.departure_time_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.departure_time_layer, GColorWhite);
  if (s_app.countdown_ui.departure_time_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.departure_time_layer));

  s_app.countdown_ui.destination_layer = text_layer_create(GRect(name_x, bot_name_y, name_w, bar_name_h));
  if (!prv_cd_alive(s_app.countdown_ui.destination_layer)) return;
  text_layer_set_text(s_app.countdown_ui.destination_layer, s_app.journey.dest_station_name);
  text_layer_set_font(s_app.countdown_ui.destination_layer, chrome_name_font);
  text_layer_set_text_alignment(s_app.countdown_ui.destination_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_app.countdown_ui.destination_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(s_app.countdown_ui.destination_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.destination_layer, GColorWhite);
  if (s_app.countdown_ui.destination_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.destination_layer));

  s_app.countdown_ui.arrival_time_layer = text_layer_create(
      is_emery ? GRect(bounds.size.w - time_w - 2, bot_time_y, time_w, bar_time_h) :
                 GRect(bounds.size.w - time_w - 2, bot_name_y, time_w, bar_name_h));
  if (!prv_cd_alive(s_app.countdown_ui.arrival_time_layer)) return;
  text_layer_set_font(s_app.countdown_ui.arrival_time_layer, chrome_time_font);
  text_layer_set_text_alignment(s_app.countdown_ui.arrival_time_layer, GTextAlignmentRight);
  text_layer_set_background_color(s_app.countdown_ui.arrival_time_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.arrival_time_layer, GColorWhite);
  if (s_app.countdown_ui.arrival_time_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.arrival_time_layer));

  s_app.countdown_ui.duration_layer = text_layer_create(GRect(0, dur_y, bounds.size.w, dur_h));
  if (!prv_cd_alive(s_app.countdown_ui.duration_layer)) return;
  text_layer_set_font(s_app.countdown_ui.duration_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_app.countdown_ui.duration_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_app.countdown_ui.duration_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.duration_layer, GColorBlack);
  if (s_app.countdown_ui.duration_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.duration_layer));

  s_app.countdown_ui.platform_border_layer = layer_create(PBL_IF_ROUND_ELSE(GRect(118, plat_y, 24, 24), GRect(plat_x, plat_y, plat_size, plat_size)));
  if (!prv_cd_alive(s_app.countdown_ui.platform_border_layer)) return;
  if (s_app.countdown_ui.platform_border_layer) layer_set_update_proc(s_app.countdown_ui.platform_border_layer, prv_platform_border_update_proc);
  if (s_app.countdown_ui.platform_border_layer) layer_add_child(window_layer, s_app.countdown_ui.platform_border_layer);

  s_app.countdown_ui.platform_number_layer = text_layer_create(PBL_IF_ROUND_ELSE(GRect(120, plat_y + 2, 20, 24), GRect(plat_x + 1, plat_y + 1, plat_size - 2, plat_size - 2)));
  if (!prv_cd_alive(s_app.countdown_ui.platform_number_layer)) return;
  text_layer_set_font(s_app.countdown_ui.platform_number_layer, fonts_get_system_font(is_large_display ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_app.countdown_ui.platform_number_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_app.countdown_ui.platform_number_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.platform_number_layer, GColorBlack);
  if (s_app.countdown_ui.platform_number_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.platform_number_layer));

  s_app.countdown_ui.over_label_layer = text_layer_create(GRect(x_pad, over_lab_y, lab_w, lab_h));
  if (!prv_cd_alive(s_app.countdown_ui.over_label_layer)) return;
  text_layer_set_text(s_app.countdown_ui.over_label_layer, "OVER");
  text_layer_set_font(s_app.countdown_ui.over_label_layer, lab_font);
  text_layer_set_text_alignment(s_app.countdown_ui.over_label_layer, GTextAlignmentLeft);
  text_layer_set_background_color(s_app.countdown_ui.over_label_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.over_label_layer, GColorBlack);
  if (s_app.countdown_ui.over_label_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.over_label_layer));

  s_app.countdown_ui.over_time_layer = text_layer_create(GRect(x_pad, over_clock_y, clock_w, over_clock_h));
  if (!prv_cd_alive(s_app.countdown_ui.over_time_layer)) return;
  text_layer_set_text_alignment(s_app.countdown_ui.over_time_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_app.countdown_ui.over_time_layer, GTextOverflowModeFill);
  text_layer_set_background_color(s_app.countdown_ui.over_time_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.over_time_layer, GColorBlack);
  prv_set_over_text("--:--");
  if (s_app.countdown_ui.over_time_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.over_time_layer));

  s_app.countdown_ui.vertrek_label_layer = text_layer_create(GRect(x_pad, vtk_lab_y, lab_w, lab_h));
  if (!prv_cd_alive(s_app.countdown_ui.vertrek_label_layer)) return;
  text_layer_set_text(s_app.countdown_ui.vertrek_label_layer, "VERTREK");
  text_layer_set_font(s_app.countdown_ui.vertrek_label_layer, lab_font);
  text_layer_set_text_alignment(s_app.countdown_ui.vertrek_label_layer, GTextAlignmentLeft);
  text_layer_set_background_color(s_app.countdown_ui.vertrek_label_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.vertrek_label_layer, GColorBlack);
  if (s_app.countdown_ui.vertrek_label_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.vertrek_label_layer));

  s_app.countdown_ui.vertrek_time_layer = text_layer_create(GRect(x_pad, vtk_clock_y, clock_w - 58, vtk_clock_h));
  if (!prv_cd_alive(s_app.countdown_ui.vertrek_time_layer)) return;
  text_layer_set_text_alignment(s_app.countdown_ui.vertrek_time_layer, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_app.countdown_ui.vertrek_time_layer, GTextOverflowModeFill);
  text_layer_set_background_color(s_app.countdown_ui.vertrek_time_layer, GColorClear);
  text_layer_set_text_color(s_app.countdown_ui.vertrek_time_layer, GColorBlack);
  prv_set_clock_text(s_app.countdown_ui.vertrek_time_layer, "--:--");
  if (s_app.countdown_ui.vertrek_time_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.vertrek_time_layer));

  s_app.countdown_ui.delay_layer = text_layer_create(GRect(x_pad + clock_w - 56, vtk_clock_y + 2, 56, vtk_clock_h));
  if (!prv_cd_alive(s_app.countdown_ui.delay_layer)) return;
  text_layer_set_font(s_app.countdown_ui.delay_layer, fonts_get_system_font(is_large_display ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_app.countdown_ui.delay_layer, GTextAlignmentLeft);
  text_layer_set_background_color(s_app.countdown_ui.delay_layer, GColorClear);
#ifdef PBL_COLOR
  text_layer_set_text_color(s_app.countdown_ui.delay_layer, GColorRed);
#else
  text_layer_set_text_color(s_app.countdown_ui.delay_layer, GColorBlack);
#endif
  if (s_app.countdown_ui.delay_layer) layer_add_child(window_layer, text_layer_get_layer(s_app.countdown_ui.delay_layer));
  layer_set_hidden(text_layer_get_layer(s_app.countdown_ui.delay_layer), true);

  s_app.countdown_ui.time_arrow_layer = NULL;

#ifdef PBL_COLOR
  s_mid_band_color = GColorYellow;
#endif
  if (!s_app.countdown_ui.bg_blue_layer || !s_app.countdown_ui.bg_blue_bottom_layer ||
#ifdef PBL_COLOR
      !s_app.countdown_ui.bg_yellow_layer ||
#endif
      !s_app.countdown_ui.trip_leg_layer || !s_app.countdown_ui.clock_layer ||
      !s_app.countdown_ui.start_station_layer || !s_app.countdown_ui.destination_layer ||
      !s_app.countdown_ui.over_time_layer || !s_app.countdown_ui.vertrek_time_layer ||
      !s_app.countdown_ui.over_label_layer || !s_app.countdown_ui.vertrek_label_layer ||
      !s_app.countdown_ui.departure_time_layer || !s_app.countdown_ui.arrival_time_layer ||
      !s_app.countdown_ui.duration_layer || !s_app.countdown_ui.delay_layer ||
      !s_app.countdown_ui.platform_border_layer || !s_app.countdown_ui.platform_number_layer) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "countdown load OOM heap=%d", (int)heap_bytes_free());
    app_timer_register(10, prv_cd_oom_pop, NULL);
    return;
  }
  if (s_app.state.refresh_timer) {
    app_timer_cancel(s_app.state.refresh_timer);
  }
  s_app.state.refresh_in_flight = false;
  s_route_retry_left = 4;
  s_app.state.refresh_timer = app_timer_register(30000, prv_refresh_timer_callback, NULL);
  prv_send_route_request();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "countdown_window_load done heap=%d has_dest_layer=%d",
          (int)heap_bytes_free(), s_app.menu_layers.dest_menu_layer ? 1 : 0);
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
  if (s_app.state.deferred_menu_destroy_timer) {
    app_timer_cancel(s_app.state.deferred_menu_destroy_timer);
    s_app.state.deferred_menu_destroy_timer = NULL;
  }
  if (s_route_retry_timer) {
    app_timer_cancel(s_route_retry_timer);
    s_route_retry_timer = NULL;
  }
  s_app.state.refresh_in_flight = false;

  // Unschedule animation if running, but don't destroy (animations auto-free when complete)
  if (s_app.state.content_animation) {
    animation_unschedule((Animation*)s_app.state.content_animation);
    property_animation_destroy(s_app.state.content_animation);
    s_app.state.content_animation = NULL;
  }
  s_app.state.is_animating = false;

  if (s_app.countdown_ui.destination_layer) text_layer_destroy(s_app.countdown_ui.destination_layer);
  if (s_app.countdown_ui.start_station_layer) text_layer_destroy(s_app.countdown_ui.start_station_layer);
  if (s_app.countdown_ui.platform_border_layer) layer_destroy(s_app.countdown_ui.platform_border_layer);
  if (s_app.countdown_ui.platform_number_layer) text_layer_destroy(s_app.countdown_ui.platform_number_layer);
  if (s_app.countdown_ui.over_label_layer) text_layer_destroy(s_app.countdown_ui.over_label_layer);
  if (s_app.countdown_ui.over_time_layer) text_layer_destroy(s_app.countdown_ui.over_time_layer);
  if (s_app.countdown_ui.vertrek_label_layer) text_layer_destroy(s_app.countdown_ui.vertrek_label_layer);
  if (s_app.countdown_ui.vertrek_time_layer) text_layer_destroy(s_app.countdown_ui.vertrek_time_layer);
  if (s_app.countdown_ui.clock_layer) text_layer_destroy(s_app.countdown_ui.clock_layer);
  if (s_app.countdown_ui.departure_time_layer) text_layer_destroy(s_app.countdown_ui.departure_time_layer);
  if (s_app.countdown_ui.time_arrow_layer) text_layer_destroy(s_app.countdown_ui.time_arrow_layer);
  if (s_app.countdown_ui.arrival_time_layer) text_layer_destroy(s_app.countdown_ui.arrival_time_layer);
  if (s_app.countdown_ui.delay_layer) text_layer_destroy(s_app.countdown_ui.delay_layer);
  if (s_app.countdown_ui.duration_layer) text_layer_destroy(s_app.countdown_ui.duration_layer);
  if (s_app.countdown_ui.trip_leg_layer) layer_destroy(s_app.countdown_ui.trip_leg_layer);
  if (s_app.countdown_ui.bg_blue_layer) layer_destroy(s_app.countdown_ui.bg_blue_layer);
  if (s_app.countdown_ui.bg_blue_bottom_layer) layer_destroy(s_app.countdown_ui.bg_blue_bottom_layer);
#ifdef PBL_COLOR
  if (s_app.countdown_ui.bg_yellow_layer) layer_destroy(s_app.countdown_ui.bg_yellow_layer);
#endif
  memset(&s_app.countdown_ui, 0, sizeof(s_app.countdown_ui));
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}