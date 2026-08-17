//
// * This file is part of the Trein Pebble app distribution (https://github.com/guusbeckett/trein-pebble).
// * Copyright (c) 2025 Guus Beckett.
// * 
// * This program is free software: you can redistribute it and/or modify  
// * it under the terms of the GNU General Public License as published by  
// * the Free Software Foundation, version 3.
// *
// * This program is distributed in the hope that it will be useful, but 
// * WITHOUT ANY WARRANTY; without even the implied warranty of 
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
// * General Public License for more details.
// *
// * You should have received a copy of the GNU General Public License 
// * along with this program. If not, see <http://www.gnu.org/licenses/>.
//
var DataSources = require("./data_sources");
var ConfigPage = require("./config_page");
var DEFAULT_API_KEY = "";
var DEFAULT_DATA_SOURCE = "ns";

function getApiKey() {
  try {
    var key = localStorage.getItem("api_key");
    if (key) {
      return key;
    }
  } catch (e) {
    console.log("Error reading from localStorage: " + e);
  }
  return DEFAULT_API_KEY;
}

function getDataSourceId() {
  try {
    var id = localStorage.getItem("data_source");
    if (DataSources.ids.indexOf(id) !== -1) {
      return id;
    }
  } catch (e) {
    console.log("Error reading data source from localStorage: " + e);
  }
  return DEFAULT_DATA_SOURCE;
}

function getDataSource() {
  return DataSources.create(getDataSourceId(), getApiKey);
}

function getFavourites() {
  try {
    var favourites = localStorage.getItem("favourites");
    if (favourites) {
      return JSON.parse(favourites);
    }
  } catch (e) {
    console.log("Error reading favourites from localStorage: " + e);
  }
  return [];
}

function sendFavouritesToWatch() {
  var favourites = getFavourites();
  if (favourites.length === 0) {
    Pebble.sendAppMessage({
      "DATA_SOURCE": getDataSourceId() === "irail" ? 1 : 0,
      "FAVOURITE_COUNT": 0
    });
    return;
  }

  var sendIndex = 0;

  function sendNextFavourite() {
    if (sendIndex >= favourites.length) {
      return;
    }

    var fav = favourites[sendIndex];
    var currentIndex = sendIndex;

    Pebble.sendAppMessage({
      "DATA_SOURCE": getDataSourceId() === "irail" ? 1 : 0,
      "FAVOURITE_INDEX": currentIndex,
      "FAVOURITE_CODE": fav.code,
      "FAVOURITE_NAME": fav.name,
      "FAVOURITE_COUNT": favourites.length
    }, function() {
      console.log("Favourite sent: " + fav.name);
      sendIndex++;
      setTimeout(sendNextFavourite, 100);
    }, function(e) {
      console.log("Failed to send favourite: " + e.error.message);
      sendIndex++;
      setTimeout(sendNextFavourite, 200);
    });
  }

  sendNextFavourite();
}

Pebble.addEventListener("showConfiguration", function(e) {
  var html = ConfigPage.render({
    api_key: getApiKey(),
    data_source: getDataSourceId(),
    favourites: getFavourites()
  });
  Pebble.openURL("data:text/html;charset=utf-8," + encodeURIComponent(html));
});

Pebble.addEventListener("webviewclosed", function(e) {
  if (!e.response) {
    return;
  }

  var settings;
  try {
    settings = JSON.parse(decodeURIComponent(e.response));
    if (Object.prototype.hasOwnProperty.call(settings, "api_key")) {
      localStorage.setItem("api_key", settings.api_key || "");
      console.log("Saved new API key.");
    }
    if (DataSources.ids.indexOf(settings.data_source) !== -1) {
      localStorage.setItem("data_source", settings.data_source);
      console.log("Saved data source: " + settings.data_source);
    }
    if (settings.favourites instanceof Array) {
      localStorage.setItem("favourites", JSON.stringify(settings.favourites));
      console.log("Saved favourites: " + settings.favourites.length);
      sendFavouritesToWatch();
      requestLocationAndFetchStations();
    }
  } catch (err) {
    console.log("Error parsing settings: " + err);
  }
});


Pebble.addEventListener("ready", function(e) {
  console.log("PebbleKit JS ready!");
  sendFavouritesToWatch();
  requestLocationAndFetchStations();
});

