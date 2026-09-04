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

#pragma once
#include <pebble.h>

// --- Constants ---
#define MAX_STATIONS 8
#define MAX_STATION_NAME_LENGTH 32
#define MAX_STATION_CODE_LENGTH 5
#define MAX_TRIPS 5
#define MAX_DATE_TIME_LENGTH 6
#define MAX_TRANSFERS_LENGTH 3
#define MAX_PLATFORM_LENGTH 3
#define MAX_DELAY_LENGTH 10
#define MAX_LEGS 4
#define MAX_LEG_STATION_LENGTH 16

// --- Data Structures ---

// Leg Data (individual journey leg information)
#define MAX_LEG_TIME_LENGTH 6
#define MAX_LEG_DURATION_LENGTH 8

typedef struct {
  char departure_station[MAX_LEG_STATION_LENGTH];
  char departure_platform[MAX_PLATFORM_LENGTH];
  char departure_time[MAX_LEG_TIME_LENGTH];
  char arrival_station[MAX_LEG_STATION_LENGTH];
  char arrival_time[MAX_LEG_TIME_LENGTH];
  char duration[MAX_LEG_DURATION_LENGTH];
  uint32_t dep_epoch;
  uint32_t arr_epoch;
} LegData;

// Trip Legs Data (all legs for a trip)
typedef struct {
  LegData legs[MAX_LEGS];
  int leg_count;
} TripLegsData;

// UI Window Components
typedef struct {
  Window *main_window;
  Window *menu_window;
  Window *dest_menu_window;
  Window *alpha_menu_window;
  Window *countdown_window;
  Window *journey_details_window;
  Window *pin_menu_window;
  Window *settings_window;
} AppWindows;

// Menu Layer Components
typedef struct {
  MenuLayer *menu_layer;
  MenuLayer *dest_menu_layer;
  MenuLayer *alpha_menu_layer;
} AppMenuLayers;

// Main Window Text Layers
typedef struct {
  TextLayer *text_layer;
  Layer *bg_blue_layer;
  Layer *bg_blue_bottom_layer;
  Layer *spinner_layer;
  #ifdef PBL_COLOR
  Layer *bg_yellow_layer;
  #endif
} MainWindowUI;

/* Dest-window overlay: full blue screen with title, journey, status, rotating arc. */
typedef struct {
  Layer *bg_layer;
  TextLayer *title_layer;
  TextLayer *journey_layer;
  TextLayer *status_layer;
  Layer *spinner_layer;
} TripsLoadingUI;

// Countdown Window UI Components
typedef struct {
  TextLayer *platform_number_layer;
  Layer *platform_border_layer;
  TextLayer *over_label_layer;
  TextLayer *over_time_layer;
  TextLayer *vertrek_label_layer;
  TextLayer *vertrek_time_layer;
  TextLayer *start_station_layer;
  TextLayer *destination_layer;
  TextLayer *departure_time_layer;
  TextLayer *time_arrow_layer;
  TextLayer *arrival_time_layer;
  TextLayer *delay_layer;
  TextLayer *clock_layer;
  TextLayer *duration_layer;
  Layer *trip_leg_layer;
  Layer *bg_blue_layer;
  Layer *bg_blue_bottom_layer;
  #ifdef PBL_COLOR
  Layer *bg_yellow_layer;
  #endif
} CountdownWindowUI;

// Journey Details Window UI Components
typedef struct {
  MenuLayer *menu_layer;
  Layer *bg_blue_layer;
  Layer *bg_blue_bottom_layer;
  #ifdef PBL_COLOR
  Layer *bg_yellow_layer;
  #endif
} JourneyDetailsUI;

// Display Buffers for Countdown Window
typedef struct {
  char platform_buffer[32];
  char over_buffer[16];
  char vertrek_buffer[16];
  char departure_time_buffer[6];
  char arrival_time_buffer[6];
  char delay_buffer[16];
  char clock_buffer[6];
  char duration_buffer[16];
  char section_header[16];
  char letter_str[2];
} DisplayBuffers;

