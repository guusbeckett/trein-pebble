# Changelog

All notable changes to the Trein Pebble app will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 25-10-2025

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
- Favorite routes storage
- Updated information about delays (currently we only get the delay at the time of getting all possible journey)
- Information about the transfers during your train journey 

### Known Issues
- None at this time
