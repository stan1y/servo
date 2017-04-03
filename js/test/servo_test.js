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

exports['servo_tests'] = {
  setUp: function(done) {
    // fixme: run servo instance here
    done();
  },

  constuct: function(test) {
    var servoUrl = 'https://localhost:8080',
        appId = 'the-app-id',
        appKey = 'SuPeR$eCrEt',
        authMode = 'HS256',
        s = null;
    
    // anon ctor
    s = servo.connect(servoUrl);
    test.equal(s.baseurl, servoUrl, 'baseurl should match.');
    test.equal(s.algmode, 'none', 'should be anonymous.');

    // auth ctor
    s = servo.connect(servoUrl).alg(authMode).auth(appId, appKey);
    test.equal(s.baseurl, servoUrl, 'baseurl should match.');
    test.equal(s.appid, appId, 'appid should match.');
    test.equal(s.appkey, appKey, 'appkey should match.');
    test.equal(s.algmode, authMode, 'algmode should match.');

    test.done();
  }
};