// Station Data (nearby stations from API)
typedef struct {
  char names[MAX_STATIONS][MAX_STATION_NAME_LENGTH];
  char codes[MAX_STATIONS][MAX_STATION_CODE_LENGTH];
  int count;
  bool loaded;
} StationData;

// Favourite Stations Data
#define MAX_FAVOURITES 5

typedef struct {
  char names[MAX_FAVOURITES][MAX_STATION_NAME_LENGTH];
  char codes[MAX_FAVOURITES][MAX_STATION_CODE_LENGTH];
  int count;
  bool loaded;
} FavouriteData;

// Trip Data (journey information)
typedef struct {
  char planned_departures[MAX_TRIPS][MAX_DATE_TIME_LENGTH];
  int departures[MAX_TRIPS];  // Unix epoch timestamps
  char planned_arrivals[MAX_TRIPS][MAX_DATE_TIME_LENGTH];
  char arrivals[MAX_TRIPS][MAX_DATE_TIME_LENGTH];
  char transfers[MAX_TRIPS][MAX_TRANSFERS_LENGTH];
  char platform[MAX_TRIPS][MAX_PLATFORM_LENGTH];
  char delay[MAX_TRIPS][MAX_DELAY_LENGTH];
  int arrivals_epoch[MAX_TRIPS];
  int planned_departures_epoch[MAX_TRIPS];
  int origin_arrivals_epoch[MAX_TRIPS];
  int count;
  uint8_t received_mask;
  bool loaded;
} TripData;

// Selected Journey Information
typedef struct {
  char start_station_code[5];
  char start_station_name[MAX_STATION_NAME_LENGTH];
  char dest_station_code[5];
  char dest_station_name[MAX_STATION_NAME_LENGTH];
  int selected_trip_index;
} SelectedJourney;

#define PERSIST_TIJD 1
#define PERSIST_REISTIJD 2
#define PERSIST_VERVOER 3

typedef enum { TIJD_MODE_VERTREK = 0, TIJD_MODE_AANKOMST = 1 } TijdMode;
typedef enum { VERVOER_LOPEN = 0, VERVOER_FIETS = 1 } VervoerMode;

typedef struct {
  TijdMode tijd_mode;
  bool reistijd_enabled;
  VervoerMode vervoer_mode;
} UserSettings;

typedef struct {
  int travel_duration_min;
  int station_offset_min;
  bool have_duration;
  bool at_station;
  uint8_t route_error; /* 0=ok, 1=no ORS key, 2=http/parse fail */
} RoutingState;

// Animation Direction
typedef enum {
  ANIMATION_DIRECTION_UP = -1,
  ANIMATION_DIRECTION_DOWN = 1
} AnimationDirection;

// App State
typedef struct {
  int last_selected_index;
  int selected_alphabet_index;
  time_t departure_time;
  AppTimer *countdown_timer;
  AppTimer *clock_timer;
  AppTimer *fallback_timer;
  AppTimer *spinner_timer;
  AppTimer *refresh_timer;
  PropertyAnimation *content_animation;
  bool is_animating;
  AnimationDirection animation_direction;
  int spinner_angle;
  bool refresh_in_flight;
  time_t last_button_time;
  bool selecting_start_station;
  AppTimer *loading_fail_timer;
  AppTimer *loading_show_timer;
  AppTimer *pop_stations_timer;
  AppTimer *deferred_menu_destroy_timer;
} AppState;

// --- Global App Data Instance ---
typedef struct {
  AppWindows windows;
  AppMenuLayers menu_layers;
  MainWindowUI main_ui;
  TripsLoadingUI loading_ui;
  CountdownWindowUI countdown_ui;
  JourneyDetailsUI journey_details_ui;
  DisplayBuffers buffers;
  StationData stations;
  FavouriteData favourites;
  TripData trips;
  TripLegsData trip_legs[MAX_TRIPS];
  SelectedJourney journey;
  UserSettings settings;
  RoutingState routing;
  AppState state;
} AppData;
