/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

//region chrome.fileSystem
function chromeChooseFileEntryAsync(fileOptions) {
	return new Promise(function (resolve, reject) {
		chrome.fileSystem.chooseEntry(fileOptions, function (fileEntry) {
			if (!fileEntry || chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(fileEntry);
			}
		});
	});
}

function chromeGetDisplayPathAsync(entry) {
	return new Promise(function (resolve, reject) {
		chrome.fileSystem.getDisplayPath(entry, function (displayPath) {
			if (!displayPath || chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(displayPath);
			}
		});
	});
}

function chromeOpenFileAsync(fileEntry) {
	return new Promise(function (resolve, reject) {
		fileEntry.file(function (file) {
			if (!file || chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(file);
			}
		});
	});
}

function chromeRestoreEntryAsync(id) {
	return new Promise(function (resolve, reject) {
		chrome.fileSystem.restoreEntry(id, function (entry) {
			if (chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(entry);
			}
		});
	});
}

function chromeRetainEntryAsync(entry) {
	return new Promise(function (resolve, reject) {
		try {
			var entryId = chrome.fileSystem.retainEntry(entry);
			resolve(entryId);
		} catch (e) {
			reject(new Error(e.message));
		}
	});
}
//endregion

//region chrome.storage
function chromeStorageLocalSetAsync(keyValuePairs) {
	return new Promise(function (resolve, reject) {
		chrome.storage.local.set(keyValuePairs, function () {
			if(chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve();
			}
		})
	});
}

function chromeStorageLocalGetAsync(keys) {
	return new Promise(function (resolve, reject) {
		chrome.storage.local.get(keys, function (items) {
			if (chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(items);
			}
		})
	});
}

function chromeStorageLocalRemoveAsync(keys) {
	return new Promise(function (resolve, reject) {
		chrome.storage.local.remove(keys, function () {
			if (chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve();
			}
		})
	});
}

function chromeStorageLocalClearAsync() {
	return new Promise(function (resolve, reject) {
		chrome.storage.local.clear(function () {
			if(chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve();
			}
		});
	});
}
//endregion

//region chrome.runtime
function chromeGetPackageDirectoryEntryAsync() {
	return new Promise(function (resolve, reject) {
		chrome.runtime.getPackageDirectoryEntry(function (directoryEntry) {
			if(!directoryEntry || chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(directoryEntry);
			}
		});
	});
}
//endregion

//region chrome.serial
function chromeSerialConnect(port, connectionOptions) {
	return new Promise(function (resolve, reject) {
		var cOptions = connectionOptions || {
			name:           chrome.runtime.getManifest().name,
			bitrate:        115200,
			ctsFlowControl: false
		};

		chrome.serial.connect(port, cOptions, function (connectionInfo) {
			if (!connectionInfo && chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(connectionInfo);
			}
		});
	});
}

function chromeSerialDisconnect(connectionId) {
	return new Promise(function (resolve, reject) {
		chrome.serial.disconnect(connectionId, function (result) {
			resolve(result);
		});
	});
}

function chromeSerialFlush(connectionId) {
	return new Promise(function (resolve, reject) {
		chrome.serial.flush(connectionId, function (result) {
			if (!result && chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(result);
			}
		});
	});
}

function chromeSerialGetControlSignals(connectionId) {
	return new Promise(function (resolve, reject) {
		chrome.serial.getControlSignals(connectionId, function (signals) {
			if (!signals && chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(signals);
			}
		});
	});
}

function chromeSerialGetDevicesAsync() {
	return new Promise(function (resolve, reject) {
		chrome.serial.getDevices(function (ports) {
			if (chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			}

			if (!ports || ports.length === 0) {
				resolve([]);
			} else {
				resolve(ports);
			}
		});
	});
}

function chromeSerialSend(connectionId, arrayBuffer) {
	return new Promise(function (resolve, reject) {
		chrome.serial.send(connectionId, arrayBuffer, function (sendInfo) {
			if (!sendInfo && chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(sendInfo);
			}
		});
	});
}

