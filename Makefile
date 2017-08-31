# Servo Makefile

KORE=$(shell which kodev)
SERVO=servo.so
S_SRC=	$(wildcard src/*.c)

$(SERVO): $(S_SRC)
	$(KORE) build

build: $(SERVO)

install:
	./tools/install

configure:
	./tools/configure

clean:
	$(KORE) clean

default: build

.PHONY: install configure clean
