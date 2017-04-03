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
    console.log('using browser-request');
  }
  else {
    var request = require('request');
    console.log('using native request');
  }

  function ServoClient(baseurl) {
    this.baseurl = baseurl;
    this.algmode = 'none';
    return this;
  };

  ServoClient.prototype.auth = function(appid, appkey, algmode) {
    this.appid = appid;
    this.appkey = appkey;
    this.algmode = algmode || 'HS256';
    return this;
  };

  ServoClient.prototype.get = function(key, type) {
    var headers = {},
        uri = this.baseurl + "/" + encodeURIComponent(key);

    if (this.appid && this.appkey) {
      headers['Authentication'] = this.buildAuthHeader();
    }

    request({
      method: 'GET',
      uri: uri,
      headers: headers
    });

  };

  ServoClient.prototype.post = function(key, item) {
  };

  ServoClient.prototype.put = function(key, item) {
  };

  ServoClient.prototype.delete = function(key) {
  };

  exports.Servo = function(baseurl) {
    return new ServoClient(baseurl);
  };


}(typeof exports === 'object' && exports || this));
