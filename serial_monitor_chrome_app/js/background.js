/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

chrome.app.runtime.onLaunched.addListener(function () {
	showStartupWindow();
});

function showStartupWindow() {
	const mainWindowOptions = {
		id: "RS485ProtoMain",
		innerBounds: {
			width: 800,
			height: 600,
			minWidth: 800,
			minHeight: 600
		},
		focused: true
	};

	chrome.app.window.create('index.html', mainWindowOptions, function (createdWindow) {

	});
}