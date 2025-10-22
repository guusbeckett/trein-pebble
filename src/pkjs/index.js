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
var DEFAULT_API_KEY = "";
var BASE_API_URL = "https://gateway.apiportal.ns.nl";
var NEAREST_STATIONS_PATH = "/nsapp-stations/v2/nearest";
var TRIP_PATH = "/reisinformatie-api/api/v3/trips";

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

Pebble.addEventListener("showConfiguration", function(e) {
  var url = "https://guusbeckett.github.io/config.html";
  var currentKey = getApiKey();
  
  Pebble.openURL(url + "?api_key=" + encodeURIComponent(currentKey));
});

Pebble.addEventListener("webviewclosed", function(e) {
  if (!e.response) {
    return;
  }
  
  var settings;
  try {
    settings = JSON.parse(decodeURIComponent(e.response));
    if (settings.api_key) {
      localStorage.setItem("api_key", settings.api_key);
      console.log("Saved new API key.");
    }
  } catch (err) {
    console.log("Error parsing settings: " + err);
  }
});


Pebble.addEventListener("ready", function(e) {
  console.log("PebbleKit JS ready!");
  requestLocationAndFetchStations();
});

Pebble.addEventListener("appmessage", function(e) {
  console.log("Received message from watch: " + JSON.stringify(e.payload));
  
  if (e.payload.REQUEST_STATIONS) {
    requestLocationAndFetchStations();
  }

  if (e.payload.START_STATION_CODE && e.payload.DEST_STATION_CODE) {
    var startCode = e.payload.START_STATION_CODE;
    var destCode = e.payload.DEST_STATION_CODE;
    requestTrips(startCode, destCode);
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
  console.log("Lat, lng" + lat + lng);
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

function processStationData(data) {
  if (!data.payload || data.payload.length === 0) {
    console.log("No stations found");
    Pebble.sendAppMessage({
      "ERROR": 1
    });
    return;
  }
  
  var stations = data.payload.slice(0, 8); // Max 8 stations
  
  console.log("Processing " + stations.length + " stations");
  
  var sendIndex = 0;
  
  function sendNextStation() {
    if (sendIndex >= stations.length) {
      return;
    }
    
    var stationName = stations[sendIndex].namen.middel;
    var stationCode = stations[sendIndex].code;
    var currentIndex = sendIndex;
    
    
    Pebble.sendAppMessage({
      "STATION_INDEX": currentIndex,
      "STATION_CODE": stationCode,
      "STATION_NAME": stationName,
      "STATION_COUNT": stations.length
    }, function() {
      console.log("Message sent successfully");
      sendIndex++;
      // Wait 20ms before sending next message to avoid buffer overflow
      setTimeout(sendNextStation, 100);
    }, function(e) {
      console.log("Failed to send message: " + e.error.message);
      sendIndex++;
      // Retry after a longer delay on error
      setTimeout(sendNextStation, 200);
    });
  }
  
  sendNextStation();
}

function sendRequest(url, sendToWatchFunction){
  var xhr = new XMLHttpRequest();
  xhr.timeout = 2000;

  xhr.open("GET", url, true); // The "true" argument makes it asynchronous.
  
  xhr.setRequestHeader("Cache-Control", "no-cache");
  xhr.setRequestHeader("Ocp-Apim-Subscription-Key", getApiKey());
  
  xhr.onload = function() {
    if (xhr.status >= 200 && xhr.status < 300) {
      var data;
      try {
        data = JSON.parse(xhr.responseText);
      } catch (e) {
        console.log("Error parsing JSON response: " + e);
        Pebble.sendAppMessage({
          "ERROR": 1
        });
        return;
      }
      
      sendToWatchFunction(data);
    } else {
      console.log("Did not receive OK. Status: " + xhr.status);
      Pebble.sendAppMessage({
        "ERROR": 1
      });
    }
  };

  xhr.onerror = function() {
    console.log("Fetch error: A network error occurred.");
    Pebble.sendAppMessage({
      "ERROR": 1
    });
  };
  
  xhr.send();
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

    var delay = (actualDepartureTime-Date.parse(trips[sendIndex].legs[0].origin.plannedDateTime))/60000;
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
      console.log(trips[sendIndex].legs[0].origin.actualDepartureTime);
    }
    
    var departurePlatform = trips[sendIndex].legs[0].origin.actualTrack;
    var tripTransfers = trips[sendIndex].transfers;
    var currentIndex = sendIndex;
    var actualDepartureTimeEpoch = convertIsoDateToEpoch(actualDepartureTime);

    console.log("TRIP_INDEX: " + currentIndex);
    console.log("TRIP_PLANNED_DEPARTURE_TIME: " + plannedDepartureTime);
    console.log("TRIP_DEPARTURE_TIME_EPOCH: " + actualDepartureTimeEpoch);
    console.log("TRIP_PLANNED_ARRIVAL_TIME: " + plannedArrivalTime);
    console.log("TRIP_ARRIVAL_TIME: " + actualArrivalTime);
    console.log("TRIP_TRANSFERS: " + tripTransfers);
    console.log("TRIP_PLATFORM: " + departurePlatform);
    console.log("TRIP_DELAY: " + tripDelay);
    console.log("TRIP_COUNT: " + trips.length);
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
  var url = BASE_API_URL + NEAREST_STATIONS_PATH + "?lat=" + lat + "&lng=" + lng + "&limit=8&includeNonPlannableStations=false";
  console.log("Url: "+ url);
  sendRequest(url, processStationData);
}

function requestTrips(start, destination) {
  const date_now = new Date();
  var url = BASE_API_URL + TRIP_PATH + "?fromStation=" + start + "&toStation=" + destination + "&dateTime=" + date_now.toISOString();
  sendRequest(url, processTripData);
}