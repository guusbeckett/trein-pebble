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
(function queueWatchMessages() {
  if (typeof Pebble === "undefined" || Pebble.__treinQueued) return;
  Pebble.__treinQueued = true;
  var orig = Pebble.sendAppMessage.bind(Pebble);
  var busy = false;
  var q = [];
  function drain() {
    if (busy || q.length === 0) return;
    busy = true;
    var it = q[0];
    orig(it.p, function() {
      busy = false;
      q.shift();
      if (it.ok) { try { it.ok.apply(null, arguments); } catch (e) {} }
      setTimeout(drain, 40);
    }, function() {
      busy = false;
      it.n = (it.n || 0) + 1;
      if (it.n >= 5) {
        q.shift();
        if (it.fail) { try { it.fail.apply(null, arguments); } catch (e) {} }
      }
      setTimeout(drain, 120);
    });
  }
  Pebble.sendAppMessage = function(payload, ok, fail) {
    q.push({ p: payload, ok: ok, fail: fail, n: 0 });
    drain();
  };
})();
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
  /* pypkjs is STPyV8, not Node.js - require('fs') doesn't exist.
   * file:// XHR also fails in pypkjs.
   * For emulator testing: inject localStorage.setItem("routing_api_key", "YOUR_KEY")
   * before selecting destination, or use config page to set key. */
  return "";
}

