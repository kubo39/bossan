require 'bossan'

Bossan::Server.listen('127.0.0.1', 8000)
Bossan::Server.run proc {|env|
  [
   200,          # Status code
   {             # Response headers
     'Content-Type' => 'text/html',
     'Content-Length' => '2',
   },
   ['hi']        # Response body
  ]
}
