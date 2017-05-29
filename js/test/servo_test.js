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

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
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
        test.ok(false, 'post failed: ' + err);
        test.done();
      }
    });
  },

  /*defaults: function(test) {
    var s = servo.Servo(servoUrl);
    s.post('default-test', 'the-default-value');
    sleepFor(1000);
    console.log(s);
    s.put('default-test', 'modified-value');
    sleepFor(1000);
    console.log(s);
    s.get('default-test', {
      success: function(body, req) {
        test.equal(req.statusCode, 200, 'unexpected status in get default');
        test.equal(body, 'modified-value', 'unpexpected value returned in get default');
        test.done(); 
      },
      error: function(err) {
        test.ok(false, 'get failed: ' + err.message);
        test.done(); 
      }
    });

  },*/

  post_get_file_multipart: function (test) {
    var s = servo.Servo(servoUrl);
    s.upload('test-upload', 'file.txt', {
      success: function(body, req) {
        test.equal(req.statusCode, 201, 'unexpected status on upload(post)');
        test.done();
      },
      error: function(err) {
        test.ok(false, 'upload failed: ' + err);
        test.done();
      }
    });
  },

  post_get_json: function(test) {
    var s = servo.Servo(servoUrl),
        json_data = {a:1, b:2};

    // post with json
    s.post('json-key', {
      type: 'json',
      body: json_data,
      success: function(body, req) {
        test.equal(req.statusCode, 201, 'unexpected status on json post');
        test.ok(s.authHeader != null, 'no auth header received');

        s.get('json-key', {
          type: 'json',
          success: function(body, req) {
            test.equal(req.statusCode, 200, 'unexpected status on json get');
            for(var k in json_data) {
              test.equal(json_data[k], body[k], 'returned json does not match: '  
                + json_data[k] + '!=' + body[k]);
            }
            test.done();  
          },
          error: function(err) {
            test.ok(false, 'json get failed:' + err.message);
          }
        });
        test.done();
      },
      error: function(err) {
        test.ok(false, 'json post failed: ' + err.message);
        test.done();
      }
    });
  },

  post_get_text: function(test) {
    var s = servo.Servo(servoUrl);
    // post an item
    s.post('bar', {
      type: 'text',
      body: 'bar-bar',    
      success: function(body, req) {
        test.equal(req.statusCode, 201, 'unexpected status on text post');
        test.ok(s.authHeader != null, 'no auth header received');

        s.get('bar', {
          success: function(body, req) {
            test.equal(req.statusCode, 200, 'unexpected status on text get');
            test.equal(body, 'bar-bar', 'returned string does not match');
            test.done();
          },
          error: function(err, req) {
            test.ok(false, 'get text failed: ' + err.message);
            test.done();
          }
        });
      },
      error: function(err, req) {
        test.ok(false, 'post text failed: ' + err.message);
        test.done();
      }
    });
  }

};
