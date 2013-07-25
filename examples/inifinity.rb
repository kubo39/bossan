require_relative '../lib/bossan'


app = lambda do |env|
  body = (1..Float::INFINITY).lazy.map(&:to_s)
  [
   200,          # Status code
   {             # Response headers
     'Content-Type' => 'text/html',
   },
   body        # Response body
  ]
end

Bossan.set_keepalive(10)
Bossan.listen('127.0.0.1', 8000)
Bossan.run(app)
