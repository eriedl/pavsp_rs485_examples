/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

/*
 Thank you fearphage: http://stackoverflow.com/questions/610406/javascript-equivalent-to-printf-string-format/4673436#4673436
 */
function formatString(format) {
	var args = Array.prototype.slice.call(arguments, 1);
	return format.replace(/{(\d+)}/g, function (match, number) {
		return typeof args[number] !== 'undefined' && args[number] !== null ? args[number] : "";
	});
}

function serialRefreshConnectedDevices() {
	return chromeSerialGetDevicesAsync().then(function (ports) {
		return ports;
	});
}