require 'parallel'
require 'bossan'

results = Parallel.map([8000, 8001, 8002], :in_processes=>3){|port|
  Bossan.run('127.0.0.1', port, proc{|env|
               body = "Hello, World!"
               [200,
                { 'Content-Type' => 'text/plain',
                  'Content-Length' => body.size.to_s
                },
                [body]
               ]
             })
}
