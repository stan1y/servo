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

  ServoClient.prototype.auth = function(appid, appkey) {
    this.appid = appid;
    this.appkey = appkey;
    return this;
  };

  ServoClient.prototype.alg = function(algmode) {
    this.algmode = algmode;
    return this;
  };

  ServoClient.prototype.get = function(itemkey) {
    return null;
  };

  ServoClient.prototype.post = function(itemkey, item) {
    /* item can be a string or an object */
    return null;
  };

  ServoClient.prototype.put = function(itemkey, item) {
    return null;
  };

  ServoClient.prototype.delete = function(itemkey) {
    return null;
  };

  exports.connect = function(baseurl) {
    return new ServoClient(baseurl);
  };


}(typeof exports === 'object' && exports || this));