Pebble.addEventListener("appmessage", function(e) {
  if (e.payload.REQUEST_STATIONS) {
    requestLocationAndFetchStations();
  }

  if (e.payload.START_STATION_CODE && e.payload.DEST_STATION_CODE) {
    var startCode = e.payload.START_STATION_CODE;
    var destCode = e.payload.DEST_STATION_CODE;
    requestTrips(startCode, destCode);
  }

  if (e.payload.REQUEST_PIN) {
    pinToTimeline(e.payload);
  }
});

function requestLocationAndFetchStations() {  
  // Use mock data in the emulator
  if (typeof Pebble !== "undefined" && Pebble.platform === "pypkjs") {
    console.log("Emulator detected - using mock Breda location");
    var mockPos = {
      coords: {
        latitude: 51.58719,
        longitude: 4.78322
      }
    };
    locationSuccess(mockPos);
    return;
  }
  
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {
      timeout: 10000,
      maximumAge: 60000,
      enableHighAccuracy: false
    }
  );
}

function locationSuccess(pos) {
  var lat = pos.coords.latitude;
  var lng = pos.coords.longitude;
  fetchNearbyStations(lat, lng);
}

function locationError(err) {
  console.log("Location error: " + err.message);
  console.log("Error code: " + err.code);
  
  Pebble.sendAppMessage({
    "ERROR": 1
  });
}

function convertIsoDateToEpoch(apiDateString) {
  if (!apiDateString || typeof apiDateString !== 'string') {
    return 0;
  }

  let compliantString = apiDateString.slice(0, -2) + ":" + apiDateString.slice(-2);
  
  let dateObject = new Date(compliantString);
  
  if (isNaN(dateObject)) {
    return 0;
  }

  return Math.round(dateObject.getTime() / 1000);
}

function processStationData(nearbyStations) {
  if (!nearbyStations || nearbyStations.length === 0) {
    console.log("No stations found");
    Pebble.sendAppMessage({
      "ERROR": 1
    });
    return;
  }

  console.log("Processing " + nearbyStations.length + " nearby stations");

  var sendIndex = 0;

  function sendNextStation() {
    if (sendIndex >= nearbyStations.length) {
      return;
    }

    var stationName = nearbyStations[sendIndex].name;
    var stationCode = nearbyStations[sendIndex].code;
    var currentIndex = sendIndex;

    Pebble.sendAppMessage({
      "STATION_INDEX": currentIndex,
      "STATION_CODE": stationCode,
      "STATION_NAME": stationName,
      "STATION_COUNT": nearbyStations.length
    }, function() {
      console.log("Message sent successfully");
      sendIndex++;
      setTimeout(sendNextStation, 100);
    }, function(e) {
      console.log("Failed to send message: " + e.error.message);
      sendIndex++;
      setTimeout(sendNextStation, 200);
    });
  }

  sendNextStation();
}

function abbreviateStation(name) {
  if (!name) return "?";
  if (name.length <= 15) return name;
  name = name.replace("Centraal", "C");
  name = name.replace("Noord", "N");
  name = name.replace("Zuid", "Z");
  name = name.replace("Oost", "O");
  name = name.replace("West", "W");
  if (name.length <= 15) return name;
  return name.substring(0, 14) + ".";
}

function extractTime(dateTimeString) {
  if (!dateTimeString) return "?";
  // Format is like "2025-01-27T12:34:00+0100", extract HH:MM
  var match = dateTimeString.match(/T(\d{2}:\d{2})/);
  return match ? match[1] : "?";
}

function calculateDuration(departureDateTime, arrivalDateTime) {
  if (!departureDateTime || !arrivalDateTime) return "?";
  var depTime = Date.parse(departureDateTime.slice(0, -2) + ":" + departureDateTime.slice(-2));
  var arrTime = Date.parse(arrivalDateTime.slice(0, -2) + ":" + arrivalDateTime.slice(-2));
  if (isNaN(depTime) || isNaN(arrTime)) return "?";
  var diffMinutes = Math.round((arrTime - depTime) / 60000);
  if (diffMinutes < 60) {
    return diffMinutes + "m";
  }
  var hours = Math.floor(diffMinutes / 60);
  var mins = diffMinutes % 60;
  return hours + "h" + (mins > 0 ? mins + "m" : "");
}

