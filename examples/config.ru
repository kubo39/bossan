require 'bossan'
require 'pp'

class MyApp
  def call env
    body = ['hi!']
    # pp env
    [
     200, # Status code
     { 'Content-Type' => 'text/html',
       'Content-Length' => body.join.size.to_s,
     }, # Reponse headers
     body # Body of the response
    ]
  end
end

# Rack::Handler::Bossan.run MyApp.new
run MyApp.new
