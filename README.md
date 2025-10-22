# Trein

A Pebble smartwatch app that shows live train information from Nederlandse Spoorwegen (Dutch Railways), including countdowns to your next train, departure times, and platform information.

## Features

- Real-time train departure information
- Countdown timer to your next train
- Platform information and delays
- Automatic station detection based on your location
- Support for all Pebble models (Aplite, Basalt, Chalk, Diorite, Emery, Flint)

## Prerequisites

To use this app, you need:

1. A Pebble smartwatch
2. The Pebble app installed on your smartphone
3. **An NS API key** from the [NS API Portal](https://apiportal.ns.nl/)

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

## Usage

1. Open the Trein app on your Pebble watch
2. The app will automatically detect nearby train stations using your location
3. Select your departure and destination stations
4. View upcoming trains with departure times, platforms, and delay information
5. Use the countdown timer to see exactly how much time you have before your next train, maybe you can still grab a drink at AH To Go!

## Development

### Building from Source

This is a Pebble SDK 3 project. To build:

```bash
pebble build
```

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
