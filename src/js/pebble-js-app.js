// ROUNDY JS

var config={}; // CONFIG_SECONDS, CONFIG_HOURVIBES, CONFIG_FAHRENHEIT, CONFIG_24H, CONFIG_SETCITY, CONFIG_REVERSE
var locationOptions = { "timeout": 60000, "maximumAge": 1200000 };
var prevcity = ""; // previous city if set
// var appid = "869c6da7ab3f807c4b15fd7574786e72"; // Openweathermap API ID old
var appid = "d204cb99d4331fffa26340b8e03bbe17"; // Openweathermap API ID

//======================================
// GET ICON FROM WEATHER ID - based on OpenWeatherMap icon codes, not ID codes
//======================================
function iconFromWeatherId(weatherIcon) {
/*
    if (weatherId < 600) {
        return 2;
    } else if (weatherId < 700) {
        return 3;
    } else if (weatherId > 800) {
        return 1;
    } else {
        return 0;
    }
*/
    var iconFile = "";
    switch (weatherIcon) {
        case "01d" : iconFile = 0; break;
        case "01n" : iconFile = 1; break;
        case "02d" : iconFile = 2; break;
        case "02n" : iconFile = 2; break;
        case "03d" : iconFile = 2; break;
        case "03n" : iconFile = 2; break;
        case "04d" : iconFile = 2; break;
        case "04n" : iconFile = 2; break;
        case "09d" : iconFile = 6; break;
        case "09n" : iconFile = 6; break;
        case "10d" : iconFile = 6; break;
        case "10n" : iconFile = 6; break;
        case "11d" : iconFile = 6; break;
        case "11n" : iconFile = 6; break;
        case "13d" : iconFile = 7; break;
        case "13n" : iconFile = 7; break;
        case "50d" : iconFile = 4; break;
        case "50n" : iconFile = 5; break;
        default    : iconFile = 3; break;
    }
    return iconFile;
}

//======================================
// TIME CONVERTER - converts UTC to ampm or 24 hour time
// depends on 24h time config setting
//======================================
function timeConverter(timeUTC) {
	var ampm = "";
	var temp_date = new Date(timeUTC * 1000);
	var temp_hours = temp_date.getHours();	
	var temp_minutes = "0" + temp_date.getMinutes();

	if (config.CONFIG_24H == 0) {
		if (temp_hours == 0) {
			temp_hours = 12;
			ampm = "AM";
		}
		if ((temp_hours > 0) && (temp_hours < 12)) {
			ampm = "AM";
		}
		if (temp_hours == 12) {
			ampm = "PM";
		}
		if (temp_hours > 12) {
			temp_hours = temp_hours - 12;
			ampm = "PM";
		}
	}
	
	if (config.CONFIG_24H == 0) {
		return temp_hours + ':' + temp_minutes.substr(temp_minutes.length-2) + " " + ampm;		
	}
	else {
		return temp_hours + ':' + temp_minutes.substr(temp_minutes.length-2);
	}
}

//======================================
// TEMPERATURE CONVERTER - Kelvin to F, Kelvin to C
// depends on Fahrenheit config setting
//======================================
function tempConverter(intemp) {
    if (intemp == "-") {
        return intemp;
    }
    else {
        if (config.CONFIG_FAHRENHEIT == 1) {
            return Math.round(((intemp - 272.15) * 9/5) + 32);
        }
        else {
            return Math.round(intemp - 272.15);
        }
    }
}

//======================================
// WINDSPEED CONVERTER - meters per second to km/h or mph
// depends on Fahrenheit/metric config setting
//======================================
function windConverter(inwind) {
	if (config.CONFIG_FAHRENHEIT == 1) {		  
		return Math.round(((inwind * 60 * 60)/1000)*0.6214) + " »"; // return mph
	}
	else {
		return Math.round((inwind * 60 * 60)/1000) + " »"; // return km/h
	}
}

