# Changelog

All notable changes to the Trein Pebble app will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
- Information about the transfers during your train journey 

### Known Issues
- None at this time
