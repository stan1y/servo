'use strict';

var servo = require('../lib/servo.js');

/*
  ======== A Handy Little Nodeunit Reference ========
  https://github.com/caolan/nodeunit

  Test methods:
    test.expect(numAssertions)
    test.done()
  Test assertions:
    test.ok(value, [message])
    test.equal(actual, expected, [message])
    test.notEqual(actual, expected, [message])
    test.deepEqual(actual, expected, [message])
    test.notDeepEqual(actual, expected, [message])
    test.strictEqual(actual, expected, [message])
    test.notStrictEqual(actual, expected, [message])
    test.throws(block, [error], [message])
    test.doesNotThrow(block, [error], [message])
    test.ifError(value)
*/

var servoUrl = 'https://localhost:8080',
  appId = 'the-app-id',
  appKey = 'SuPeR$eCrEt',
  authMode = 'HS512';


var sleepFor = function(duration) {
    var now = new Date().getTime();
    while(new Date().getTime() < now + duration) {} 
}

exports['servo_tests'] = {

  setUp: function(done) {
    // fixme: run servo instance here
    done();
  },

  constuct: function(test) {
    // empty ctor
    var s = servo.Servo();
    test.equal(s.baseurl, undefined, 'baseurl should be empty.');
    s.setUrl(servoUrl);
    test.equal(s.baseurl, servoUrl, 'baseurl should match after setUrl.');

    // anon ctor
    s = servo.Servo(servoUrl);
    test.equal(s.baseurl, servoUrl, 'baseurl should match.');
    test.equal(s.algmode, 'none', 'should be anonymous.');

    // auth ctor
    s = servo.Servo(servoUrl).auth(appId, appKey);
    test.equal(s.baseurl, servoUrl, 'baseurl should match.');
    test.equal(s.appid, appId, 'appid should match.');
    test.equal(s.appkey, appKey, 'appkey should match.');
    test.equal(s.algmode, 'HS256', 'default algmode should match.');

    // custom algmode ctor
    s = servo.Servo(servoUrl).auth(appId, appKey, authMode);
    test.equal(s.baseurl, servoUrl, 'baseurl should match.');
    test.equal(s.appid, appId, 'appid should match.');
    test.equal(s.appkey, appKey, 'appkey should match.');
    test.equal(s.algmode, authMode, 'default algmode should match.');

    test.done();
  },

  post: function(test) {
    var s = servo.Servo(servoUrl);

    // post text
    s.post('foo', {
      type: 'text',
      body: 'foo-foo',
      success: function(body, req) {
        test.equal(req.statusCode, 201);
        test.done();
      },
      error: function(err) {
        test.ok(false, 'post failed: ' + err.message);
        test.done();
      }
    });

    // post with defaults
    s.post('post-default', 'the-default');
  },

  post_json: function(test) {
    var s = servo.Servo(servoUrl);

    // post with json
    s.post('json-key', {
      type: 'json',
      body: {a:1, b:2},
      success: function(body, req) {
        test.equal(req.statusCode, 201);
        test.done();
      },
      error: function(err) {
        test.ok(false, 'post failed: ' + err.message);
        test.done();
      }
    });
  },

  post_get: function(test) {
    var s = servo.Servo(servoUrl);
    // post an item
    s.post('bar', {
      type: 'text',
      body: 'bar-bar',    
      success: function(body, req) {
          
        // check response is expected
        test.equal(req.statusCode, 201, "unexpected response on post");
        // check response has given us auth header
        test.ok(s.authHeader != null, "no auth header received");

        // get it back and compare
        s.get('bar', {
          success: function(body, req) {
            test.equal(req.statusCode, 200, "unexpected. response on get");
            test.equal(body, 'bar-bar', 'returned string does not match');
            test.done();
          },
          error: function(err, req) {
            test.ok(false, 'get failed: ' + err.message);
            test.done();
          }
        });
      },
      error: function(err, req) {
        test.ok(false, 'post failed: ' + err.message);
        test.done();
      }
    });
  }

};
