FROM node:latest

WORKDIR /src/servo/js

COPY package.json /src/servo/js

RUN rm -fr ./node_modules
RUN npm install
RUN ls -la ./node_modules
