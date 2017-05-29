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

function isBrowser() {
	try {
		return this === window;
	}
	catch(e) {
		return false;
	}
};

(function(exports) {

	'use strict';

	
	if (isBrowser()) {
		var request = require('browser-request');
	}
	else {
		var request = require('request');
	}
	var FormData = require('form-data'),
		      fs = require('fs');

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
		var headers = {},
			form = new FormData(),
			path = key.startsWith('/', key) ? key : ('/' + key),
			uri = this.baseurl + path,
			req = {
				method: method,
				uri: uri,
				rejectUnauthorized: false,
				requestCert: true,
				agent: false
			},
			self = this;

		if (!this.authHeader && this.appid && this.appkey) {
			this.authHeader = this.buildAuthHeader();
		}
		if (this.authHeader) {
			headers['authorization'] = this.authHeader;
		}

		if (!opts.type || opts.type == 'text') {
			headers['Content-Type'] = 'text/plain';
			headers['Accept'] = 'text/plain';
			if (opts.body) {
				req['body'] = opts.body;
			}
		}
		else if (opts.type == 'json') {
			headers['Accept'] = 'application/json';
			if (opts.body && typeof opts.body == 'object') {
				headers['Content-Type'] = 'application/json';
				req['body'] = JSON.stringify(opts.body);
			}
		}
		else if (opts.type == 'form-data') {
			headers['Accept'] = 'multipart/form-data';
			if (opts.body && typeof opts.body == 'string') {
				var stream = fs.createReadStream(opts.body);
				headers['Content-Type'] = 'multipart/form-data';
				headers['Content-Length'] = 'multipart/form-data';
				form.append('file', stream);
				req.form = form;
			}
		}
		else {
			throw 'Unknown type: ' + opts.type;
		}

		req.headers = headers;
		
		return request(req, function(err, xhr, body) {
			if (!self.authHeader) {
				if (xhr && xhr.getResponseHeader)
					self.authHeader = xhr.getResponseHeader('authorization');
				if (xhr && xhr.headers)
					self.authHeader = xhr.headers['authorization'];
			}

			if (self.authHeader == undefined) {
				throw 'No auth header assigned';
			}
			
			if (headers['Accept'] == 'application/json' && body) {
				try { 
					body = JSON.parse(body);
				}
				catch(e) {
					body = {code: 0, message: 'invalid json: ' + e };
				}
			}

			if (typeof body == 'object' && body.code && body.message) {
				if (body.code != 200 && body.code != 201) {
					err = 'servo error code: ' + body.code + ', ' + body.message;
				}
			}

			if (err && opts.error) {
				console.log('error occured: ' + err);
				opts.error.apply(this, [body, xhr]);
			}
			else if (opts.success) {
				opts.success.apply(this, [body, xhr]);
			}
		});
	}

	ServoClient.prototype.upload = function(key, filename, opts) {
		// file can be sent as base64 or form-data
		if (typeof opts == 'string') {
			opts = {
				'type': opts
			};
		}
		if (opts == undefined) {
			opts = {
				'type': 'form-data'
			}
		}
		opts['body'] = filename;
		return this.do('POST', key, opts);
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
