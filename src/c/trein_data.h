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
#define MAX_DATE_TIME_LENGTH 25
#define MAX_TRANSFERS_LENGTH 3
#define MAX_PLATFORM_LENGTH 3
#define MAX_DELAY_LENGTH 10

// --- Data Structures ---

// UI Window Components
typedef struct {
  Window *main_window;
  Window *menu_window;
  Window *dest_menu_window;
  Window *alpha_menu_window;
  Window *countdown_window;
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
  #ifdef PBL_COLOR
  Layer *bg_blue_layer;
  Layer *bg_yellow_layer;
  Layer *bg_blue_bottom_layer;
  #endif
} MainWindowUI;

// Countdown Window UI Components
typedef struct {
  TextLayer *platform_layer;
  TextLayer *countdown_layer;
  TextLayer *start_station_layer;
  TextLayer *destination_layer;
  TextLayer *departure_time_layer;
  TextLayer *arrival_time_layer;
  TextLayer *delay_layer;
  TextLayer *clock_layer;
  Layer *trip_leg_layer;
  #ifdef PBL_COLOR
  Layer *bg_blue_layer;
  Layer *bg_yellow_layer;
  Layer *bg_blue_bottom_layer;
  #endif
} CountdownWindowUI;

// Display Buffers for Countdown Window
typedef struct {
  char platform_buffer[32];
  char countdown_buffer[16];
  char departure_time_buffer[6];
  char arrival_time_buffer[6];
  char delay_buffer[10];
  char clock_buffer[6];
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

// Trip Data (journey information)
typedef struct {
  char planned_departures[MAX_TRIPS][MAX_DATE_TIME_LENGTH];
  int departures[MAX_TRIPS];  // Unix epoch timestamps
  char planned_arrivals[MAX_TRIPS][MAX_DATE_TIME_LENGTH];
  char arrivals[MAX_TRIPS][MAX_DATE_TIME_LENGTH];
  char transfers[MAX_TRIPS][MAX_TRANSFERS_LENGTH];
  char platform[MAX_TRIPS][MAX_PLATFORM_LENGTH];
  char delay[MAX_TRIPS][MAX_DELAY_LENGTH];
  int count;
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
  PropertyAnimation *content_animation;
  bool is_animating;
  AnimationDirection animation_direction;
} AppState;

// --- Global App Data Instance ---
typedef struct {
  AppWindows windows;
  AppMenuLayers menu_layers;
  MainWindowUI main_ui;
  CountdownWindowUI countdown_ui;
  DisplayBuffers buffers;
  StationData stations;
  TripData trips;
  SelectedJourney journey;
  AppState state;
} AppData;
