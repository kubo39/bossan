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
