/*
 * Train data source adapters. Each adapter exposes the same two operations:
 *   nearbyStations(latitude, longitude, callback)
 *   trips(fromStation, toStation, callback)
 *
 * Callbacks receive (error, normalizedData). Keeping the normalization here
 * prevents provider-specific response formats from leaking into the watch UI.
 */

var NS_BASE_URL = "https://gateway.apiportal.ns.nl";
var IRAIL_BASE_URL = "https://api.irail.be";
var IRAIL_USER_AGENT = "Trein-Pebble/1.8.0 (https://github.com/guusbeckett/trein-pebble)";
var stationCache = null;

function requestJson(url, headers, callback) {
  var xhr = new XMLHttpRequest();
  xhr.timeout = 10000;
  xhr.open("GET", url, true);
  xhr.setRequestHeader("Accept", "application/json");

  Object.keys(headers || {}).forEach(function(name) {
    xhr.setRequestHeader(name, headers[name]);
  });

  xhr.onload = function() {
    if (xhr.status < 200 || xhr.status >= 300) {
      callback(new Error("HTTP " + xhr.status));
      return;
    }
    try {
      callback(null, JSON.parse(xhr.responseText));
    } catch (error) {
      callback(error);
    }
  };
  xhr.onerror = function() { callback(new Error("Network error")); };
  xhr.ontimeout = function() { callback(new Error("Request timed out")); };
  xhr.send();
}

function distanceSquared(latitude, longitude, station) {
  // Good enough for ordering stations within one country and cheaper than a
  // full geodesic calculation on older phones.
  var latitudeDelta = Number(station.locationY) - latitude;
  var longitudeScale = Math.cos(latitude * Math.PI / 180);
  var longitudeDelta = (Number(station.locationX) - longitude) * longitudeScale;
  return latitudeDelta * latitudeDelta + longitudeDelta * longitudeDelta;
}

function pad(value) {
  return value < 10 ? "0" + value : String(value);
}

function epochToLocalIso(epoch) {
  var date = new Date(Number(epoch) * 1000);
  var offsetMinutes = -date.getTimezoneOffset();
  var sign = offsetMinutes >= 0 ? "+" : "-";
  var absoluteOffset = Math.abs(offsetMinutes);
  return date.getFullYear() + "-" + pad(date.getMonth() + 1) + "-" + pad(date.getDate()) +
    "T" + pad(date.getHours()) + ":" + pad(date.getMinutes()) + ":" + pad(date.getSeconds()) +
    sign + pad(Math.floor(absoluteOffset / 60)) + pad(absoluteOffset % 60);
}

function normalizeIRailStop(stop, useArrivalDelay) {
  stop = stop || {};
  var delay = Number(useArrivalDelay && stop.arrivalDelay !== undefined ? stop.arrivalDelay :
    (stop.departureDelay !== undefined ? stop.departureDelay : stop.delay)) || 0;
  var canceled = Number(useArrivalDelay && stop.arrivalCanceled !== undefined ? stop.arrivalCanceled :
    (stop.departureCanceled !== undefined ? stop.departureCanceled : stop.canceled)) === 1;
  var plannedEpoch = Number(useArrivalDelay && stop.scheduledArrivalTime ? stop.scheduledArrivalTime :
    (stop.scheduledDepartureTime || stop.time));
  var stationInfo = stop.stationinfo || {};

  return {
    name: stationInfo.name || stationInfo.standardname || stop.station || "?",
    plannedDateTime: epochToLocalIso(plannedEpoch),
    actualDateTime: epochToLocalIso(plannedEpoch + delay),
    plannedTrack: String((stop.platforminfo && stop.platforminfo.name) || stop.platform || "?"),
    actualTrack: String((stop.platforminfo && stop.platforminfo.name) || stop.platform || "?"),
    canceled: canceled
  };
}

function normalizeIRailConnection(connection) {
  var vias = connection.vias && connection.vias.via ? connection.vias.via : [];
  var legs = [];
  var origin = normalizeIRailStop(connection.departure, false);

  for (var index = 0; index < vias.length; index++) {
    var via = vias[index];
    legs.push({
      origin: origin,
      destination: normalizeIRailStop(via.arrival || via, true)
    });
    origin = normalizeIRailStop(via.departure || via, false);
  }

  legs.push({
    origin: origin,
    destination: normalizeIRailStop(connection.arrival, true)
  });

  var canceled = Number(connection.departure.canceled) === 1 ||
    Number(connection.arrival.canceled) === 1;
  return {
    legs: legs,
    transfers: Math.max(0, legs.length - 1),
    status: canceled ? "CANCELLED" : "NORMAL"
  };
}

function createNSDataSource(getApiKey) {
  function headers() {
    return {"Ocp-Apim-Subscription-Key": getApiKey()};
  }

  return {
    id: "ns",
    apiKeyRequired: true,
    nearbyStations: function(latitude, longitude, callback) {
      var url = NS_BASE_URL + "/nsapp-stations/v2/nearest?lat=" + encodeURIComponent(latitude) +
        "&lng=" + encodeURIComponent(longitude) + "&limit=8&includeNonPlannableStations=false";
      requestJson(url, headers(), function(error, data) {
        if (error) return callback(error);
        var stations = (data.payload || []).slice(0, 8).map(function(station) {
          return {code: station.code, name: station.namen.middel};
        });
        callback(null, stations);
      });
    },
    trips: function(fromStation, toStation, callback) {
      var url = NS_BASE_URL + "/reisinformatie-api/api/v3/trips?fromStation=" +
        encodeURIComponent(fromStation) + "&toStation=" + encodeURIComponent(toStation) +
        "&dateTime=" + encodeURIComponent(new Date().toISOString());
      requestJson(url, headers(), function(error, data) {
        callback(error, data && data.trips ? data.trips : []);
      });
    }
  };
}

function createIRailDataSource() {
  var headers = {"User-Agent": IRAIL_USER_AGENT};
  return {
    id: "irail",
    apiKeyRequired: false,
    nearbyStations: function(latitude, longitude, callback) {
      function selectNearby(data) {
        var stations = (data.station || []).slice();
        stations.sort(function(left, right) {
          return distanceSquared(latitude, longitude, left) - distanceSquared(latitude, longitude, right);
        });
        callback(null, stations.slice(0, 8).map(function(station) {
          return {
            code: station.id,
            name: station.name || station.standardname
          };
        }));
      }

      if (stationCache) {
        selectNearby(stationCache);
        return;
      }
      requestJson(IRAIL_BASE_URL + "/stations/?format=json&lang=nl", headers, function(error, data) {
        if (error) return callback(error);
        stationCache = data;
        selectNearby(data);
      });
    },
    trips: function(fromStation, toStation, callback) {
      var url = IRAIL_BASE_URL + "/connections/?from=" + encodeURIComponent(fromStation) +
        "&to=" + encodeURIComponent(toStation) +
        "&timesel=departure&format=json&lang=nl&typeOfTransport=automatic&alerts=false&results=5";
      requestJson(url, headers, function(error, data) {
        if (error) return callback(error);
        callback(null, (data.connection || []).map(normalizeIRailConnection));
      });
    }
  };
}

module.exports = {
  create: function(id, getApiKey) {
    return id === "irail" ? createIRailDataSource() : createNSDataSource(getApiKey);
  },
  ids: ["ns", "irail"],
  irailUserAgent: IRAIL_USER_AGENT,
  _normalizeIRailConnection: normalizeIRailConnection
};
