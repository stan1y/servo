# Servo standalone image

FROM python:3.6

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get install -y libpq-dev

COPY . /src/servo
VOLUME /src/servo
WORKDIR /src/servo

RUN pip install -r requirements.txt
RUN python setup.py develop
RUN mkdir -p /etc/servo
RUN cp ./config /etc/servo/conf
