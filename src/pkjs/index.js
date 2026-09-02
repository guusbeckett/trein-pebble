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

function asStr(v, fallback) {
  if (v == null) return fallback;
  return String(v);
}

function loadEmulatorDefaultKey() {
  if (typeof Pebble === "undefined" || Pebble.platform !== "pypkjs") return;
  if (DEFAULT_API_KEY) return;
  try {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "file:///home/box/.config/trein/ns_api_key", false);
    xhr.send(null);
    var body = xhr.responseText ? String(xhr.responseText).replace(/^\s+|\s+$/g, "") : "";
    if (body) DEFAULT_API_KEY = body;
  } catch (e) {}
}

function trimKey(s) {
  return s ? String(s).replace(/^\s+|\s+$/g, "") : "";
}

function getApiKey() {
  try {
    var key = trimKey(localStorage.getItem("api_key"));
    if (key) {
      return key;
    }
  } catch (e) {
    console.log("Error reading from localStorage: " + e);
  }
  loadEmulatorDefaultKey();
  return trimKey(DEFAULT_API_KEY);
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


function lsGet(k, d) { try { var v = localStorage.getItem(k); return v == null ? d : v; } catch (e) { return d; } }
function lsSet(k, v) { try { localStorage.setItem(k, v); } catch (e) {} }
function loadEmulatorOrsKey() {
  if (typeof Pebble === "undefined" || Pebble.platform !== "pypkjs") return "";
  try {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "file:///home/box/.config/trein/ors_api_key", false);
    xhr.send(null);
    return trimKey(xhr.responseText);
  } catch (e) {
    return "";
  }
}