function chromeSerialSetControlSignals(connectionId, signals) {
	return new Promise(function (resolve, reject) {
		chrome.serial.setControlSignals(connectionId, signals, function (result) {
			if (!result && chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(result);
			}
		});
	});
}

function serialGenericOnReceiveErrorCallback(info) {
	var msg = "An error occurred while waiting for data on the serial port: ";
	switch (info.error) {
		case "disconnected":
			msg += "The connection was disconnected";
			break;
		case "timeout":
			msg += "No data has been received for receiveTimeout milliseconds.";
			break;
		case "device_lost":
			msg += "The device was most likely disconnected from the host.";
			break;
		case "system_error":
			msg += "A system error occurred and the connection may be unrecoverable.";
			break;
		default:
			msg += "Error 13.... Unknown..."
			break;
	}
	msg += " Connection ID " + info.connectionId.toString();

	console.error(msg);
};
//endregion

//region Generic file operations
const ChromeFileChooserDialogType = {
	OPEN_FILE:      "openFile",
	SAVE_FILE:      "saveFile",
	OPEN_DIRECTORY: "openDirectory"
};

function chromeGetFileSystemErrorMessage(e) {
	if (chrome.runtime.lastError) {
		return formatString(localizeHtml('__MSG_str_filesystem_operation_error__'), chrome.runtime.lastError);
	} else {
		return formatString('{0}: {1}', e.name, e.message);
	}
}

function chromeGetFileEntryAsync(directoryEntry, fileName, options) {
	return new Promise(function (resolve, reject) {
		directoryEntry.getFile(fileName, options, function (fileEntry) {
			if(!fileEntry || chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(fileEntry);
			}
		}, function (e) {
			reject(new Error(chromeGetFileSystemErrorMessage(e)));
		});
	});
}

function chromeGetDirectoryEntryAsync(directoryEntry, dirName, options) {
	return new Promise(function (resolve, reject) {
		directoryEntry.getDirectory(dirName, options, function (dirEntry) {
			if (!dirEntry || chrome.runtime.lastError) {
				reject(new Error(chrome.runtime.lastError.message));
			} else {
				resolve(dirEntry);
			}
		}, function (e) {
			reject(new Error(chromeGetFileSystemErrorMessage(e)));
		});
	});
}

function chromeReadFileAsTextAsync(fileEntry) {
	return chromeOpenFileAsync(fileEntry).then(function (file) {
		return new Promise(function (resolve, reject) {
			var fileReader = new FileReader();
			fileReader.onload = function (e) {
				resolve(fileReader.result);
			};
			fileReader.onerror = function (e) {
				reject(new Error(chromeGetFileSystemErrorMessage(e)));
			};

			fileReader.readAsText(file);
		});
	});
}

function chromeReadFileAsArrayBufferAsync(fileEntry) {
	return chromeOpenFileAsync(fileEntry).then(function (file) {
		return new Promise(function (resolve, reject) {
			var fileReader = new FileReader();
			fileReader.onload = function (e) {
				resolve(fileReader.result);
			};
			fileReader.onerror = function (e) {
				reject(new Error(chromeGetFileSystemErrorMessage(e)));
			};

			fileReader.readAsArrayBuffer(file);
		});
	});
}

function chromeReadFileAsBinaryStringAsync(fileEntry) {
	return chromeOpenFileAsync(fileEntry).then(function (file) {
		return new Promise(function (resolve, reject) {
			var fileReader = new FileReader();
			fileReader.onload = function (e) {
				resolve(fileReader.result);
			};
			fileReader.onerror = function (e) {
				reject(new Error(chromeGetFileSystemErrorMessage(e)));
			};

			fileReader.readAsBinaryString(file);
		});
	});
}

