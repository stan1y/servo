# Servo standalone image

FROM kore:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get install -y \
	cmake \
	automake \
	autoconf \
	libtool \
	pkg-config \
	uuid-dev

RUN mkdir -p /usr/local/src/servo
COPY .  /src/servo
WORKDIR /src/servo

RUN cd jansson && cmake . && make && make install
RUN cd libjwt && autoreconf -i && ./configure && make && make install

RUN echo /usr/local/lib > /etc/ld.so.conf.d/usr-local-lib.conf
RUN ldconfig


RUN kodev flavor prod
RUN kodev build

RUN mkdir -p /usr/local/servo/conf
COPY ./config.default /usr/local/servo/conf/servo.conf
