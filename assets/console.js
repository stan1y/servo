$(document).ready(function() {
	var sendBtn = $("#send")[0],
		resetBtn = $("#reset")[0],
		methodSel = $("#method")[0],
		body = $("#body")[0];

	var reset = function() {
		$("#alert-warn").hide();
		$("#alert-error").hide();
		$("div.form-group").removeClass("has-error");
	};

	var onServoError = function(xhr, type, ex) {
		console.log("Error received: " + xhr.responseText);
		$("#alert-error").children(".details").html(xhr.responseText);
		$("#alert-error").show();
	};

	var onServoSuccess = function(data) {
		if (data && data.length > 0)
			$("#body").val(data);
	};

	var send = function(clientId, method, itemKey, contentType, data) {
		var servoUrl = "https://" + window.location.host
		             + "/" + itemKey;
		console.log("servoUrl=" + servoUrl);
		$.ajax(servoUrl, {
			method: method,
			data: data,
			contentType: contentType,
			error: onServoError,
			success:onServoSuccess
		});
	};

	$(sendBtn).on("click", function() {

		var clientId = $("#client-id").val(),
			itemKey = $("#item-key").val(),
			contentType = $("#content-type").val(),
			method = $("#method").val(),
			body = $("body").val(),
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
		console.log("contentType=" + contentType);
		console.log("method=" + method);
		console.log("bodylen=" + body.length);

		if (error) {
			$("#alert-warn").show();
			return;
		}

		return send(clientId, method, itemKey, contentType, body);
	});

	$(resetBtn).on("click", function() {
		reset();
		resetBtn.val("");
	});

	$(methodSel).on("change", function() {
		var val = $("#method option:selected").text()
		if (val == "POST" || val == "PUT") {
			$(body).prop('readonly', false);
		}
		else {
			$(body).prop('readonly', true);
		}
	})
});