function sendLegData(trips) {
  var legQueue = [];

  for (var t = 0; t < trips.length; t++) {
    var legs = trips[t].legs;
    var legCount = Math.min(legs.length, 4);

    for (var l = 0; l < legCount; l++) {
      var departureDateTime = legs[l].origin.actualDateTime || legs[l].origin.plannedDateTime;
      var arrivalDateTime = legs[l].destination.actualDateTime || legs[l].destination.plannedDateTime;
      legQueue.push({
        tripIndex: t,
        legIndex: l,
        legCount: legCount,
        departureStation: abbreviateStation(legs[l].origin.name),
        departurePlatform: legs[l].origin.actualTrack || legs[l].origin.plannedTrack || "?",
        departureTime: extractTime(departureDateTime),
        arrivalStation: abbreviateStation(legs[l].destination.name),
        arrivalTime: extractTime(arrivalDateTime),
        duration: calculateDuration(departureDateTime, arrivalDateTime),
        depEpoch: convertIsoDateToEpoch(departureDateTime),
        arrEpoch: convertIsoDateToEpoch(arrivalDateTime)
      });
    }
  }

  var sendIndex = 0;

  function sendNextLeg() {
    if (sendIndex >= legQueue.length) {
      return;
    }

    var leg = legQueue[sendIndex];

    Pebble.sendAppMessage({
      "LEG_TRIP_INDEX": leg.tripIndex,
      "LEG_INDEX": leg.legIndex,
      "LEG_COUNT": leg.legCount,
      "LEG_DEPARTURE_STATION": leg.departureStation,
      "LEG_DEPARTURE_PLATFORM": leg.departurePlatform,
      "LEG_DEPARTURE_TIME": leg.departureTime,
      "LEG_ARRIVAL_STATION": leg.arrivalStation,
      "LEG_ARRIVAL_TIME": leg.arrivalTime,
      "LEG_DURATION": leg.duration,
      "LEG_DEPARTURE_EPOCH": leg.depEpoch,
      "LEG_ARRIVAL_EPOCH": leg.arrEpoch
    }, function() {
      sendIndex++;
      setTimeout(sendNextLeg, 100);
    }, function(e) {
      console.log("Failed to send leg message: " + e.error.message);
      sendIndex++;
      setTimeout(sendNextLeg, 200);
    });
  }

  sendNextLeg();
}

function createLegPinFromPayload(payload) {
  var tripIndex = payload.PIN_TRIP_INDEX;
  var legIndex = payload.PIN_LEG_INDEX;
  var depEpoch = payload.PIN_DEP_EPOCH;
  var arrEpoch = payload.PIN_ARR_EPOCH;
  var depStation = payload.PIN_DEP_STATION || '?';
  var arrStation = payload.PIN_ARR_STATION || '?';
  var platform = payload.PIN_PLATFORM || '?';

  var depDate = new Date(depEpoch * 1000);
  var arrDate = new Date(arrEpoch * 1000);
  var depTime = ('0' + depDate.getHours()).slice(-2) + ':' + ('0' + depDate.getMinutes()).slice(-2);
  var arrTime = ('0' + arrDate.getHours()).slice(-2) + ':' + ('0' + arrDate.getMinutes()).slice(-2);
  var durationMins = Math.round((arrEpoch - depEpoch) / 60);

  return {
    id: 'trein-' + tripIndex + '-' + legIndex,
    time: depDate.toISOString(),
    duration: durationMins,
    layout: {
      type: 'genericPin',
      title: abbreviateStation(depStation) + ' → ' + abbreviateStation(arrStation),
      subtitle: 'Platform ' + platform,
      tinyIcon: 'system://images/SCHEDULED_FLIGHT',
      body: 'Dep: ' + depTime + '  Arr: ' + arrTime
    }
  };
}

function pushPin(token, pin, callback) {
  var xhr = new XMLHttpRequest();
  xhr.open('PUT', 'https://timeline-api.getpebble.com/v1/user/pins/' + pin.id);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.setRequestHeader('X-User-Token', token);
  xhr.timeout = 10000;
  xhr.onload = function() { callback(xhr.status >= 200 && xhr.status < 300); };
  xhr.onerror = function() { callback(false); };
  xhr.ontimeout = function() { callback(false); };
  xhr.send(JSON.stringify(pin));
}

