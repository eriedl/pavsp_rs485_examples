/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

$(document).ready(function (e) {
	var pnlLogContainer = $('#pnlLogContainer');
	var serialReceiveCB = function (info) {
		// TODO: Read ArrayBuffer as HEX string
		var msgReceived = new Uint8Array(info.data);
		var serialMsgBuf = '';
		var lastCharLFCR = false;

		for (var i = 0; i < msgReceived.length; ++i) {
			if (msgReceived[i] === 0x0A || msgReceived[i] === 0x0D) {
				if (lastCharLFCR === false) {
					lastCharLFCR = true;
					serialMsgBuf += '<br>';
				}
			} else {
				lastCharLFCR = false;
				// human-readable alpha numberic character 'Space' thru '~'
				if (msgReceived[i] >= 0x20 && msgReceived[i] <= 0x7E) {
					serialMsgBuf += String.fromCharCode(msgReceived[i]);
				} else {
					serialMsgBuf += ((msgReceived[i] < 0x10) ? '0' + msgReceived[i].toString(16) : msgReceived[i].toString(16));
				}
			}
		}

		pnlLogContainer.append(serialMsgBuf).scrollTop(pnlLogContainer[0].scrollHeight);
	};

	function getDefaultConnectionOptions() {
		var opt = {
			"persistent":     false,
			"name":           "Pentair Air COM",
			"bufferSize":     4096,
			"bitrate":        9600,
			"dataBits":       chrome.serial.DataBits.EIGHT,
			"parityBit":      chrome.serial.ParityBit.NO,
			"stopBits":       chrome.serial.StopBits.ONE,
			"ctsFlowControl": false, // Need this explicitly inform the pump to switch to Receive mode
			"receiveTimeout": 0,
			"sendTimeout":    0
		};

		return JSON.parse(JSON.stringify(opt));
	}

	var _prevSelectedDevice = null;
	var _activeConnection = null;
	$('#lstConnectedDevices').on('dblclick', null, null, function (e) {
		var lstConnectedDevices = $(e.currentTarget);

		return Promise.resolve().then(function () {
			if (_activeConnection !== null) {
				chrome.serial.onReceive.removeListener(serialReceiveCB);

				return chromeSerialDisconnect(_activeConnection.connectionId).then(function (result) {
					$('#pnlLogContainer').append(formatString('<p>Disconnected from serial port at {0}</p>', _prevSelectedDevice));
					_activeConnection = null;
					lstConnectedDevices.find('.active').removeClass('active');

					return null;
				});
			}
		}).then(function () {
			if (_prevSelectedDevice !== e.currentTarget.value) {
				chrome.serial.onReceive.addListener(serialReceiveCB);

				return chromeSerialConnect(e.currentTarget.value, getDefaultConnectionOptions()).then(function (connectionInfo) {
					$('#pnlLogContainer').append(formatString('<p>Connected to serial port at {0}</p>', e.currentTarget.value));
					$('#btnRefreshDeviceList').prop('disabled', true);
					_activeConnection = connectionInfo;
					_prevSelectedDevice = e.currentTarget.value;
					lstConnectedDevices.find(formatString('option[value="{0}"]', e.currentTarget.value)).addClass('active');

					console.info(_activeConnection);

					return null;
				});
			} else {
				_prevSelectedDevice = null;

				return null;
			}
		}).catch(function (e) {
			console.error(e);
			$('#pnlLogContainer').append(formatString('<p class="logError">{0}</p>', e.message));

			chrome.serial.onReceive.removeListener(serialReceiveCB);
			_activeConnection = null;
			_prevSelectedDevice = null;
			$('#btnRefreshDeviceList').prop('disabled', false);
		});
	});

	$('#btnRefreshDeviceList').on('click', null, null, function (e) {
		return chromeSerialGetDevicesAsync().then(function (serialPorts) {
			const NO_DEVICE_FOUND = '<option value="none">No Device Found</option>';
			var lstConnectedDevices = $('#lstConnectedDevices');
			var ddlCommandPort = $('#ddlCommandPort');
			var html = '';

			serialPorts.forEach(function (seriapPort, index, array) {
				if (seriapPort.path.startsWith('/dev/cu') === false && seriapPort.path.startsWith('COM') === false) {
					return;
				}

				html += formatString('<option value="{0}">{0}</option>', seriapPort.path);
			});

			if (html.length > 0) {
				lstConnectedDevices.html(html);
				ddlCommandPort.html(html);
				$('#btnSendCommand').prop('disabled', false);
			} else {
				lstConnectedDevices.html(NO_DEVICE_FOUND);
				ddlCommandPort.html(NO_DEVICE_FOUND);
				$('#btnSendCommand').prop('disabled', true);
			}
		})
	});

	$('#btnSendCommand').on('click', null, null, function (e) {
		var cmd = $('#txtSerialCommand').val();

		if (cmd.length === 0) {
			return;
		}

		var barr = [];
		// Convert command string to bytes
		for (var i = 0; i < cmd.length; ++i) {
			barr.push(cmd.charCodeAt(i));
		}
		var uint8Array = new Uint8Array(barr);

		var selPort = $('#ddlCommandPort').val();
		var tmpConnectionInfo = null;
		var destIsSrc = (selPort === _prevSelectedDevice);

		return Promise.resolve().then(function () {
			if (destIsSrc === true) {
				tmpConnectionInfo = _activeConnection;
			} else {
				return chromeSerialConnect(selPort, getDefaultConnectionOptions()).then(function (connectionInfo) {
					tmpConnectionInfo = connectionInfo;
				});
			}

			return null;
		}).then(function () {
			// Request sending
			return chromeSerialSetControlSignals(tmpConnectionInfo.connectionId, {
				"dtr": true,
				"rts": true
			});
		}).then(function (result) {
			console.info("Request to send result: " + String(result));

			return chromeSerialSend(tmpConnectionInfo.connectionId, uint8Array.buffer);
		}).then(function () {
			$('#pnlLogContainer').append(formatString('<p>Sent command to {0}: [{1}]</p>', selPort
				, uint8Array.join(", ")));

			return chromeSerialFlush(tmpConnectionInfo.connectionId);
		}).then(function () {
			return chromeSerialSetControlSignals(tmpConnectionInfo.connectionId, {
				"dtr": true,
				"rts": false
			});
		}).then(function (result) {
			console.info("Ready to receive: " + String(result));

			if (destIsSrc === false) {
				return chromeSerialDisconnect(tmpConnectionInfo.connectionId);
			} else {
				return null;
			}
		}).then(function () {
			tmpConnectionInfo = null;
		}).catch(function (e) {
			console.error(e);
			$('#pnlLogContainer').append(formatString('<p class="logError">Couldn\'t send command: {0}</p>', e.message));

			if (tmpConnectionInfo !== null) {
				return chromeSerialDisconnect(tmpConnectionInfo.connectionId).catch(function (e) {
					// noop
				});
			}
		});
	});

	$('#btnClearLog').on('click', null, null, function (e) {
		pnlLogContainer.empty();
	})
});