function chromeReadFileAsDataURLAsync(fileEntry) {
	return chromeOpenFileAsync(fileEntry).then(function (file) {
		return new Promise(function (resolve, reject) {
			var fileReader = new FileReader();
			fileReader.onload = function (e) {
				resolve(fileReader.result);
			};
			fileReader.onerror = function (e) {
				reject(new Error(chromeGetFileSystemErrorMessage(e)));
			};

			fileReader.readAsDataURL(file);
		});
	});
}

function chromeWriteFileAsync(fileEntry, data, mimeType) {
	return new Promise(function (resolve, reject) {
		fileEntry.createWriter(function (fileWriter) {
			var blob = new Blob([data], {type: mimeType});
			fileWriter.onerror = function (e) {
				reject(new Error(chromeGetFileSystemErrorMessage(e)));
			};
			var truncated = false;
			fileWriter.onwriteend = function () {
				if (!truncated) {
					truncated = true;
					this.truncate(this.position);
				}

				resolve(blob.size);
			};

			fileWriter.write(blob);
		}), function (e) {
			reject(new Error(chromeGetFileSystemErrorMessage(e)));
		};
	});
}

function chromeWriteBinaryFileAsync(fileEntry, dataArray) {
	return chromeWriteFileAsync(fileEntry, dataArray, "application/octet-stream");
}

function chromeWriteTextFileAsync(fileEntry, text) {
	return chromeWriteFileAsync(fileEntry, text, "text/plain");
}

function chromeFileExistsAsync(directoryEntry, fileName) {
	return chromeGetFileEntryAsync(directoryEntry, fileName, {create: false}).then(function (fileEntry) {
		return true;
	}).catch(function (e) {
		return false;
	})
}

function chromeDirectoryExistsAsync(directoryEntry, dirName) {
	return chromeGetDirectoryEntryAsync(directoryEntry, dirName, {create: false}).then(function (dirEntry) {
		return true;
	}).catch(function (e) {
		return false;
	})
}

function chromeReadDirectoryEntriesAsync(directoryEntry) {
	return new Promise(function (resolve, reject) {
		var dirReader = directoryEntry.createReader();
		var entries = [];

		/*
		 * Source: https://developer.mozilla.org/en/docs/Web/API/DirectoryReader
		 * 1. Call direcoryEntry.createReader() to create a new DirectoryReader.
		 * 2. Call readEntries().
		 * 3. Continue calling readEntries() until an empty array is returned. You have to do this because the API might not return all entries in a single call.
		 */
		// Keep calling readEntries() until no more results are returned.
		var readEntries = function () {
			dirReader.readEntries(function (results) {
				if (results.length === 0) {
					resolve(entries);
				} else {
					extendArray(entries, results);
					readEntries();
				}
			}, function (e) {
				reject(new Error(chromeGetFileSystemErrorMessage(e)));
			});
		};

		readEntries();
	});
}

function chromeRemoveDirectoryAsync(directoryEntry) {
	return new Promise(function (resolve, reject) {
		directoryEntry.removeRecursively(function () {
			resolve();
		}, function (e) {
			reject(new Error(chromeGetFileSystemErrorMessage(e)));
		})
	});
}

function chromeRemoveFileAsync(entry) {
	return new Promise(function (resolve, reject) {
		entry.remove(function () {
			resolve();
		}, function (e) {
			reject(new Error(chromeGetFileSystemErrorMessage(e)));
		})
	});
}

function chromeCopyEntryToAsync(entry, targetDirectory, name) {
	return new Promise(function (resolve, reject) {
		entry.copyTo(targetDirectory, name, resolve, function (e) {
			reject(new Error(chromeGetFileSystemErrorMessage(e)));
		});
	});
}

function chromeMoveEntryToAsync(entry, targetDirectory, name) {
	return new Promise(function (resolve, reject) {
		entry.moveTo(targetDirectory, name, resolve, function (e) {
			reject(new Error(chromeGetFileSystemErrorMessage(e)));
		});
	});
}
//endregion