//======================================
// FETCH CURRENT WEATHER DATA
//======================================
function fetchWeather(favcity, latitude, longitude) {
	// save location data for cache
	if (latitude != null) {
		localStorage.setItem("latitude", latitude);
	}
	if (longitude != null) {
		localStorage.setItem("longitude", longitude);
	}
	
    // load weather data based on city name, city id, gps
	var req = new XMLHttpRequest();
    if (favcity != null) {
        if (config.CONFIG_CITYID == 0) {
            console.log("fetching city name");
            req.open('GET', "http://api.openweathermap.org/data/2.5/weather?" +
                     "q=" + favcity + "&cnt=1&APPID=" + appid, true);
        }
        else {
            console.log("fetching city id");
            req.open('GET', "http://api.openweathermap.org/data/2.5/weather?" +
                     "id=" + favcity + "&cnt=1&APPID=" + appid, true);
        }
    }
    else {
        console.log("fetching gps");
        req.open('GET', "http://api.openweathermap.org/data/2.5/weather?" +
                 "lat=" + latitude + "&lon=" + longitude + "&cnt=1&APPID=" + appid, true);
    }

//	req.onload = function (e) {
	req.onreadystatechange = function (e) {
		if (req.readyState == 4) {
			if (req.status == 200) {
				console.log(req.responseText);
                var response = JSON.parse(req.responseText);
                if (response.cod == "404") {
                    localStorage.setItem("temperature", "-");
                    localStorage.setItem("city", "no location");
                }
                else {
                    var temperature = response.main.temp; // in K
                    var icon = iconFromWeatherId(response.weather[0].icon);
                    //var icon = response.weather[0].icon;
                    var conditions = response.weather[0].main;
                    var humidity = response.main.humidity;
                    var windspeed = response.wind.speed; // in mps
                    var city = response.name;
                    var sunrise_UTC = response.sys.sunrise;
                    var sunset_UTC = response.sys.sunset;
                    var fetch_timestamp = new Date(response.dt * 1000);

                    // Save weather data for cache
                    localStorage.setItem("icon", icon);
                    localStorage.setItem("temperature", temperature);
                    localStorage.setItem("conditions", conditions);
                    localStorage.setItem("humidity", humidity);
                    localStorage.setItem("windspeed", windspeed);
                    localStorage.setItem("city", city);
                    localStorage.setItem("sunrise_time", sunrise_UTC);
                    localStorage.setItem("sunset_time", sunset_UTC);
                    localStorage.setItem("weather_timestamp", fetch_timestamp);
                }
                readCachedData();
            }
		}
	}
	req.send(null);
	
}

//======================================
// READ CACHED DATA FROM PHONE
//======================================
function readCachedData() {
	if (localStorage.getItem("weather_timestamp") != null) {
		var cached_city = localStorage.getItem("city").substr(0,15);
		var cached_icon = Math.floor(localStorage.getItem("icon"));
		var cached_temperature = localStorage.getItem("temperature");
		var cached_conditions = localStorage.getItem("conditions");
        var cached_humidity = localStorage.getItem("humidity");
		var cached_windspeed = localStorage.getItem("windspeed");
		var cached_sunrise_UTC = localStorage.getItem("sunrise_time");
		var cached_sunset_UTC = localStorage.getItem("sunset_time");
		var cached_weather_timestamp = localStorage.getItem("weather_timestamp");
        cached_weather_timestamp = cached_weather_timestamp.substring(cached_weather_timestamp.indexOf(':')-2,cached_weather_timestamp.indexOf(':')+3);
        //variable = variable.substring(variable.indexOf('?v=')+3) // First occurence of ?v=

        // do temperature conversions
		var temperature_formatted = tempConverter(cached_temperature);
		
		// do windspeed conversions
		var windspeed_formatted = windConverter(cached_windspeed)
		
		// do time conversions from UTC to ampm/24 hour time
		var cached_sunrise_time = timeConverter(cached_sunrise_UTC);
		var cached_sunset_time = timeConverter(cached_sunset_UTC);
		
        // show sunrise/sunset times as default, or humidity/windspeed as alternate
        if (config.CONFIG_ALTDATA == 0) {
            var misc_data = cached_sunrise_time;
            var misc2_data = cached_sunset_time;
        }
        else {
            var misc_data = "∆" + cached_humidity + "%";
            var misc2_data = windspeed_formatted;
        }
        
		Pebble.sendAppMessage({
			"WEATHER_ICON":cached_icon,
			"WEATHER_WEATHERDATA": temperature_formatted + "\u00B0",
			"WEATHER_MISC": misc_data,
			"WEATHER_MISC2": misc2_data,
            "WEATHER_MISC3": cached_weather_timestamp
//            "WEATHER_MISC3": cached_city + "\n" + cached_weather_timestamp
//		  	"CONFIG_SECONDS": config.CONFIG_SECONDS,
//		  	"CONFIG_HOURVIBES": config.CONFIG_HOURVIBES,
//		  	"CONFIG_REVERSE": config.CONFIG_REVERSE,
		});
	}
	else { 
		console.log("Error, no data in cache");
		Pebble.sendAppMessage({
    		"WEATHER_WEATHERDATA":"-",
            "WEATHER_MISC3":"no data"
  		});
	}
}

