# Bossan

[![Build Status](https://secure.travis-ci.org/kubo39/bossan.png?branch=master)](http://travis-ci.org/kubo39/bossan)

Bossan is a high performance asynchronous ruby's rack-compliant web server.

## Requirements

Bossan requires Ruby 1.9.2 or later.

Bossan supports Linux, FreeBSD and Mac OS X.

## Installation

`gem install bossan`

Or from source:

```
git clone git://github.com/kubo39/bossan.git
cd bossan
rake build
rake install
```

## Usage

simple rack app:

``` ruby
require 'bossan'

Bossan.run('127.0.0.1', 8000, proc {|env|
  [
   200,          # Status code
   {             # Response headers
     'Content-Type' => 'text/html',
     'Content-Length' => '13',
   },
   ['hello, world!']        # Response body
  ]
})
```

## Contributing

1. Fork it
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request