function sendPinStatus(success) {
  Pebble.sendAppMessage({'PIN_STATUS': success ? 1 : 0});
}

function pinToTimeline(payload) {
  var isLast = payload.PIN_IS_LAST;
  var pin = createLegPinFromPayload(payload);

  Pebble.getTimelineToken(function(token) {
    pushPin(token, pin, function(ok) {
      if (isLast) sendPinStatus(ok);
    });
  }, function() {
    if (isLast) sendPinStatus(false);
  });
}

function processTripData(data) {
  if (!data.trips || data.trips.length === 0) {
    console.log("No trips found");
    Pebble.sendAppMessage({
      "ERROR": 1
    });
    return;
  }


  var trips = data.trips.slice(0, 5); // Max 5 trips

  // Send each trip to the watch with a delay to avoid buffer overflow
  var sendIndex = 0;

  function sendNextTrip() {
    if (sendIndex >= trips.length) {
      // All trips sent, now send leg data
      sendLegData(trips);
      return;
    }

    var actualDepartureTime = trips[sendIndex].legs[0].origin.actualDateTime;
    var plannedDepartureTime = trips[sendIndex].legs[0].origin.plannedDateTime;
    var actualArrivalTime = trips[sendIndex].legs[trips[sendIndex].transfers].destination.actualDateTime;
    var plannedArrivalTime = trips[sendIndex].legs[trips[sendIndex].transfers].destination.plannedDateTime;
    if (actualDepartureTime === undefined) {
      tripDelay = "Cancelled";
      actualDepartureTime = trips[sendIndex].legs[trips[sendIndex].transfers].destination.plannedDateTime;
    }

    var delay = (Date.parse(actualDepartureTime)-Date.parse(plannedDepartureTime))/60000;
    var tripDelay = "On time";
    if (delay > 0) {
      tripDelay = "+" + delay;
    }

    if (trips[sendIndex].status == "CANCELLED") {
      actualDepartureTime = plannedDepartureTime;
      tripDelay = "Cancelled";
      actualArrivalTime = "--:--";
    }

    if (actualDepartureTime == undefined){
      console.log('Actual departure time is undefined; provider value is:');
      console.log(trips[sendIndex].legs[0].origin.actualDepartureTime);
    }

    var departurePlatform = trips[sendIndex].legs[0].origin.actualTrack;
    var tripTransfers = trips[sendIndex].transfers;
    var currentIndex = sendIndex;
    var actualDepartureTimeEpoch = convertIsoDateToEpoch(actualDepartureTime);


    Pebble.sendAppMessage({
      "TRIP_INDEX": currentIndex,
      "TRIP_PLANNED_DEPARTURE_TIME": plannedDepartureTime,
      "TRIP_DEPARTURE_TIME_EPOCH": actualDepartureTimeEpoch,
      "TRIP_PLANNED_ARRIVAL_TIME": plannedArrivalTime,
      "TRIP_ARRIVAL_TIME": actualArrivalTime,
      "TRIP_TRANSFERS": tripTransfers,
      "TRIP_PLATFORM": departurePlatform,
      "TRIP_DELAY": tripDelay,
      "TRIP_COUNT": trips.length
    }, function() {
      sendIndex++;
      // Wait 100ms before sending next message to avoid buffer overflow
      setTimeout(sendNextTrip, 100);
    }, function(e) {
      console.log("Failed to send message: " + e.error.message);
      sendIndex++;
      // Retry after a longer delay on error
      setTimeout(sendNextTrip, 200);
    });
  }

  sendNextTrip();
}

function fetchNearbyStations(lat, lng) {
  getDataSource().nearbyStations(lat, lng, function(error, stations) {
    if (error) {
      console.log("Could not load nearby stations: " + error.message);
      Pebble.sendAppMessage({"ERROR": 1});
      return;
    }
    processStationData(stations);
  });
}

function requestTrips(start, destination) {
  getDataSource().trips(start, destination, function(error, trips) {
    if (error) {
      console.log("Could not load trips: " + error.message);
      Pebble.sendAppMessage({"ERROR": 1});
      return;
    }
    processTripData({trips: trips});
  });
}
