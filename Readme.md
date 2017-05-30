# Servo

## Servo is a minimalist backend session engine and storage.

[Servo](http://www.endlessinsomnia.com/projects/servo) is a backend session storage engine. It allows you easily store structured and unstructured data in a key/value remote on-demand storage with enforced expiration. Servo is minimalistic so there is no authentication (by default) however there is an isolation between sessions. Servo is no configuration, RESTfull, a full CRUD scrap storage for web application and (mainly) javascript in generated static web sites.

### Servo features

- No client configuration, just AJAX/REST requests on a fixed path
- Auto-expiration of stored items in an isolated anonymous sessions
- Understands and speaks in `text/plain`, `application/base64`, `application/json` and `multipart/form-data`
- Json Web Tokens RFC 7519 client-side state

Think of Servo as a shopping cart persistent across devices or persons;
Or as poll storage for your static blog post; 
Or as temporary storage to upload user's picture to manipulate it on the client side (javascript).

There is a javascript client library for easy use, however plan REST API lets

### Usage

Clients use Servo API to establish a session and store data in it. The service is not designed to be publicly visible to external clients and it is advised to use request throttling in a dedicated proxy service. For example [nginx's ngx_http_limit_req_module](http://nginx.org/en/docs/http/ngx_http_limit_req_module.html) is a very good choice for this job.


### Dependencies 

* [Kore Framework](https://kore.io)
* [libjansson](http://www.digip.org/jansson/)
* [libjwt](https://github.com/benmcollins/libjwt)


#### Install 

* __CentOS 7:__ `sudo yum install postgresql-sever postgresql-devel libuuid-devel`
* __Mac OS X:__ `brew install postgresql ossp-uuid`


Servo runs on [Kore framework](https://kore.io/), so you need to install it first.
Select build flavor corresponding to your platform, see available flavors with `kore flavor`

     $ kore flavor linux-dev

And finally build Servo.

     $ kore build

This command will build a `servo.so` module which is an application for Kore. Next you can install Servo to the system or run it locally in debug mode. 

Execute

     $ kodev run

To run locally, or run

     $ sudo tools/install

To install globally to `PREFIX` specified at build.


### Configuration

To configure a fresh installation of Servo run the following tools:

     $ sudo tools/configure

This will drop and create a new fresh database.


### Javascript Client Library

The use of Servo app from Javascript environemnt is pretty straight-forward with any modern framework or library such as JQuery. However it requires certain repetition of common features on client side of Servo prototcol. In order to provide an example of Servo API usage and speed up addoptation in static web sites, there is a Grunt-based Javascript Library in `js` folder.


### Console App

There is a separate Node.JS based console application witch illustrates usage of Servo JS client library and CORS features of Servo. The app located in `js/app` folder and uses Grunt for build and distribution together with JS library itself.


### Query Data

To ask Servo for saved item, clients need to perform `GET` request to a one of following paths. Session index request can be used to verify session availability in case caller in black-listed or blocked otherwise, but in general caller need to be prepared to handle error status code from `GET` interface.

- `GET /` - Session index. Returns statistics or debug console in [public mode](#Public Mode).
- `GET /foo` - Get item data for specified key `/foo`.

Item data is formatted as specified by `Accept` header in the request. If no item found with such key, a 404 error is returned. So client may upload binary files as `multipart/form-data` and get it back as `application/base64` for later use in data urls.

### JSON Data Type Query

TBD

### Store Data

To store data in Servo, clients need to perform either `POST` or `PUT` requests with item {key} in request path.

- `POST /foo` - Create a new item with key `/foo`. If there is an item with key `/foo` error 409 Conflict is returned.
- `PUT  /foo` - Alter existing item with key `/foo`. If no such item returns error 404 Not Found is retured.

Internally Servo understands data as 3 possible types: JSON, TEXT and BLOB and inspects `Content-Type` header to pick a data parser for request data. Broken JSON or Base64 will lead to error 400.
The following values are recognized by Servo:

- `application/json` Servo reads data from request `body` and stores it as JSON type. 
  For JSON items Servo support additional [GET query parameters](#JSON Data Type Query).
- `text/plain` Servo reads data from request `body` and stores it as TEXT type. 
- `application/base64` Servo read data from request `body` as Base64 encoded binary and stores it as BLOB type.
- `multipart/form-data` Servo read multi-part binary data from client and stores it as BLOB type.

Requests may return with error status 403 if sent data was not well formed or too long. 

### Data Removal

Servo automatically expires session and purges all data associated with a session during removal. At the same time clients
may want to remove saved data for cleanup/reset purposes.

- `DELETE /foo` - Create a new item with key `/foo`. 

## Public Mode

Servo is serving a debug console to query data using browser itself. This is an example for client lib usage as well.

## Releases

There are no releases yet. Servo is at the concept and PoC stage now.
Data storage engine, rules and client API contracts are prototypes. 

## License
GNU General Public v 3.0
