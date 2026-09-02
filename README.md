# Trein (Stef)

Stef's personal fork of the Trein Pebble app with enhanced UX improvements.

A Pebble smartwatch app that shows live train information from Nederlandse Spoorwegen (Dutch Railways), including countdowns to your next train, departure times, and platform information.

**Fork-specific improvements:**
- **Location fallback**: If geolocation fails or times out, the app automatically falls back to manual station selection instead of requiring an app restart
- **Loading indicators**: Clear loading UI with animated spinner when fetching trip data, so slow API responses are distinguishable from dropped taps
- **OVER/VERTREK commute layout**: Smart countdown screen showing both "time until you must leave" (OVER) and "time until train departs" (VERTREK), with color-coded background (green/yellow/red) based on available slack time
- **Real-time routing**: Integrated OpenRouteService routing for walking/cycling directions to the departure station, with automatic GPS-based re-routing when you move
- **30-second refresh cycle**: Automatically refreshes NS delay information AND GPS position every 30 seconds while viewing a trip
- **On-station detection**: When you arrive at the station (within ~150m), routing stops and the display switches to departure-focused layout
- **Long-press settings**: Long-press SELECT on countdown screen to access watch-based settings (Tijd mode, Reistijd on/off, Vervoer walk/bike)
- **Auto-exit to watchface**: After the train departs and 3 minutes of inactivity, automatically returns to watchface
- **Vibration alerts**: Gentle vibration when NS updates the actual departure time due to delay changes
- **Increased API timeout**: Extended NS API timeout from 2s to 8s for more reliable operation on slower connections

**Original project**: https://github.com/guusbeckett/trein-pebble

## Features

- Real-time train departure information
- Countdown timer to your next train
- Platform information and delays
- Automatic station detection based on your location
- Journey details screen with leg-by-leg breakdown for multi-transfer trips
- Favourite stations: save up to 5 favourite stations for quick access when selecting your destination
- Timeline pinning: pin a single leg or your entire journey to the Pebble Timeline
- Optimized UI for round screens (Pebble Time Round)
- Support for all Pebble models (Aplite, Basalt, Chalk, Diorite, Emery, Flint)

## Prerequisites

To use this app, you need:

1. A Pebble smartwatch
2. The Pebble app installed on your smartphone
3. **An NS API key** from the [NS API Portal](https://apiportal.ns.nl/) (required)
4. **An OpenRouteService API key** from [OpenRouteService](https://openrouteservice.org/dev/#/signup) (optional but recommended for OVER/VERTREK routing features)

## Installation

1. Install the app on your Pebble watch through the Pebble app store or by sideloading
2. **Configure your NS API key** (required for the app to work):
   - Open the Pebble app on your phone
   - Navigate to Settings → My Pebble Apps → Trein
   - Tap on "Settings"
   - Enter your NS API key in the configuration page
   - Save the settings

### Getting an NS API Key

1. Visit the [NS API Portal](https://apiportal.ns.nl/)
2. Create an account or log in
3. Subscribe to the required APIs (NS App Stations API and Reisinformatie API)
4. Generate an API key
5. Copy the key and paste it in the app settings

### Getting an OpenRouteService API Key (Optional)

The OVER/VERTREK routing features require an OpenRouteService API key for real walking/cycling directions to your departure station. Without this key, the OVER timer will show a GPS spinner and routing features will be disabled (VERTREK timer still works).

1. Visit [OpenRouteService](https://openrouteservice.org/dev/#/signup)
2. Create a free account
3. Request a free API token (Standard plan: 2000 requests/day)
4. Copy the API key and paste it in the app settings under "Routing API Key"

**Important**: This fork uses ONLY real routed walking/cycling directions via OpenRouteService. No fallback calculations are performed. The free tier is sufficient for personal use with the 30-second refresh cycle.

## Usage

1. Open the Trein app on your Pebble watch
2. The app will automatically detect nearby train stations using your location
3. Select your departure and destination stations
4. View upcoming trains with departure times, platforms, and delay information
5. Press SELECT on the countdown screen to view journey details with each leg
6. Press SELECT on a leg to pin it (or the whole journey) to the Pebble Timeline
7. Use the countdown timer to see exactly how much time you have before your next train, maybe you can still grab a drink at AH To Go!

### OVER/VERTREK Layout Features

The countdown screen shows two timers:

- **OVER (large)**: Time until you must leave to catch the train, accounting for walking/cycling time + per-station buffer
- **VERTREK (small)**: Countdown to actual (delayed) train departure

**Background colors** (on color Pebbles):
- **Green**: More than 2 minutes of slack time
- **Yellow**: 0-2 minutes of slack
- **Red**: Negative slack (running late)

**On-station behavior**: When GPS detects you're at the departure station (~150m radius), the OVER timer hides and VERTREK becomes the hero countdown.

**Settings** (long-press SELECT on countdown screen):
- **Tijd**: Choose between Vertrek (departure) or Aankomst (arrival) countdown mode
- **Reistijd**: Toggle travel time calculation on/off
- **Vervoer**: Choose between Lopen (walking) or Fiets (cycling) for route calculations

**Automatic updates**:
- Every 30 seconds: refreshes NS delay info AND GPS position
- Routing recalculates only when you move >80 meters
- Vibrates once when NS changes the actual departure time
- Auto-exits to watchface 3 minutes after train departs (if no button presses)

### Advanced Configuration

**Per-station offsets** (configured via phone settings):
- Add extra buffer time (positive) or reduce time (negative) for specific stations
- Keyed by station code (e.g., "Ut", "Asd", "Rtd")
- Example: Add 2 minutes for Utrecht Centraal because of the long platforms

**Phone config page** (if you create your own):
- The app expects config parameters: `api_key`, `routing_api_key`, `station_offsets`, `favourites`
- Config URL (this fork): `https://sberkers.github.io/trein-pebble/config.html` (GitHub Pages, `text/html`)
- Original config URL: `https://guusbeckett.github.io/config.html`
- For testing, you can set values directly in localStorage via browser console

## Development

### Building from Source

This is a Pebble SDK 3 project. First install the [Pebble SDK](https://developer.repebble.com/sdk/)

To build:

```bash
pebble build
```

### Sideloading onto Pebble Time 2

To install the built app onto your Pebble watch:

1. Ensure your phone and watch are connected
2. Enable Developer Connection in the Pebble mobile app (Settings → Developer Mode)
3. Run:

```bash
pebble install --phone <your-phone-ip>
```

Or use the Pebble mobile app to install the `.pbw` file from `build/` directory.

### Testing

The app can be tested with the Pebble emulator:

```bash
pebble install --emulator flint
```

For full functionality testing (GPS, routing, NS API), you must use a physical device with:
- Valid NS API key configured
- Valid OpenRouteService API key (optional, for routing features)
- Location services enabled on phone

### Project Structure

```
trein-pebble/
├── src/
│   ├── c/           # Native C code for the watch app
│   └── pkjs/        # JavaScript code for phone communication
├── resources/       # App resources (icons, etc.)
├── package.json     # Project configuration
└── README.md
```

### Requirements

- Pebble SDK 3.x
- Node.js (for building)

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.

See the source code headers for full license information.

## Author

Guus H. Beckett

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## Acknowledgments

- Eric Migicovsky for bringing back Pebble
- Nederlandse Spoorwegen (NS) for providing the API
- The Pebble developer community

## Support

If you encounter any issues or have questions:
- Check that your NS API key is correctly configured in the app settings
- Ensure your phone has location services enabled
- Verify that you have an active internet connection

For bugs and feature requests, please open an issue on GitHub.

---

[Nederlandse versie](README.nl.md)
