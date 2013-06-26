require_relative '../lib/bossan'

Bossan.listen('127.0.0.1', 8000)
Bossn.run(proc {|env|
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