//======================================
// LOCATION SUCCESS - fetch weather and forecast data
//======================================
function locationSuccess(pos) {
  	var coordinates = pos.coords;
  	fetchWeather(null, coordinates.latitude, coordinates.longitude);
	//console.log("Coordinates " + coordinates.latitude + " ," + coordinates.longitude);
}

//======================================
// LOCATION ERROR - fetch weather and forecast data using cached lat/long
//======================================
function locationError(err) {
	console.log('location error (' + err.code + '): ' + err.message);
	if (localStorage.getItem("latitude") != null) {
		fetchWeather(null, localStorage.getItem("latitude"), localStorage.getItem("longitude"));
	}
	else {
  		Pebble.sendAppMessage({
          "WEATHER_WEATHERDATA":"GPS!",
          "WEATHER_MISC3":"gps error"
  		});
	}
}

//======================================
// DEFAULT CONFIG
//======================================
function defaultConfig() {
	o={};
    o["CONFIG_REVERSE"]=0;		// black background layout
	o["CONFIG_24H"]=1; 			// use 24 hour time
	o["CONFIG_SECONDS"]=0;      // show second hand
	o["CONFIG_HOURVIBES"]=0;	// don't vibrate on the hour
	o["CONFIG_FAHRENHEIT"]=0;   // use metric units
    o["CONFIG_CITYID"]=0;		// no city ID, use city name or GPS
	o["CONFIG_SETCITY"]="";		// no set location, use GPS
    o["CONFIG_ALTDATA"]=0;      // sunrise/sunset data
	return o;
}

//======================================
// SEND CONFIG TO WATCH
//======================================
function sendConfig() {
    Pebble.sendAppMessage({
      "CONFIG_SECONDS":config.CONFIG_SECONDS,
      "CONFIG_HOURVIBES":config.CONFIG_HOURVIBES,
      "CONFIG_REVERSE": config.CONFIG_REVERSE,
      });
}

//======================================
// ON INITIAL LOAD
//======================================
Pebble.addEventListener("ready", function(e) {
	// load config from phone storage
	var e=localStorage.getItem("roundyconfig");
	"string" == typeof e && (config=JSON.parse(e));
	if (JSON.stringify(config) == "{}") {
		//load default config
		//console.log("loading default config");
		config = defaultConfig();
	}
	console.log("config is " + JSON.stringify(config));

	// load cached data if less than 60 minutes, otherwise fetch fresh data
	var current_time = new Date();
	var weather_timestamp = new Date(localStorage.getItem("weather_timestamp"));
	var timediff = Math.floor(current_time.getTime()/1000/60) - Math.floor(weather_timestamp.getTime()/1000/60);
	console.log("Time diff is " + timediff);
	if (timediff < 60) {
//	  	console.log("Within refresh limit, loading cached data");
		readCachedData();
	}
	else {		
		// load weather using favorite city name, else use GPS location
		if ((config.CONFIG_SETCITY != "") && (config.CONFIG_SETCITY != "undefined")) {
//			console.log("Loading city config for " + config.CONFIG_SETCITY);
			fetchWeather(config.CONFIG_SETCITY, null, null);
		}
		else {
//			console.log("Loading with gps");
			locationWatcher = window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
		}
	}
});

//======================================
// ON APPMESSAGE RECEIVED - fetch fresh data
//======================================
Pebble.addEventListener("appmessage", function(e) {	
	if ((config.CONFIG_SETCITY != "") && (config.CONFIG_SETCITY != "undefined")) {
//        console.log("Forced reload with set data");
        fetchWeather(config.CONFIG_SETCITY, null, null);
	}
	else {
//        console.log("Forced reload with GPS");
		locationWatcher = window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
	}
});
 
