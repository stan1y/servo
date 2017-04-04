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
			uri = this.baseurl + "/" + encodeURIComponent(key),
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
			headers['Content-Type'] = 'application/json';
			headers['Accept'] = 'application/json';
			if (opts.body) {
				req['body'] = JSON.stringify(opts.body);
			}
		}
		else if (opts.type == 'binary') {
			throw "Not supported";
		}
		else {
			throw "Unknown type: " + opts.type;
		}
		req.headers = headers;
		
		return request(req, function(err, xhr, body) {
			if (!self.authHeader)
				if (xhr.getResponseHeader)
					self.authHeader = xhr.getResponseHeader("authorization");
				if (xhr.headers)
					self.authHeader = xhr.headers["authorization"];
			
			try { body = (body != null ? JSON.parse(body) : {code:0, message:err}); }
			catch(e) {}

			if (err && opts.error) {
				opts.error.apply(this, [body, xhr]);
			}
			else if (typeof body == 'object' && body.code && body.message) {
				opts.error.apply(this, [body, xhr]);	
			}
			else if (opts.success) {
				opts.success.apply(this, [body, xhr]);
			}
		});
	}

	ServoClient.prototype.get = function(key, opts) {
		return this.do('GET', key, opts || {});
	}

	ServoClient.prototype.post = function(key, opts) {
		if (opts == undefined) {
			throw "No body or options given to post.";
		}
		if (typeof opts == "string") {
			opts = {
				'body': opts,
				'type': 'text'
			};
		}
		return this.do('POST', key, opts);
	}

	ServoClient.prototype.put = function(key, opts) {
		if (opts == undefined) {
			throw "No body or options given to put.";
		}
		if (typeof opts == "string") {
			opts = {
				'body': opts,
				'type': 'text'
			};
		}
		return this.do('PUT', key, opts);
	}

	ServoClient.prototype.delete = function(key) {
		return this.do('DELETE', key, {});
	}

	exports.Servo = function(baseurl) {
		return new ServoClient(baseurl);
	}


}(typeof exports === 'object' && exports || this));
