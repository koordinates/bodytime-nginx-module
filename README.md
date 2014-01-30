Nginx BodyTime
===============

Nginx has a variable called [`$request_time`](http://nginx.org/en/docs/http/ngx_http_log_module.html), which is the *time elapsed between the first bytes were read from the client and the log write after the last bytes were sent to the client*, and another called [`$upstream_response_time`](http://nginx.org/en/docs/http/ngx_http_upstream_module.html#variables), which *keeps times of responses obtained from upstream servers*.

But if you're trying to serve responses where sizes are magnitudes apart (eg. varying between 3KB-300MB) and compare server-side processing time before the content starts streaming from a backend, what you (possibly) want to look at is the time until the client gets sent the first byte of the response body. That's what this BodyTime module does.

This module is not distributed with the Nginx source. See the [installation instructions](#installation).


Synopsis
========

```nginx
http {
    bodytime on;

    log_format  main  '$remote_addr - $remote_user [$time_local] "$request" '
                      '$status $body_bytes_sent "$http_referer" '
                      '"$http_user_agent" "$http_x_forwarded_for" '
                      'req=$request_time body=$body_start';
    access_log  logs/access.log  main;

    ...

}
```

And you should see something like this appear in your access log:

```
127.0.0.1 - - [30/Jan/2014:12:13:28 +1300] "GET /data/ HTTP/1.1" 200 100000000 "-" "curl/7.30.0" "-" req=1.000 body=0.100
```


Installation
============

Grab the nginx source code from [nginx.org](http://nginx.org/), for example, and then build the source with this module:

```bash
wget 'http://nginx.org/download/nginx-1.5.8.tar.gz'
tar -xzvf nginx-1.5.8.tar.gz
cd nginx-1.5.8/

./configure --add-module=/path/to/bodytime-nginx-module
```


Directives
==========

**syntax:** *bodytime on|off*

**default:** *off*

**context:** *http, server, location*

**phase:** *output filter*

Enables or disables setting of the `$body_start` variable for each request.

Enabling `bodytime` for a request's location will force the underlying Nginx timer to update to the current system time, regardless of the timer resolution settings elsewhere in the configuration. This has a performance penalty, so you may not want to enable it for entire servers.


Variables
=========

### `$body_start`

This variable holds the elapsed time in seconds between the start of the current request and the first chain of the body response being sent.

Like `$request_time`, the value has millisecond resolution with three digits after the decimal point. (eg. `1.010`)

The BodyTime filter is executed *after* any gzipping starts, since zlib typically buffers the first 90KB or so of a response when compressing before starting to output data, so the `$body_start` value will reflect that. Look at Nginx's `auto/modules` file and the `config` file to learn more about or change the filter ordering.


TODO
====

* Look and test whether the filter should be inserted before/after the spdy/chunked filters to provide a more accurate measurement.
* Investigate chunked transfer encoding & gzip causing bogus `$request_time` values (see ["$request_time is 0.000 with gzip/chunked?"](http://forum.nginx.org/read.php?29,246968)).


Copyright & License
===================

Copyright (c) 2014, Robert Coup, Koordinates Limited.

This module is licensed under the terms of the BSD license - see LICENSE.
