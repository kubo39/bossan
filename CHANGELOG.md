# CHANGELOG

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