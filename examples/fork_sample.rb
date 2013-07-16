require_relative '../lib/bossan'
require 'parallel'

app = ->(env){
 body = ['hello, world!']        # Response body
 [200,          # Status code
  {             # Response headers
    'Content-Type' => 'text/html',
    'Content-Length' => body.join.size.to_s,
  },
  body]
}

Bossan.set_keepalive(10)
Bossan.listen('localhost', 8000)
Parallel.each([0,1,2,3]) {
  Bossan.run(app)
}
