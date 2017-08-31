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

COPY jansson /src/jansson
WORKDIR /src/jansson
RUN cmake . && make && make install

COPY libjwt /src/libjwt
WORKDIR /src/libjwt
RUN autoreconf -i && ./configure && make && make install

RUN echo /usr/local/lib > /etc/ld.so.conf.d/usr-local-lib.conf
RUN ldconfig

RUN mkdir -p /usr/local/servo/conf
COPY ./config.docker /usr/local/servo/conf/servo.conf
