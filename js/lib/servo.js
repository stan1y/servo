/*
 * Servo client API implementation
 * https://github.com/stan1y/servo
 *
 * This is an example of Servo API usage with
 * browser-request library.
 * https://www.npmjs.com/package/browser-request
 *
 * Copyright (c) 2017 Stanislav Yudin
 * Licensed under the MIT license.
 */

(function(exports) {

	'use strict';

	/* Utilities */

	function isBrowser() {
		try {
			return this === window;
		}
		catch(e) {
			return false;
		}
	}

	function toBuffer(ab) {
		var buffer = new Buffer(ab.byteLength);
		var view = new Uint8Array(ab);
		for (var i = 0; i < buffer.length; ++i) {
			buffer[i] = view[i];
		}
		return buffer;
	}

	/* Imports */
	
	if (isBrowser()) {
		var Request = require('browser-request');
	}
	else {
		var Request = require('request');
	}
	var FileSystem = require('fs'),
		  FormData = require('form-data');

	/* Servo client class */

	function ServoClient(baseurl) {
		this.baseurl = baseurl;
		this.algmode = 'none';
		return this;
	}

	ServoClient.prototype.setUrl = function(baseurl) {
		this.baseurl = baseurl;
	}

	ServoClient.prototype.auth = function(appid, appkey, algmode) {
		this.appid = appid;
		this.appkey = appkey;
		this.algmode = algmode || 'HS256';
		return this;
	}

	ServoClient.prototype.buildAuthHeader = function() {
		return 'Fake-Auth-Header';
	}

	ServoClient.prototype.do = function(method, key, opts) {
		var path = key.startsWith('/', key) ? key : ('/' + key),
			url = this.baseurl + path,
			req = {
				method: method,
				url: url,
				rejectUnauthorized: false,
				requestCert: true,
				agent: false,
				headers: {}
			},
			self = this;

		var requestCallback = function(err, xhr, body) {
			if (!self.authHeader) {
				if (xhr && xhr.getResponseHeader)
					self.authHeader = xhr.getResponseHeader('authorization');
				if (xhr && xhr.headers)
					self.authHeader = xhr.headers['authorization'];
			}

			if (!err && self.authHeader == undefined) {
				throw 'No auth header assigned';
			}
			
			// expected json ?
			if (req.headers['Accept'] == 'application/json' && body) {
				try { 
					body = JSON.parse(body);
				}
				catch(e) {
					body = {code: 0, message: 'invalid json: ' + e };
				}
			}

			// received an exception from servo ?
			if (typeof body == 'object' && body.code && body.message) {
				if (body.code != 200 && body.code != 201) {
					err = 'Servo exception! ' + body.code + ': ' + body.message;
				}
			}

			if (err && opts.error) {
				console.log('request failed:' + err);
				opts.error.apply(this, [err]);
			}
			else if (opts.success) {
				opts.success.apply(this, [body, xhr]);
			}
		};

		// prepare request object
		if (this.authHeader) {
			req.headers['Authorization'] = this.authHeader;
		}

		if (!opts.type || opts.type == 'text') {
			req.headers['Content-Type'] = 'text/plain';
			req.headers['Accept'] = 'text/plain';
			if (opts.body) {
				req.body = opts.body;
			}
		}
		else if (opts.type == 'json') {
			req.headers['Accept'] = 'application/json';
			if (opts.body && typeof opts.body == 'object') {
				req.headers['Content-Type'] = 'application/json';
				req.body = JSON.stringify(opts.body);
			}
		}
		else if (opts.type == 'file') {
			req.headers['Accept'] = 'multipart/form-data';
			if (opts.body && typeof opts.body == 'string') {
				req.headers['Content-Type'] = 'multipart/form-data';
				req.formData = { file: FileSystem.createReadStream(opts.body) };
			}
		}
		else {
			throw 'Unknown type: ' + opts.type;
		}

		// make request with callback & request object
		Request(req, requestCallback);
	}

	ServoClient.prototype.upload = function(key, opts) {
		if (opts == undefined) {
			throw 'No filename or options given to form-data POST.';
		}
		if (typeof opts == 'string') {
			// opts are a filename 
			return this.do('POST', key, {
				'type': 'file',
				'body': opts
			});
		}
		if (typeof opts == 'object') {
			return this.do('POST', key, opts);
		}
		throw 'Unexpected type of options argument.';
	}

	ServoClient.prototype.get = function(key, opts) {
		if (opts == undefined || typeof opts != 'object') {
			throw 'No options given or options type is incorrect.';
		}
		return this.do('GET', key, opts);
	}

	ServoClient.prototype.post = function(key, opts) {
		if (opts == undefined) {
			throw 'No body or options given to POST.';
		}
		if (typeof opts == 'string') {
			return this.do('POST', key, {
				body: opts,
				type: 'text'
			});
		}
		if (typeof opts == 'object') {
			return this.do('POST', key, opts);
		}
		throw 'Unexpected type of options argument.';
	}

	ServoClient.prototype.put = function(key, opts) {
		if (opts == undefined) {
			throw 'No body or options given to PUT.';
		}
		if (typeof opts == 'string') {
			return this.do('PUT', key, {
				'body': opts,
				'type': 'text'
			});
		}
		if (typeof opts == 'object') {
			return this.do('PUT', key, opts);
		}
		throw 'Unexpected type of options argument.';
	}

	ServoClient.prototype.delete = function(key) {
		return this.do('DELETE', key, {});
	}

	exports.Servo = function(baseurl) {
		return new ServoClient(baseurl);
	}


}(typeof exports === 'object' && exports || this));
