# Servo Makefile

KORE_SRC=lib/kore
KORE=$(shell which kodev)
SERVO=servo.so
S_SRC=	$(wildcard src/*.c)

$(SERVO): $(S_SRC)
	$(KORE) build

all: $(SERVO)

install:
	./tools/install

configure:
	./tools/configure

clean:
	$(KORE) clean

.PHONY: all clean
