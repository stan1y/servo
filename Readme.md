# Servo

## Servo is a minimalist backend session engine and storage.

Servo at it's core is basically on demand session with ability to store 
arbitrary data in [Redis](http://redis.io/).

## Usage

Clients use Servo [API](http://www.endlessinsomnia.com/projects/servo) to establish
a session and store data in it. Servo can store any data supported by Redis and 
allows to query it with much of Redis commands API.

## Sessions
The session can be created on demand during first call of the client or created of
new session can be limited by a specific list of clients (by their ip addresses).
The lifetime of a session is limited by per client and global property.

## License
GNU General Public v 3.0