function getOrsKey() {
  var k = trimKey(lsGet("routing_api_key", ""));
  if (k) {
    console.log("getOrsKey: from localStorage, length=" + k.length);
    return k;
  }
  console.log("getOrsKey: NO KEY (emulator: inject localStorage.setItem('routing_api_key', 'YOUR_KEY'))");
  return "";
}
function offsetForCode(code) {
  var map = {};
  try { map = JSON.parse(lsGet("station_offsets", "{}")); } catch (e) {}
  if (!code) return 2;
  if (map[code] != null && map[code] !== "") {
    var n = parseInt(map[code], 10);
    if (!isNaN(n)) return n;
  }
  var up = String(code).toUpperCase();
  for (var k in map) {
    if (k.toUpperCase() === up) {
      var m = parseInt(map[k], 10);
      if (!isNaN(m)) return m;
    }
  }
  return 2;
}
function setOffsetForCode(code, minutes) {
  if (!code) return 2;
  var map = {};
  try { map = JSON.parse(lsGet("station_offsets", "{}")); } catch (e) {}
  if (!map || typeof map !== "object") map = {};
  var n = parseInt(minutes, 10);
  if (isNaN(n)) n = 2;
  if (n < -15) n = -15;
  if (n > 30) n = 30;
  map[code] = n;
  lsSet("station_offsets", JSON.stringify(map));
  return n;
}
function addFavouriteFromWatch(code, name) {
  if (!code) return;
  var favourites = getFavourites();
  var up = String(code).toUpperCase();
  for (var i = 0; i < favourites.length; i++) {
    if (favourites[i] && String(favourites[i].code).toUpperCase() === up) {
      sendFavouritesToWatch();
      return;
    }
  }
  if (favourites.length >= 5) {
    sendFavouritesToWatch();
    return;
  }
  favourites.push({ code: code, name: name || code });
  try { localStorage.setItem("favourites", JSON.stringify(favourites)); } catch (e) {}
  sendFavouritesToWatch();
}
var lastStartCode = "", lastDestCode = "", stationCoords = {}, lastRouted = null, cachedDurationMin = null, orsInFlight = false, tripsSentOnce = false, routeTickMissedDest = false;
var lastRouteGps = null, lastStationGps = null, tripsInFlight = false, pendingRouteTick = null, routeAfterTrips = false;
var lastSentDelay = null;
function haversineMeters(a, b, c, d) {
  var P = Math.PI / 180, dLat = (c - a) * P, dLon = (d - b) * P;
  var x = Math.sin(dLat / 2), y = Math.sin(dLon / 2);
  var z = x * x + Math.cos(a * P) * Math.cos(c * P) * y * y;
  return 12742000 * Math.atan2(Math.sqrt(z), Math.sqrt(1 - z));
}
function pickCoord(obj, keys) {
  if (!obj) return null;
  for (var i = 0; i < keys.length; i++) {
    if (obj[keys[i]] != null && obj[keys[i]] !== "") return Number(obj[keys[i]]);
  }
  return null;
}
function rememberStation(st) {
  if (!st) return;
  if (st.station && typeof st.station === "object") rememberStation(st.station);
  var code = st.code || st.stationCode || (st.payload && st.payload.code);
  if (!code) return;
  var lat = pickCoord(st, ["lat", "latitude"]);
  var lng = pickCoord(st, ["lng", "lon", "longitude"]);
  if (lat == null) lat = pickCoord(st.locatie, ["lat", "latitude"]) || pickCoord(st.location, ["lat", "latitude"]) || pickCoord(st.coordinates, ["lat", "latitude"]);
  if (lng == null) lng = pickCoord(st.locatie, ["lng", "lon", "longitude"]) || pickCoord(st.location, ["lng", "lon", "longitude"]) || pickCoord(st.coordinates, ["lng", "lon", "longitude"]);
  if ((lat == null || lng == null) && st.payload && typeof st.payload === "object") {
    lat = lat != null ? lat : pickCoord(st.payload, ["lat", "latitude"]);
    lng = lng != null ? lng : pickCoord(st.payload, ["lng", "lon", "longitude"]);
  }
  if (lat != null && lng != null && !isNaN(lat) && !isNaN(lng)) {
    stationCoords[code] = { lat: lat, lng: lng };
    stationCoords[String(code).toUpperCase()] = { lat: lat, lng: lng };
  }
}
function coordsFor(code) {
  if (!code) return null;
  if (stationCoords[code]) return stationCoords[code];
  var up = String(code).toUpperCase();
  if (stationCoords[up]) return stationCoords[up];
  for (var k in stationCoords) {
    if (String(k).toUpperCase() === up) return stationCoords[k];
  }
  return null;
}
function parseOrsDurationSec(data) {
  if (!data || typeof data !== "object") return null;
  try {
    if (data.routes && data.routes[0] && data.routes[0].summary && typeof data.routes[0].summary.duration === "number") {
      return data.routes[0].summary.duration;
    }
  } catch (e) {}
  try {
    if (data.features && data.features[0] && data.features[0].properties && data.features[0].properties.summary &&
        typeof data.features[0].properties.summary.duration === "number") {
      return data.features[0].properties.summary.duration;
    }
  } catch (e) {}
  return null;
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
var orsPendingCallbacks = [];
var orsTimeoutHandle = null;
function fetchOrsDuration(lat, lng, dest, profile, callback, isRetry) {
  var key = trimKey(getOrsKey());
  console.log("fetchOrsDuration START: key=" + (key ? "yes" : "no") + " dest=" + (dest ? "lat=" + dest.lat.toFixed(5) + ",lng=" + dest.lng.toFixed(5) : "no") + " lastStartCode=" + lastStartCode + " GPS=" + (lat != null ? lat.toFixed(5) + "," + lng.toFixed(5) : "none") + " profile=" + profile);
  
  if (!key || !dest) {
    console.log("fetchOrsDuration ABORT: missing key or dest");
    callback(null, key ? 2 : 1);
    return;
  }
  
  if (orsInFlight && !isRetry) {
    console.log("fetchOrsDuration DEFER: orsInFlight=true, cache=" + cachedDurationMin);
    if (cachedDurationMin != null) {
      callback(cachedDurationMin, 0);
    } else {
      orsPendingCallbacks.push(callback);
    }
    return;
  }
  
  console.log("fetchOrsDuration CALL ORS: " + profile + " from [" + lng.toFixed(5) + "," + lat.toFixed(5) + "] to [" + dest.lng.toFixed(5) + "," + dest.lat.toFixed(5) + "]");
  orsInFlight = true;
  if (orsTimeoutHandle) clearTimeout(orsTimeoutHandle);
  orsTimeoutHandle = setTimeout(function() {
    console.log("ORS timeout reset: orsInFlight stuck, clearing");
    orsInFlight = false;
    orsTimeoutHandle = null;
  }, 8000);
  
  var xhr = new XMLHttpRequest();
  xhr.timeout = 8000;
  var url = "https://api.heigit.org/openrouteservice/v2/directions/" + profile;
  xhr.open("POST", url, true);
  xhr.setRequestHeader("Authorization", key);
  xhr.setRequestHeader("Content-Type", "application/json");
  
  var handled = false;
  function handleResponse() {
    if (handled) return;
    handled = true;
    if (orsTimeoutHandle) clearTimeout(orsTimeoutHandle);
    orsTimeoutHandle = null;
    orsInFlight = false;
    
    var status = xhr.status || 0;
    console.log("ORS status: " + status + " readyState: " + xhr.readyState);
    
    if (status < 200 || status >= 300 || status === 0) {
      if (!isRetry) {
        console.log("ORS retry on status " + status);
        fetchOrsDuration(lat, lng, dest, profile, callback, true);
        return;
      }
      console.log("ORS final fail status " + status);
      callback(null, 2);
      while (orsPendingCallbacks.length > 0) {
        var cb = orsPendingCallbacks.shift();
        cb(null, 2);
      }
      return;
    }
    
    try {
      var sec = parseOrsDurationSec(JSON.parse(xhr.responseText));
      if (typeof sec === "number") {
        lastRouted = { lat: lat, lng: lng };
        cachedDurationMin = Math.max(0, Math.round(sec / 60));
        console.log("ORS success: " + sec + "s = " + cachedDurationMin + " min");
        callback(cachedDurationMin, 0);
        while (orsPendingCallbacks.length > 0) {
          var cb = orsPendingCallbacks.shift();
          cb(cachedDurationMin, 0);
        }
        return;
      }
    } catch (e) {
      console.log("ORS parse fail: " + e);
    }
    
    if (!isRetry) {
      console.log("ORS retry on parse fail");
      fetchOrsDuration(lat, lng, dest, profile, callback, true);
      return;
    }
    console.log("ORS final fail parse");
    callback(null, 2);
    while (orsPendingCallbacks.length > 0) {
      var cb = orsPendingCallbacks.shift();
      cb(null, 2);
    }
  }
  
  xhr.onloadend = handleResponse;
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      handleResponse();
    }
  };
  
  xhr.send(JSON.stringify({ coordinates: [[lng, lat], [dest.lng, dest.lat]] }));
}
function runRouteFrom(lat, lng, vervoer, force) {
  lastRouteGps = { lat: lat, lng: lng };
  var dest = coordsFor(lastStartCode);
  if (!dest) {
    console.log("runRouteFrom: missing VAN coords for " + lastStartCode + ", looking up");
    routeTickMissedDest = true;
    lookupStartCoords(lastStartCode);
    return;
  }
  routeTickMissedDest = false;
  var distM = haversineMeters(lat, lng, dest.lat, dest.lng);
  if (distM <= 150) {
    console.log("runRouteFrom: at station " + distM + "m, walk=0");
    cachedDurationMin = 0;
    sendRouteToWatch(0, true);
    return;
  }
  if (!getOrsKey()) {
    console.log("runRouteFrom: ORS ABORT NO KEY");
    sendRouteError(1);
    return;
  }
  var moved = !lastRouted || haversineMeters(lat, lng, lastRouted.lat, lastRouted.lng) > 80;
  if (!force && !moved && cachedDurationMin != null) {
    console.log("runRouteFrom: cached " + cachedDurationMin + " min, moved=" + moved);
    sendRouteToWatch(cachedDurationMin, false);
    return;
  }
  console.log("runRouteFrom: calling ORS " + (vervoer === 1 ? "bike" : "walk") + " dist=" + distM + "m");
  fetchOrsDuration(lat, lng, dest, vervoer === 1 ? "cycling-regular" : "foot-walking", function(mins, err) {
    if (mins != null) sendRouteToWatch(mins, false);
    else if (err) sendRouteError(err);
  });
}
function flushPendingRoute() {
  tripsInFlight = false;
  var tick = pendingRouteTick;
  pendingRouteTick = null;
  var need = routeAfterTrips;
  routeAfterTrips = false;
  if (tick) {
    handleRouteTick(tick.vervoer, tick.force);
    return;
  }
  if (need && lastStartCode && lastDestCode && parseInt(lsGet("settings_reistijd", "1"), 10) !== 0) {
    handleRouteTick(parseInt(lsGet("settings_vervoer", "0"), 10), true);
  }
}
function handleRouteTick(vervoer, force) {
  if (tripsInFlight) {
    console.log("handleRouteTick deferred tripsInFlight=1");
    pendingRouteTick = { vervoer: vervoer, force: !!force };
    return;
  }
  function go(lat, lng) {
    lastRouteGps = { lat: lat, lng: lng };
    runRouteFrom(lat, lng, vervoer, !!force);
  }
  function onFail(attempt) {
    if (attempt < 1) {
      setTimeout(function() { tryPos(attempt + 1); }, 500);
      return;
    }
    if (lastRouteGps) {
      runRouteFrom(lastRouteGps.lat, lastRouteGps.lng, vervoer, true);
      return;
    }
    if (!coordsFor(lastStartCode)) {
      routeTickMissedDest = true;
      lookupStartCoords(lastStartCode);
    }
    console.log("handleRouteTick GPS fail, no fallback");
  }
  function tryPos(attempt) {
    if (typeof Pebble !== "undefined" && Pebble.platform === "pypkjs") {
      var g = lastRouteGps || { lat: 51.58719, lng: 4.78322 };
      go(g.lat, g.lng);
      return;
    }
    navigator.geolocation.getCurrentPosition(
      function(pos) { go(pos.coords.latitude, pos.coords.longitude); },
      function() { onFail(attempt); },
      { timeout: 8000, maximumAge: 60000, enableHighAccuracy: true }
    );
  }
  tryPos(0);
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
    lastRouted = null;
    cachedDurationMin = null;
    requestTrips(lastStartCode, lastDestCode);
  }

  if (e.payload.REQUEST_PIN) {
    pinToTimeline(e.payload);
  }

  if (e.payload.REQUEST_FAVOURITE && e.payload.FAVOURITE_CODE) {
    addFavouriteFromWatch(e.payload.FAVOURITE_CODE, e.payload.FAVOURITE_NAME);
  }
  if (e.payload.STATION_OFFSET != null && e.payload.START_STATION_CODE && !e.payload.DEST_STATION_CODE) {
    var stored = setOffsetForCode(e.payload.START_STATION_CODE, e.payload.STATION_OFFSET);
    Pebble.sendAppMessage({ "STATION_OFFSET": stored }, function() {}, function() {});
  }
  if (e.payload.SETTINGS_TIJD_MODE != null) lsSet("settings_tijd_mode", String(e.payload.SETTINGS_TIJD_MODE));
  if (e.payload.SETTINGS_REISTIJD != null) lsSet("settings_reistijd", String(e.payload.SETTINGS_REISTIJD));
  if (e.payload.SETTINGS_VERVOER != null) {
    lsSet("settings_vervoer", String(e.payload.SETTINGS_VERVOER));
    lastRouted = null;
    cachedDurationMin = null;
  }
  if (e.payload.REQUEST_ROUTE) {
    var vervoer = e.payload.ROUTE_MODE;
    if (vervoer == null) vervoer = parseInt(lsGet("settings_vervoer", "0"), 10);
    else lsSet("settings_vervoer", String(vervoer));
    var reistijd = e.payload.SETTINGS_REISTIJD;
    if (reistijd == null) reistijd = parseInt(lsGet("settings_reistijd", "1"), 10);
    else lsSet("settings_reistijd", String(reistijd));
    if (parseInt(reistijd, 10) === 0) {
      sendRouteToWatch(null, false);
    } else {
      handleRouteTick(vervoer, true);
    }
  } else if (e.payload.SETTINGS_VERVOER != null && parseInt(lsGet("settings_reistijd", "1"), 10) !== 0) {
    lastRouted = null;
    cachedDurationMin = null;
    handleRouteTick(parseInt(e.payload.SETTINGS_VERVOER, 10), true);
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
  lastStationGps = { lat: lat, lng: lng };
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
  if (kind === "trips") {
    flushPendingRoute();
  }
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
  if (!code) return;
  if (coordsFor(code)) return;
  var url = BASE_API_URL + "/nsapp-stations/v2?q=" + encodeURIComponent(code) + "&limit=8";
  sendRequest(url, function(data) {
    var list = data.payload || data.stations || [];
    if (!Array.isArray(list)) list = list ? [list] : [];
    for (var i = 0; i < list.length; i++) {
      rememberStation(list[i]);
    }
    if (!coordsFor(code) && list[0]) {
      rememberStation({ code: code, lat: list[0].lat, lng: list[0].lng, lon: list[0].lon });
    }
    if (coordsFor(code)) {
      handleRouteTick(parseInt(lsGet("settings_vervoer", "0"), 10), true);
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
  if (!dateTimeString) return "";
  if (dateTimeString === "--:--") return "--:--";
  if (/^\d{2}:\d{2}$/.test(dateTimeString)) return dateTimeString;
  var match = String(dateTimeString).match(/T(\d{2}:\d{2})/);
  return match ? match[1] : "";
}

function lastTrainLeg(trip) {
  var legs = trip && trip.legs ? trip.legs : [];
  if (!legs.length) return null;
  for (var i = legs.length - 1; i >= 0; i--) {
    var leg = legs[i];
    if (!leg) continue;
    var t = "";
    if (leg.travelType) t = String(leg.travelType);
    else if (leg.mode) t = String(leg.mode);
    else if (leg.product && (leg.product.shortCategory || leg.product.categoryCode)) {
      t = String(leg.product.shortCategory || leg.product.categoryCode);
    }
    t = t.toUpperCase();
    if (leg.walk || t.indexOf("WALK") >= 0 || t.indexOf("LOPEN") >= 0) continue;
    return leg;
  }
  return legs[legs.length - 1];
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
  flushPendingRoute();
  
  var legQueue = [];

  for (var t = 0; t < trips.length; t++) {
    var legs = (trips[t] && trips[t].legs) ? trips[t].legs : [];
    var legCount = Math.min(legs.length, 4);

    for (var l = 0; l < legCount; l++) {
      if (!legs[l] || !legs[l].origin || !legs[l].destination) continue;
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
      flushPendingRoute();
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


  var rawTrips = data.trips.slice(0, 5);
  var trips = [];
  for (var ti = 0; ti < rawTrips.length; ti++) {
    var cand = rawTrips[ti];
    var fl = cand && cand.legs && cand.legs[0];
    var ll = lastTrainLeg(cand);
    if (!fl || !fl.origin || !ll || !ll.destination) continue;
    trips.push(cand);
  }
  if (!trips.length) {
    reportNsFailure("trips", 200);
    return;
  }
  try {
    var origin = trips[0].legs[0].origin;
    rememberStation({
      code: lastStartCode || origin.stationCode || origin.code,
      lat: origin.lat,
      lng: origin.lng != null ? origin.lng : origin.lon
    });
  } catch (e) {}
  if (routeTickMissedDest && coordsFor(lastStartCode)) {
    handleRouteTick(parseInt(lsGet("settings_vervoer", "0"), 10), true);
  }

  // Send each trip to the watch with a delay to avoid buffer overflow
  var sendIndex = 0;

  function sendNextTrip() {
    if (sendIndex >= trips.length) {
      /* All TRIP_* messages sent - flush pending route NOW so ORS can run */
      flushPendingRoute();
      sendLegData(trips);
      return;
    }
    try {
    var trip = trips[sendIndex];
    var firstLeg = trip && trip.legs && trip.legs[0];
    var lastLeg = lastTrainLeg(trip);
    if (!firstLeg || !firstLeg.origin || !lastLeg || !lastLeg.destination) {
      console.log("skip trip missing legs");
      sendIndex++;
      setTimeout(sendNextTrip, 100);
      return;
    }

    var actualDepartureTime = firstLeg.origin.actualDateTime;
    var plannedDepartureTime = firstLeg.origin.plannedDateTime;
    var actualArrivalTime = lastLeg.destination.actualDateTime;
    var plannedArrivalTime = lastLeg.destination.plannedDateTime;
    var tripDelay = "";
    if (actualDepartureTime === undefined) {
      tripDelay = "Cancelled";
      actualDepartureTime = lastLeg.destination.plannedDateTime;
    }

    var delay = (Date.parse(actualDepartureTime)-Date.parse(plannedDepartureTime))/60000;
    if (delay > 0 && tripDelay !== "Cancelled") {
      tripDelay = "+" + delay;
    }

    if (trip.status == "CANCELLED") {
      actualDepartureTime = plannedDepartureTime;
      tripDelay = "Cancelled";
      actualArrivalTime = "--:--";
    }

    var origin0 = firstLeg.origin;
    var departurePlatform = asStr(origin0.actualTrack || origin0.plannedTrack, "?");
    var tripTransfers = typeof trip.transfers === "number" ? trip.transfers : 0;
    var currentIndex = sendIndex;
    var actualDepartureTimeEpoch = convertIsoDateToEpoch(actualDepartureTime);
    var plannedDepartureTimeEpoch = convertIsoDateToEpoch(plannedDepartureTime);
    var actualArrivalTimeEpoch = convertIsoDateToEpoch(actualArrivalTime);
    if (!actualArrivalTimeEpoch) {
      actualArrivalTimeEpoch = convertIsoDateToEpoch(plannedArrivalTime);
    }
    var originArrivalIso = origin0.actualArrivalDateTime || origin0.plannedArrivalDateTime || "";
    var originArrivalEpoch = convertIsoDateToEpoch(originArrivalIso);
    if (!originArrivalEpoch) originArrivalEpoch = plannedDepartureTimeEpoch;
    
    if (sendIndex === 0) {
      var delayKey = actualDepartureTimeEpoch;
      if (lastSentDelay !== null && lastSentDelay !== delayKey && tripDelay !== "On time") {
        if (typeof Pebble !== "undefined" && Pebble.vibrateOnce) {
          Pebble.vibrateOnce();
        }
      }
      lastSentDelay = delayKey;
    }
    
    tripsSentOnce = true;
    } catch (err) {
      console.log("trip parse fail: " + err);
      sendIndex++;
      setTimeout(sendNextTrip, 100);
      return;
    }

    console.log("trip arr planned=" + extractTime(plannedArrivalTime) +
                " actual=" + extractTime(actualArrivalTime));
    Pebble.sendAppMessage({
      "TRIP_INDEX": currentIndex,
      "TRIP_PLANNED_DEPARTURE_TIME": extractTime(plannedDepartureTime),
      "TRIP_PLANNED_DEPARTURE_TIME_EPOCH": plannedDepartureTimeEpoch,
      "TRIP_DEPARTURE_TIME_EPOCH": actualDepartureTimeEpoch,
      "TRIP_ORIGIN_ARRIVAL_EPOCH": originArrivalEpoch,
      "TRIP_PLANNED_ARRIVAL_TIME": extractTime(plannedArrivalTime),
      "TRIP_ARRIVAL_TIME": extractTime(actualArrivalTime),
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
  if (lat != null && lng != null && !isNaN(lat) && !isNaN(lng)) {
    lastStationGps = { lat: lat, lng: lng };
  }
  if (typeof Pebble !== "undefined" && Pebble.platform === "pypkjs") {
    processStationData({ payload: [
      { code: "bd", namen: { middel: "Breda" }, lat: 51.5958, lng: 4.779 },
      { code: "ehv", namen: { middel: "Eindhoven Centraal" }, lat: 51.443, lng: 5.481 }
    ]});
    return;
  }
  if (!getApiKey()) {
    Pebble.sendAppMessage({ "ERROR": 1 });
    return;
  }
  var url = BASE_API_URL + NEAREST_STATIONS_PATH + "?lat=" + lat + "&lng=" + lng + "&limit=8&includeNonPlannableStations=false";
  sendRequest(url, processStationData, "stations");
}


function pad2(n) { return n < 10 ? "0" + n : String(n); }
function toNsIso(ms) {
  var d = new Date(ms + 2 * 3600000);
  return d.getUTCFullYear() + "-" + pad2(d.getUTCMonth() + 1) + "-" + pad2(d.getUTCDate()) +
    "T" + pad2(d.getUTCHours()) + ":" + pad2(d.getUTCMinutes()) + ":" + pad2(d.getUTCSeconds()) + "+0200";
}
function emulatorSendMockTrip(start, destination, delayed) {
  if (typeof Pebble === "undefined" || Pebble.platform !== "pypkjs") return false;
  var now = Date.now();
  var plannedDep = delayed ? now - 2 * 60000 : now + 12 * 60000;
  var actualDep = delayed ? plannedDep + 8 * 60000 : plannedDep;
  var plannedArr = plannedDep + 81 * 60000;
  var actualArr = actualDep + 81 * 60000;
  var c = coordsFor(start) || { lat: 51.58719, lng: 4.78322 };
  processTripData({
    trips: [{
      transfers: 0,
      status: "NORMAL",
      legs: [{
        travelType: "TRAIN",
        origin: {
          stationCode: start,
          plannedDateTime: toNsIso(plannedDep),
          actualDateTime: toNsIso(actualDep),
          plannedArrivalDateTime: toNsIso(plannedDep - 3 * 60000),
          actualArrivalDateTime: toNsIso(plannedDep - 3 * 60000),
          plannedTrack: "3",
          actualTrack: "3",
          lat: c.lat,
          lng: c.lng
        },
        destination: {
          plannedDateTime: toNsIso(plannedArr),
          actualDateTime: toNsIso(actualArr)
        }
      }]
    }]
  });
  return true;
}

function emulatorInjectDelayTrip(start, destination) {
  if (typeof Pebble === "undefined" || Pebble.platform !== "pypkjs") return false;
  if (lsGet("emu_inject_delay", "") !== "1") return false;
  return emulatorSendMockTrip(start, destination, true);
}

function requestTrips(start, destination) {
  lastStartCode = start;
  lastDestCode = destination;
  tripsInFlight = true;
  if (parseInt(lsGet("settings_reistijd", "1"), 10) !== 0) {
    routeAfterTrips = true;
  }
  lookupStartCoords(start);
  if (emulatorInjectDelayTrip(start, destination)) return;
  if (typeof Pebble !== "undefined" && Pebble.platform === "pypkjs" && !getApiKey()) {
    emulatorSendMockTrip(start, destination, false);
    return;
  }
  var date_now = new Date();
  var url = BASE_API_URL + TRIP_PATH + "?fromStation=" + start + "&toStation=" + destination + "&dateTime=" + date_now.toISOString();
  sendRequest(url, processTripData, "trips");
}