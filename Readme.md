# Servo

## Servo is a minimalist backend session engine and storage.

[Servo](http://www.endlessinsomnia.com/projects/servo) is a backend session storage engine. It allows you easily store structured and unstructured data in a key/value remote on-demand storage with enforced expiration. Servo is minimalistic so there is no authentication however there is an isolation between sessions. Servo is no configuration, RESTfull, a full CRUD scrap storage for web application and (mainly) javascript in generated static web sites.

### Servo features

- No client configuration, just AJAX/REST requests on a fixed path
- Auto-expiration of stored items in an isolated anonymous sessions
- Understands and speaks in `text/plain`, `application/base64`, `application/json` and `multipart/form-data`

Think of Servo as a shopping cart persistent across devices or persons;
Or as poll storage for your static blog post; 
Or as temporary storage to upload user's picture to manipulate it on the client side (javascript).

## Usage

Clients use Servo API to establish a session and store data in it. The service is not designed to be publicly visible to external clients and it is advised to use request throttling in a dedicated proxy service. For example [nginx's ngx_http_limit_req_module](http://nginx.org/en/docs/http/ngx_http_limit_req_module.html) is a very good choice for this job.

### Query Data

To ask Servo for saved item, clients need to perform `GET` request to a one of following paths. Session index request can be used to verify session availability in case caller in black-listed or blocked otherwise, but in general caller need to be prepared to handle error status code from `GET` interface.

- `GET /session/` Session index. Returns statistics or debug console in [public mode](#Public Mode).
- `GET /session/{key}` - Get item data for specified key.

Item data is formatted as specified by `Accept` header in the request. If no item found with such key, a 404 error is returned. So client may upload binary files as `multipart/form-data` and get it back as `application/base64` for later use in data urls.

### JSON Data Type Query

TBD

### Store Data

To store data in Servo, clients need to perform either `POST` or `PUT` requests with item {key} in request path.

- `POST /session/{key}` - Create a new item with specified key. 
- `PUT  /session/{key}` - Alter existing item with specified key, if no such item returns error status 404.

Internally Servo understands data as 3 possible types: JSON, TEXT and BLOB and inspects `Content-Type` header to pick a data parser
for request data. 
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

- `DELETE /session/{key}` - Create a new item with specified key. 

## Public Mode

TBD

## License
GNU General Public v 3.0
