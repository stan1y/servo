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
    s.post('foo', {
      type: 'string',
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
  },

  get: function(test) {
    var s = servo.Servo(servoUrl);
    // post an item
    s.post('bar', {
      type: 'string',
      body: 'bar-bar',    
      success: function(body, req) {
        test.equal(req.statusCode, 201);

        // get it back and compare
        s.get('bar', {
          success: function(body, req) {
            test.equal(req.statusCode, 200);
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
