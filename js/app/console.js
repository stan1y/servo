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
		body = $("textarea#body");

	var reset = function() {
		$("#alert-warn").hide();
		$("#alert-error").hide();
		$("#alert-info").hide();
		$("#alert-error").children(".details").html("");
		$("#alert-info").children(".details").html("");
		$("div.form-group").removeClass("has-error");
	};

	var onServoError = function(xhr, type, ex) {
		console.log("Error received: " + xhr.responseText);
		var err = JSON.parse(xhr.responseText);

		$("#alert-error").children(".details").html(
			"<strong>" + xhr.status + ": " + xhr.statusText + "</strong>");
		if (err["message"] != xhr.statusText)
			$("#alert-error").children(".message").html(err["message"]);
		$("#alert-error").show();
		$("#response").val(xhr.responseText);
	};

	var onServoSuccess = function(data, status, resp) {
		var isStatusResponse = false;
		if (typeof data == "object") {
			if ("code" in data && "message" in data) {
				// this is standard API status response
				isApiResponse = true;
				$("#alert-info").children(".details").html(
					data["code"] + ": " + data["message"]);
			}
			data =  JSON.stringify(data)
		}

		if (!isStatusResponse && data && data.length) {
			console.log("received bytes length=" + data.length);
			$("#response").val(data);
		}
		$("#alert-info").children(".details").html(
			"<strong>" + resp.status + ": " + resp.statusText + "</strong>");
		$("#alert-info").show();
	};

	var send = function(clientId, method, itemKey, dataType, data) {
		var servoUrl = "https://" + window.location.host
		             + "/" + itemKey,
		    dataTypesMap = { 
				text: 'text/plain; charset=UTF-8',
				json: 'application/json; charset=UTF-8',
				binary: 'multipart/form-data; charset=UTF-8' 
			};

		console.log("servoUrl=" + servoUrl);
		$.ajax(servoUrl, {
			method: method,
			data: data,
			dataType: dataType,
			accepts: dataTypesMap,
			contentType: dataTypesMap[dataType],
			error: onServoError,
			success: onServoSuccess,
			processData: false
		});
	};

	$(sendBtn).on("click", function() {

		var clientId = $("#client-id").val(),
			itemKey = $("#item-key").val(),
			dataType = $("#data-type").val(),
			method = $("input#method:checked").val(),
			data = body.val(),
			error = false;

		reset();
		if (clientId.length == 0) {
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

		return send(clientId, method, itemKey, dataType, data);
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