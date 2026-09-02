# Changelog

All notable changes to the Trein Pebble app will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.7.4] - 02-09-2026

### Changed

- Countdown cream is taller: thinner bottom (and tighter top) blue bars.
- Planned departure HH:MM sits top-right in the origin blue bar; arrival HH:MM bottom-right in the destination bar. Station names stay left with trailing ellipsis.
- Cream no longer shows the `18:04 > 19:25` row. Total trip time (`1u21` / `48m`) is small and centered at the top of the cream; platform stays top-right.
- OVER sits higher. Compact OVER / VERTREK labels on the left, LECO clocks using the rest of the row (42 on flint / Time 2 even in 144x168 qemu).
- VERTREK counts down to the original/planned departure. Delay is red beside it (`+N`); after planned time it ticks remaining delay. On time hides the slot. Auto-exit is still 3 minutes after actual departure.
- OVER always shows a departure countdown (actual remain, or leave-by slack when ORS duration exists). Routing failures no longer replace the clock with ORS? / geen route / ...
- Phone sends `TRIP_PLANNED_DEPARTURE_TIME_EPOCH` and `TRIP_ORIGIN_ARRIVAL_EPOCH` with each trip.
- Watch settings: ORS ja/nee first, then Tijd, Vervoer, then VAN slack (select +1, long-press -1). Default offset is 2 minutes.
- ORS = Nee uses a single hero clock (VERTREK to planned departure, or AANKOMST to origin-arrival). ORS = Ja keeps OVER + VERTREK.
- Phone sends TRIP_ORIGIN_ARRIVAL_EPOCH for the train arriving at VAN. Long-press a station to favourite it.
- Switching lopen/fiets invalidates the ORS cache and re-routes immediately.


## [1.7.3] - 02-09-2026

### Changed

- Countdown layout matches the OVER / VERTREK mockup: OVER at the top of the cream band, large LECO timer, VERTREK stacked under it, status small beside VERTREK. Flint / Time 2 uses the larger type when the screen is bigger than basalt.
- OpenRouteService uses POST JSON to api.heigit.org with an Authorization header (no GET query key).

### Fixed

- LECO blobs and OVER overlapping VERTREK on Pebble Time 2.
- Cancelled trains show GEANNULEERD (Gothic) on OVER with a red band instead of LECO dashes.
- ORS failures no longer leave OVER as infinite ...; the watch shows geen route (no key) or ORS? (HTTP/parse). VERTREK still updates from NS.
- API keys are trimmed when saved and read.

## [1.7.2] - 02-09-2026

### Added

- Instant "ritten laden" screen after choosing a destination (station bars + spinner) so the watch responds before NS returns.

### Changed

- OpenRouteService directions now use `api.heigit.org/openrouteservice` (the old `api.openrouteservice.org` host is deprecated).

### Fixed

- Phone settings is a light form (favourites and station offsets as rows, not JSON textareas). Dark WebView no longer hides labels.
- Selecting a destination no longer crashes: `STATION_OFFSET` is not treated as a trip failure, route ticks do not double-fetch trips, and AppMessage strings are never NULL.

## [1.7.1] - 02-09-2026

### Fixed

- Phone settings page now loads as HTML via GitHub Pages. jsDelivr and GitHub raw both serve `config.html` as `text/plain`, which Core Devices WebView shows as source.

## [1.7.0] - 18-05-2026

### Added

- Timeline pinning: press SELECT on a leg in the journey details screen to pin that leg or the entire journey to the Pebble Timeline
- A short vibration confirms a successful pin; a long vibration indicates failure

## [1.6.0] - 03-02-2026

### Changed

- Favourites now only appear in the destination station menu (not the departure station menu)
- This improves usability as the departure station is typically the one you're at
- Centered section headers on round screens (Chalk) for better readability

### Fixed

- Stations that are in your favourites now correctly appear in the nearby stations list

## [1.5.0] - 01-02-2026

### Added

- Favourite stations feature: save up to 5 favourite stations via the settings page
- Favourites appear at the top of destination station menu
- Section headers in station menus (Favourites, Top Stations, By Letter)
- Duplicate filtering: favourite stations that are also nearby won't appear twice

## [1.4.0] - 29-01-2026

### Added

- Journey details screen showing leg information for multi-transfer trips

### Fixed

- Arrow character not rendering on non-Aplite platforms

## [1.3.0] - 26-10-2025

### Added
- Animated station clock spinner on startup/loading screen
- Clock animation with rotating hour and minute hands showing loading progress
- Centered clock display taking up exactly half the screen for maximum visibility
- White text in bottom bar for loading status message
- Responsive clock scaling across all Pebble platforms (Aplite, Basalt, Chalk, Diorite, Emery)

## [1.2.0] - 25-10-2025

### Added
- Clock display on countdown screen showing current time at the top center
- Dutch railway-style platform indicator with white background, blue border, and small blue square in top-left corner
- Arrow indicator between departure and arrival times for better visual clarity
- Black bars on top and bottom for non-color displays (Aplite and Diorite)
- Better support for Emery (still not perfect)

## [1.1.0] - 25-10-2025

### Fixed
- Second menu for selecting destination didn't group by first letter correctly
- Stations starting with the Dutch article "De" are now sorted in the Dutch way (sort by the first letter after the word "De")
- Delay not calculated properly

## [1.0.0] - 22-10-2025

### Added
- Initial public release
- Real-time train departure information from NS API
- Countdown timer to next train
- Platform information display
- Delay notifications
- Automatic nearby station detection using GPS location
- Configuration page for NS API key setup
- Support for all Pebble platforms (Aplite, Basalt, Chalk, Diorite, Emery, Flint)
- Station selection interface
- Trip planning between two stations
- Multi-transfer journey support

### Features
- Integration with NS Gateway API Portal
- Location-based station discovery
- Configurable API key storage
- Real-time departure and arrival times
- Platform change notifications
- Emulator support with mock data

---

## Future Plans

### Planned Features
- Updated information about delays (currently we only get the delay at the time loading in the journeys)

### Known Issues
- None at this time
