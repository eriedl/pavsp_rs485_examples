/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

/*
	Extends the left array with the right array
 */
function extendArray(array1, array2) {
	array1.push.apply(array1, array2);
}

/*
 Thank you fearphage: http://stackoverflow.com/questions/610406/javascript-equivalent-to-printf-string-format/4673436#4673436
 */
function formatString(format) {
	var args = Array.prototype.slice.call(arguments, 1);
	return format.replace(/{(\d+)}/g, function (match, number) {
		return typeof args[number] !== 'undefined' && args[number] !== null ? args[number] : "";
	});
}

/*
 http://stackoverflow.com/questions/7285296/what-is-the-best-way-to-set-a-particular-bit-in-a-variable-in-c
 #define BIT_MASK(bit)             (1 << (bit))
 #define SET_BIT(value,bit)        ((value) |= BIT_MASK(bit))
 #define CLEAR_BIT(value,bit)      ((value) &= ~BIT_MASK(bit))
 #define TEST_BIT(value,bit)       (((value) & BIT_MASK(bit)) ? 1 : 0)
 shareedit
 answered Sep 2 '11 at 15:02

 Brandon E Taylor
 12.9k32649
 */
function bitMask(bit) {
	return (1 << bit) >>> 0;
}

function setBit(value, bit) {
	return (value | bitMask(bit)) >>> 0;
}

function clearBit(value, bit) {
	return (value & (~bitMask(bit) >>> 0)) >>> 0;
}

function testBit(value, bit) {
	var _bitMask = bitMask(bit);
	return (((value & _bitMask) >>> 0) === _bitMask);
}

// JS can only do bit shifting with 32-bit numbers
function leftShift(num, bits) {
	return num * Math.pow(2, bits);
}

function rightShift(num, bits) {
	if (num < 0) {
		return Math.ceil(num / Math.pow(2, bits));
	} else {
		return Math.floor(num / Math.pow(2, bits));
	}
}

// Most functions are based on https://github.com/google/closure-library/blob/master/closure/goog/crypt/crypt.js
function byteArrayToHex(array) {
	return array.map(function (byte) {
		var hexByte = byte.toString(16);

		return hexByte.length > 1 ? hexByte : '0' + hexByte;
	}).join('');
}

function hexToByteArray(hexString) {
	if(hexString.length % 2 !== 0) {
		//throw new Error('hexString length must be multiple of 2');
		hexString += " ";
	}

	var ret = [];

	for (var i = 0; i < hexString.length; i += 2) {
		ret.push(parseInt(hexString.substring(i, i + 2), 16));
	}

	return ret;
}

function utf8ByteArrayToString(bytes) {
	// TODO(user): Use native implementations if/when available
	var out = [], pos = 0, c = 0;
	while (pos < bytes.length) {
		var c1 = bytes[pos++];
		if (c1 < 128) {
			out[c++] = String.fromCharCode(c1);
		} else if (c1 > 191 && c1 < 224) {
			var c2 = bytes[pos++];
			out[c++] = String.fromCharCode((c1 & 31) << 6 | c2 & 63);
		} else {
			var c2 = bytes[pos++];
			var c3 = bytes[pos++];
			out[c++] = String.fromCharCode(
					(c1 & 15) << 12 | (c2 & 63) << 6 | c3 & 63);
		}
	}
	return out.join('');
}

function stringToUtf8ByteArray(str) {
	// TODO(user): Use native implementations if/when available
	var out = [], p = 0;

	for (var i = 0; i < str.length; i++) {
		var c = str.charCodeAt(i);

		if (c < 128) {
			out[p++] = c;
		} else if (c < 2048) {
			out[p++] = (c >> 6) | 192;
			out[p++] = (c & 63) | 128;
		} else {
			out[p++] = (c >> 12) | 224;
			out[p++] = ((c >> 6) & 63) | 128;
			out[p++] = (c & 63) | 128;
		}
	}

	return out;
}

function numberToHexString(number, size, littleEndian) {
	var s = number.toString(16);
	size = (size ? size * 2 : 8); //we default to 4 bytes ===> 1 byte is 2 characters
	var ret = (Array(size).join('0') + s).slice(-size);

	if (littleEndian === true) {
		var hexLE = "";
		for (var i = ret.length; i > 0; i -= 2) {
			hexLE += ret.slice(i - 2, i);
		}
		ret = hexLE;
	}

	return ret;
}

function hexStringToNumber(hexString, littleEndian) {
	var ret = Number.NaN;
	var s = hexString;

	if (s.trim().length === 0) {
		return ret;
	}

	if (littleEndian === true) {
		var hexBE = "";
		for (var i = s.length; i > 0; i -= 2) {
			hexBE += s.slice(i - 2, i);
		}
		s = hexBE;
	}

	ret = parseInt(s, 16);

	return ret;
}
