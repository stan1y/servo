


servo-js:
	cd src/js && grunt

tests: servo-js
	docker-compose up -d
	docker exec -it servo_console_1 grunt nodeunit
	docker-componse down