function getOrsKey() {
  var k = trimKey(lsGet("routing_api_key", ""));
  if (k) return k;
  return loadEmulatorOrsKey();
}
function offsetForCode(code) {
  var map = {};
  try { map = JSON.parse(lsGet("station_offsets", "{}")); } catch (e) {}
  if (!code) return 0;
  if (map[code] != null) return parseInt(map[code], 10) || 0;
  var up = String(code).toUpperCase();
  for (var k in map) { if (k.toUpperCase() === up) return parseInt(map[k], 10) || 0; }
  return 0;
}
var lastStartCode = "", lastDestCode = "", stationCoords = {}, lastRouted = null, cachedDurationMin = null, orsInFlight = false, tripsSentOnce = false, routeTickMissedDest = false;
function haversineMeters(a, b, c, d) {
  var P = Math.PI / 180, dLat = (c - a) * P, dLon = (d - b) * P;
  var x = Math.sin(dLat / 2), y = Math.sin(dLon / 2);
  var z = x * x + Math.cos(a * P) * Math.cos(c * P) * y * y;
  return 12742000 * Math.atan2(Math.sqrt(z), Math.sqrt(1 - z));
}
function rememberStation(st) {
  if (!st || !st.code) return;
  var lat = st.lat != null ? st.lat : (st.latitude != null ? st.latitude : (st.locatie && st.locatie.lat) || (st.location && st.location.lat));
  var lng = st.lng != null ? st.lng : (st.lon != null ? st.lon : st.longitude);
  if (lng == null && st.locatie) lng = st.locatie.lng;
  if (lng == null && st.location) lng = st.location.lng || st.location.lon;
  if (lat != null && lng != null) stationCoords[st.code] = { lat: Number(lat), lng: Number(lng) };
}
function sendRouteToWatch(durationMin, atStation) {
  var payload = { "STATION_OFFSET": offsetForCode(lastStartCode) };
  if (atStation) payload.ROUTE_DURATION = -1;
  else if (durationMin != null) payload.ROUTE_DURATION = durationMin;
  Pebble.sendAppMessage(payload, function() {}, function() {});
}
function sendRouteError(code) {
  Pebble.sendAppMessage({
    "STATION_OFFSET": offsetForCode(lastStartCode),
    "ROUTE_ERROR": code
  }, function() {}, function() {});
}
function fetchOrsDuration(lat, lng, dest, profile, callback) {
  var key = getOrsKey();
  if (!key || !dest) { callback(null, key ? 2 : 1); return; }
  if (orsInFlight) { callback(cachedDurationMin, cachedDurationMin == null ? 0 : 0); return; }
  orsInFlight = true;
  var xhr = new XMLHttpRequest();
  xhr.timeout = 8000;
  var url = "https://api.heigit.org/openrouteservice/v2/directions/" + profile;
  xhr.open("POST", url, true);
  xhr.setRequestHeader("Authorization", key);
  xhr.setRequestHeader("api_key", key);
  xhr.setRequestHeader("Content-Type", "application/json");
  xhr.onload = function() {
    orsInFlight = false;
    console.log("ORS status: " + xhr.status);
    if (xhr.status < 200 || xhr.status >= 300) {
      callback(null, 2);
      return;
    }
    try {
      var data = JSON.parse(xhr.responseText), sec;
      if (data.features && data.features[0] && data.features[0].properties && data.features[0].properties.summary) {
        sec = data.features[0].properties.summary.duration;
      } else if (data.routes && data.routes[0] && data.routes[0].summary) {
        sec = data.routes[0].summary.duration;
      }
      if (typeof sec === "number") {
        lastRouted = { lat: lat, lng: lng };
        cachedDurationMin = Math.max(0, Math.round(sec / 60));
        callback(cachedDurationMin, 0);
        return;
      }
    } catch (e) {
      console.log("ORS parse fail");
    }
    callback(null, 2);
  };
  xhr.onerror = xhr.ontimeout = function() {
    orsInFlight = false;
    console.log("ORS status: error/timeout");
    callback(null, 2);
  };
  xhr.send(JSON.stringify({ coordinates: [[lng, lat], [dest.lng, dest.lat]] }));
}
function handleRouteTick(vervoer) {
  var doGps = function(pos) {
    var lat = pos.coords.latitude, lng = pos.coords.longitude;
    if (tripsSentOnce && lastStartCode && lastDestCode) requestTrips(lastStartCode, lastDestCode);
    var dest = stationCoords[lastStartCode];
    if (!dest) { routeTickMissedDest = true; sendRouteToWatch(null, false); return; }
    routeTickMissedDest = false;
    if (haversineMeters(lat, lng, dest.lat, dest.lng) <= 150) { cachedDurationMin = 0; sendRouteToWatch(0, true); return; }
    if (!getOrsKey()) { sendRouteError(1); return; }
    var moved = !lastRouted || haversineMeters(lat, lng, lastRouted.lat, lastRouted.lng) > 80;
    if (!moved && cachedDurationMin != null) { sendRouteToWatch(cachedDurationMin, false); return; }
    if (cachedDurationMin != null) sendRouteToWatch(cachedDurationMin, false);
    fetchOrsDuration(lat, lng, dest, vervoer === 1 ? "cycling-regular" : "foot-walking", function(mins, err) {
      if (mins != null) sendRouteToWatch(mins, false);
      else if (err) sendRouteError(err);
    });
  };
  var onErr = function() {
    if (tripsSentOnce && lastStartCode && lastDestCode) requestTrips(lastStartCode, lastDestCode);
    if (!stationCoords[lastStartCode]) routeTickMissedDest = true;
    sendRouteToWatch(cachedDurationMin, false);
  };
  if (typeof Pebble !== "undefined" && Pebble.platform === "pypkjs") {
    doGps({ coords: { latitude: 51.58719, longitude: 4.78322 } }); return;
  }
  navigator.geolocation.getCurrentPosition(doGps, onErr, { timeout: 10000, maximumAge: 15000, enableHighAccuracy: false });
}


function sendFavouritesToWatch() {
  var favourites = getFavourites();
  if (favourites.length === 0) {
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
      "FAVOURITE_INDEX": currentIndex,
      "FAVOURITE_CODE": asStr(fav && fav.code, ""),
      "FAVOURITE_NAME": asStr(fav && fav.name, ""),
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
  var url = "https://sberkers.github.io/trein-pebble/config.html";
  var currentKey = getApiKey();
  var favourites = getFavourites();
  var ors = getOrsKey();
  var offsets = localStorage.getItem("station_offsets") || "{}";
  Pebble.openURL(url + "?api_key=" + encodeURIComponent(currentKey) +
    "&routing_api_key=" + encodeURIComponent(ors) +
    "&station_offsets=" + encodeURIComponent(offsets) +
    "&favourites=" + encodeURIComponent(JSON.stringify(favourites)));
});