//======================================
// CONFIGURATION WINDOW - hack to show offline HTML page
// Thanks to Tom Gidden, loads current settings too
//======================================
Pebble.addEventListener("showConfiguration", function(e) {
	prevcity = config.CONFIG_SETCITY; // set previous city before opening config window
	Pebble.openURL("data:text/html,"+encodeURIComponent(
	'<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"></head><body><header><h1><span>Roundy 2.2</span></h1></header><form onsubmit="return s(this)"><p><input type="checkbox" id="CONFIG_REVERSE" class="showhide"><label for="CONFIG_REVERSE">Reversed layout with white background<br>(default is black background)</label><p><input type="checkbox" id="CONFIG_24H" class="showhide"><label for="CONFIG_24H">24 hour time<br>(default is 12 hour am/pm time)</label><p><input type="checkbox" id="CONFIG_SECONDS" class="showhide"><label for="CONFIG_SECONDS">Show second hand</label><p><input type="checkbox" id="CONFIG_HOURVIBES" class="showhide"><label for="CONFIG_HOURVIBES">Vibrate at the start of every hour</label><p><input type="checkbox" id="CONFIG_FAHRENHEIT" class="showhide"><label for="CONFIG_FAHRENHEIT">Use Fahrenheit for temperature and mph for wind speed<br>(default is Centigrade and km/h)</label><p><input type="checkbox" id="CONFIG_ALTDATA" class="showhide"><label for="CONFIG_ALTDATA">Show humidity and windspeed at bottom corners<br>(default is sunrise and sunset times)</label><p><input type="checkbox" id="CONFIG_CITYID" class="showhide"><label for="CONFIG_CITYID">Use OpenWeathermap city ID<br>(default is to search for city name)</label><p><input type="text" id="CONFIG_SETCITY" class="showhide"><label for="CONFIG_SETCITY"><br>Set location (leave empty to use GPS)</label><p><p><input type="submit" value="Save Settings"></form><p><footer>By Mango Lazi</footer><script>function s(e){o={};o["CONFIG_REVERSE"]=document.getElementById("CONFIG_REVERSE").checked?1:0;o["CONFIG_24H"]=document.getElementById("CONFIG_24H").checked?1:0;o["CONFIG_SECONDS"]=document.getElementById("CONFIG_SECONDS").checked?1:0;o["CONFIG_HOURVIBES"]=document.getElementById("CONFIG_HOURVIBES").checked?1:0;o["CONFIG_FAHRENHEIT"]=document.getElementById("CONFIG_FAHRENHEIT").checked?1:0;o["CONFIG_ALTDATA"]=document.getElementById("CONFIG_ALTDATA").checked?1:0;o["CONFIG_CITYID"]=document.getElementById("CONFIG_CITYID").checked?1:0;o["CONFIG_SETCITY"]=document.getElementById("CONFIG_SETCITY").value;return window.location.href="pebblejs://close#"+JSON.stringify(o),!1}var d="_CONFDATA_";for(var i in d)d.hasOwnProperty(i)&&(document.getElementById(i).checked=d[i]);document.getElementById("CONFIG_SETCITY").value=d[i];</script></body></html>\n<!--.html'.replace('"_CONFDATA_"',JSON.stringify(config),"g")))});

//var options = { color: 'white', border: true };
//document.location = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(options));
//return window.location.href="pebblejs://close#"+JSON.stringify(o),!1}var d="_CONFDATA_";for(var i in d)d.hasOwnProperty(i)&&(document.getElementById(i).checked=d[i]);document.getElementById("CONFIG_SETCITY").value=d[i];</script></body></html>\n<!--.html'.replace('"_CONFDATA_"',JSON.stringify(config),"g")))});

//======================================
// ON CONFIGURATION WINDOW CLOSED - sync settings with watch
//======================================
Pebble.addEventListener("webviewclosed", function(e) {
	"string"==typeof e.response && e.response.length>0 && (config=JSON.parse(e.response),localStorage.setItem("roundyconfig",e.response));
	//console.log("New config " + JSON.stringify(e.response));
    sendConfig();
    if (config.CONFIG_SETCITY != "") {
		if (config.CONFIG_SETCITY != prevcity) {
			fetchWeather(config.CONFIG_SETCITY, null, null);
		}
        else {
            readCachedData();
        }
	}
	else {
		locationWatcher = window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
	}
});