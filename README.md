# Bossan

Bossan is a high performance asynchronous rack web server.

## Requirements

Bossan requires Ruby 1.9.x.

Bossan supports Linux only.

## Installation

Add this line to your application's Gemfile:

    gem 'bossan'

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install bossan

## Usage

simple rack app:

``` ruby
require 'bossan'

Bossan.run('127.0.0.1', 8000, proc {|env|
  [
   200,          # Status code
   {             # Response headers
     'Content-Type' => 'text/html',
     'Content-Length' => '2',
   },
   ['hi']        # Response body
  ]
})
```

## Contributing

1. Fork it
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request
