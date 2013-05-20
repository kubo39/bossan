require_relative '../lib/bossan'

Bossan.run('127.0.0.1', 8000, proc {|env|
  body = 'hello, world!'
  [
   200,          # Status code
   {             # Response headers
     'Content-Type' => 'text/html',
     'Content-Length' => body.size.to_s,
   },
   [body]        # Response body
  ]
})
