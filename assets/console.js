$(document).ready(function() {
	var sendBtn = $("#send"),
		resetBtn = $("#reset"),
		methodSel = $("input#method"),
		body = $("textarea#body");

	var reset = function() {
		$("#alert-warn").hide();
		$("#alert-error").hide();
		$("#alert-info").hide();
		$("div.form-group").removeClass("has-error");
	};

	var onServoError = function(xhr, type, ex) {
		console.log("Error received: " + xhr.responseText);
		$("#alert-error").children(".details").html(xhr.responseText);
		$("#alert-error").show();
	};

	var onServoSuccess = function(data, status, resp) {
		console.log("Request completed, received " + data.length);
		if (data && data.length > 0)
			body.val(data);
		if (data.length)
			$("#alert-info").children(".details").html("Received " + data.length + " chars.");
		$("#alert-info p#info-resp").replaceWith(resp.status + ": " + resp.statusText);
		$("#alert-info").show();
	};

	var send = function(clientId, method, itemKey, dataType, data) {
		var servoUrl = "https://" + window.location.host
		             + "/" + itemKey;
		console.log("servoUrl=" + servoUrl);
		$.ajax(servoUrl, {
			method: method,
			data: data,
			dataType: dataType,
			accepts: { 
				text: 'text/plain; charset=UTF-8',
				json: 'application/json; charset=UTF-8',
				binary: 'multipart/form-data; charset=UTF-8' 
			},
			error: onServoError,
			success: onServoSuccess
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