Pebble.addEventListener("webviewclosed", function(e) {
  if (!e.response) {
    return;
  }

  var settings;
  try {
    settings = JSON.parse(decodeURIComponent(e.response));
    if (settings.api_key) {
      localStorage.setItem("api_key", trimKey(settings.api_key));
      console.log("Saved new API key.");
    }
    if (settings.routing_api_key != null) lsSet("routing_api_key", trimKey(settings.routing_api_key));
    if (settings.station_offsets != null) {
      var off = settings.station_offsets;
      lsSet("station_offsets", typeof off === "string" ? off : JSON.stringify(off));
    }
    if (settings.favourites) {
      localStorage.setItem("favourites", JSON.stringify(settings.favourites));
      console.log("Saved favourites: " + settings.favourites.length);
      sendFavouritesToWatch();
    }
  } catch (err) {
    console.log("Error parsing settings: " + err);
  }
});


Pebble.addEventListener("ready", function(e) {
  console.log("PebbleKit JS ready!");
  loadEmulatorDefaultKey();
  sendFavouritesToWatch();
  requestLocationAndFetchStations();
});

Pebble.addEventListener("appmessage", function(e) {
  if (e.payload.REQUEST_STATIONS) {
    requestLocationAndFetchStations();
  }

  if (e.payload.START_STATION_CODE && e.payload.DEST_STATION_CODE) {
    lastStartCode = e.payload.START_STATION_CODE;
    lastDestCode = e.payload.DEST_STATION_CODE;
    requestTrips(lastStartCode, lastDestCode);
  }

  if (e.payload.REQUEST_PIN) {
    pinToTimeline(e.payload);
  }

  if (e.payload.REQUEST_ROUTE) {
    var vervoer = e.payload.ROUTE_MODE;
    if (vervoer == null) vervoer = parseInt(lsGet("settings_vervoer", "0"), 10);
    else lsSet("settings_vervoer", String(vervoer));
    handleRouteTick(vervoer);
  }
  if (e.payload.SETTINGS_TIJD_MODE != null) lsSet("settings_tijd_mode", String(e.payload.SETTINGS_TIJD_MODE));
  if (e.payload.SETTINGS_REISTIJD != null) lsSet("settings_reistijd", String(e.payload.SETTINGS_REISTIJD));
  if (e.payload.SETTINGS_VERVOER != null) lsSet("settings_vervoer", String(e.payload.SETTINGS_VERVOER));
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
  Pebble.sendAppMessage({ "STATION_COUNT": 0 });
}

function convertIsoDateToEpoch(apiDateString) {
  if (!apiDateString || typeof apiDateString !== 'string' || apiDateString.length < 19 || apiDateString[0] === '-') {
    return 0;
  }

  var compliantString = apiDateString.slice(0, -2) + ":" + apiDateString.slice(-2);
  
  var dateObject = new Date(compliantString);
  
  if (isNaN(dateObject)) {
    return 0;
  }

  return Math.round(dateObject.getTime() / 1000);
}

function processStationData(data) {
  if (!data.payload || data.payload.length === 0) {
    console.log("No stations found");
    Pebble.sendAppMessage({ "STATION_COUNT": 0 });
    return;
  }

  var nearbyStations = [];
  for (var j = 0; j < data.payload.length && nearbyStations.length < 8; j++) {
    rememberStation(data.payload[j]);
    nearbyStations.push({
      code: data.payload[j].code,
      name: data.payload[j].namen.middel
    });
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
      "STATION_CODE": asStr(stationCode, ""),
      "STATION_NAME": asStr(stationName, "?"),
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

function reportNsFailure(kind, status) {
  if (kind === "lookup") return;
  var missingKey = !getApiKey() || status === 401 || status === 403;
  if (kind === "trips") {
    if (!tripsSentOnce && missingKey) {
      Pebble.sendAppMessage({ "ERROR": 1 });
      return;
    }
    Pebble.sendAppMessage({ "TRIPS_FAILED": 1 });
    return;
  }
  if (missingKey) {
    Pebble.sendAppMessage({ "ERROR": 1 });
    return;
  }
  Pebble.sendAppMessage({ "STATION_COUNT": 0 });
}

function sendRequest(url, sendToWatchFunction, kind){
  kind = kind || "stations";
  var xhr = new XMLHttpRequest();
  xhr.timeout = 8000;

  xhr.open("GET", url, true);
  
  xhr.setRequestHeader("Cache-Control", "no-cache");
  xhr.setRequestHeader("Ocp-Apim-Subscription-Key", getApiKey());
  
  xhr.onload = function() {
    if (xhr.status >= 200 && xhr.status < 300) {
      var data;
      try {
        data = JSON.parse(xhr.responseText);
      } catch (e) {
        console.log("Error parsing JSON response: " + e);
        reportNsFailure(kind, xhr.status);
        return;
      }
      
      sendToWatchFunction(data);
    } else {
      console.log("Did not receive OK. Status: " + xhr.status);
      reportNsFailure(kind, xhr.status);
    }
  };

  xhr.onerror = function() {
    console.log("Fetch error: A network error occurred.");
    reportNsFailure(kind, 0);
  };
  
  xhr.send();
}

function lookupStartCoords(code) {
  if (!code || stationCoords[code]) return;
  var url = BASE_API_URL + "/nsapp-stations/v2?q=" + encodeURIComponent(code) + "&limit=8";
  sendRequest(url, function(data) {
    var list = data.payload || [];
    if (!Array.isArray(list)) list = list ? [list] : [];
    for (var i = 0; i < list.length; i++) {
      rememberStation(list[i]);
    }
    if (!stationCoords[code] && list[0]) {
      rememberStation({ code: code, lat: list[0].lat, lng: list[0].lng });
    }
    if (stationCoords[code]) {
      handleRouteTick(parseInt(lsGet("settings_vervoer", "0"), 10));
    }
  }, "lookup");
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
      "LEG_DEPARTURE_STATION": asStr(leg.departureStation, "?"),
      "LEG_DEPARTURE_PLATFORM": asStr(leg.departurePlatform, "?"),
      "LEG_DEPARTURE_TIME": asStr(leg.departureTime, "?"),
      "LEG_ARRIVAL_STATION": asStr(leg.arrivalStation, "?"),
      "LEG_ARRIVAL_TIME": asStr(leg.arrivalTime, "?"),
      "LEG_DURATION": asStr(leg.duration, "?"),
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
    reportNsFailure("trips", 200);
    return;
  }


  var trips = data.trips.slice(0, 5); // Max 5 trips
  try {
    var origin = trips[0].legs[0].origin;
    rememberStation({
      code: lastStartCode || origin.stationCode || origin.code,
      lat: origin.lat,
      lng: origin.lng != null ? origin.lng : origin.lon
    });
  } catch (e) {}
  if (routeTickMissedDest && stationCoords[lastStartCode]) {
    handleRouteTick(parseInt(lsGet("settings_vervoer", "0"), 10));
  }

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
      console.log('Actual departure time is undefined, value gotten from NS API is:');
      console.log(trips[sendIndex].legs[0].origin.actualDepartureTime);
    }

    var origin0 = trips[sendIndex].legs[0].origin;
    var departurePlatform = asStr(origin0.actualTrack || origin0.plannedTrack, "?");
    var tripTransfers = trips[sendIndex].transfers;
    var currentIndex = sendIndex;
    var actualDepartureTimeEpoch = convertIsoDateToEpoch(actualDepartureTime);
    var actualArrivalTimeEpoch = convertIsoDateToEpoch(actualArrivalTime);
    if (!actualArrivalTimeEpoch) {
      actualArrivalTimeEpoch = convertIsoDateToEpoch(plannedArrivalTime);
    }
    tripsSentOnce = true;

    Pebble.sendAppMessage({
      "TRIP_INDEX": currentIndex,
      "TRIP_PLANNED_DEPARTURE_TIME": asStr(plannedDepartureTime, ""),
      "TRIP_DEPARTURE_TIME_EPOCH": actualDepartureTimeEpoch,
      "TRIP_PLANNED_ARRIVAL_TIME": asStr(plannedArrivalTime, ""),
      "TRIP_ARRIVAL_TIME": asStr(actualArrivalTime, ""),
      "TRIP_ARRIVAL_TIME_EPOCH": actualArrivalTimeEpoch,
      "TRIP_TRANSFERS": tripTransfers,
      "TRIP_PLATFORM": departurePlatform,
      "TRIP_DELAY": asStr(tripDelay, ""),
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
  if (!getApiKey()) {
    Pebble.sendAppMessage({ "ERROR": 1 });
    return;
  }
  var url = BASE_API_URL + NEAREST_STATIONS_PATH + "?lat=" + lat + "&lng=" + lng + "&limit=8&includeNonPlannableStations=false";
  sendRequest(url, processStationData, "stations");
}

function requestTrips(start, destination) {
  lastStartCode = start;
  lastDestCode = destination;
  lookupStartCoords(start);
  var date_now = new Date();
  var url = BASE_API_URL + TRIP_PATH + "?fromStation=" + start + "&toStation=" + destination + "&dateTime=" + date_now.toISOString();
  sendRequest(url, processTripData, "trips");
}