/*
 * Servo Debug Console
 * https://github.com/stan1y/servo
 *
 * This is an example of Servo client javascript library.
 *
 * Copyright (c) 2017 Stanislav Yudin
 * Licensed under the MIT license.
 */

var servo = require('servo');

$(document).ready(function() {
	'use strict';

	var sendBtn = $("#send"),
		resetBtn = $("#reset"),
		methodSel = $("input#method"),
		body = $("textarea#body"),
		s = servo.Servo();

	var reset = function() {
		$("#alert-warn").hide();
		$("#alert-error").hide();
		$("#alert-info").hide();
		$("#alert-error").children(".details").html("");
		$("#alert-info").children(".details").html("");
		$("div.form-group").removeClass("has-error");
	};

	var send = function(s, clientId, method, itemKey, dataType, data) {
		s.do(method, itemKey, {
			type: dataType,
			body: data,
			success: function(body, xhr) {
				$("#alert-info").children(".details").html(
					"<strong>" + xhr.status + ": " + xhr.statusText + "</strong>");
				$("#alert-info").show();
				$("#response").val(JSON.stringify(body));
			},
			error: function(err, xhr) {
				$("#alert-error").children(".details").html(
					"<strong>" + xhr.status + ": " + xhr.statusText + "</strong>");
				if (err.message && err.message != xhr.statusText)
					$("#alert-error").children(".message").html(err.message);
				
				$("#alert-error").show();
				$("#response").val(JSON.stringify(err));
			}
		});
	};

	$(sendBtn).on("click", function() {

		var auth = $("enable-auth").is(":checked"),
			servoUrl = $("#servo-url").val(),
			clientId = $("#client-id").val(),
			authKey = $("#auth-key").val(),
			itemKey = $("#item-key").val(),
			dataType = $("#data-type").val(),
			method = $("input#method:checked").val(),
			data = body.val(),
			error = false;

		reset();
		if (auth && clientId.length == 0) {
			error = true;
			$("#client-id").parent().parent('.form-group').addClass("has-error");
		}

		if (auth && authKey.length == 0) {
			error = true;
			$("#client-id").parent().parent('.form-group').addClass("has-error");
		}

		if (itemKey.length == 0) {
			error = true;
			$("#item-key").parent().parent('.form-group').addClass("has-error");
		}

		if (method == "GET" || method == "DELETE")
			data = "";

		console.log("clientId=" + clientId);
		console.log("itemKey=" + itemKey);
		console.log("dataType=" + dataType);
		console.log("method=" + method);
		console.log("data length=" + data.length);

		if (error) {
			$("#alert-warn").show();
			return;
		}

		s.setUrl(servoUrl);
		return send(s, clientId, method, itemKey, dataType, data);
	});

	$(resetBtn).on("click", function() {
		reset();
		resetBtn.val("");
	});

	$(methodSel).on("change", function() {
		var checked = $("input#method:checked").val();
		if (checked == "POST" || checked == "PUT") {
			body.prop('readonly', false);
		}
		else {
			body.prop('readonly', true);
		}
	})
});