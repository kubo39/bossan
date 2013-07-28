# CHANGELOG

### v0.4.4

(Bug release. release 2013-XX-XX)

* return BadRequest when invalid encoded url

* fix rack::handler

### v0.4.3

(Bug release. release 2013-07-27)

* fix keepalive

### v0.4.2

(Bug release. release 2013-07-25)

* using tmpfile for very large message body(over 512K)

* fix infinte accept loop while high load

### v0.4.1

(Bug release. release 2013-07-20)

* fix Build failed in OSX.

### v0.4.0

(New Feature release. release 2013-07-19)

* Support RFC2616(HTTP/1.1)

* fix reason phrase

### v0.3.0

(New Feature release. release 2013-06-28)

* Add Bossan.listen method, for supporting pre-fork server

* change API Bossan.run

### v0.2.0

(New Feature release. release 2013-05-22)

* support keep-alive (use Bossan.set_keepalive)

* use TCP_DEFER_ACCEPT

* add set_backlog (default:1024)

* add set_picoev_max_fd (default:4096)