FROM node:latest

WORKDIR /src/servo/js
RUN npm install -g grunt

COPY package.json /src/servo/js

RUN rm -fr ./node_modules
RUN npm install
RUN ls -la ./node_modules