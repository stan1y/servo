/*
 * Servo debug console
 * https://github.com/stan1y/servo
 * Minimalistic http server using 
 * express framework node.js
 *
 * Copyright (c) 2017 Stanislav Yudin
 * Licensed under the MIT license.
 */

var express = require('express'),
       path = require('path'),
	    app = express(),
	 client = path.join(__dirname, 'client'),
	   dist = path.join(__dirname, '..', 'dist');

app.set('port', process.env.PORT | 5000);
app.use(express.static(client));
app.use(express.static(dist));

app.listen(
	app.get('port'),
	function() {
		console.log('Servo Console listens port ' + 
			app.get('port'));
		console.log(' client path: ' + client);
		console.log(' distribution: ' + dist);
});