require 'test/unit'
require "uri"
require 'net/http'
require_relative '../lib/bossan'


class RackEnvBigMessageBodyTest < Test::Unit::TestCase

  RESPONSE = ["Hello ", "world!"].freeze
  DEFAULT_HOST = "localhost"
  DEFAULT_PORT = 8000

  def test_query_app
    pid = fork do
      trap(:INT) { Bossan.stop }
      Bossan.listen(DEFAULT_HOST, DEFAULT_PORT)
      Bossan.run(proc {|env|
                   @env = env.dup
                   assert_equal(IO, @env["rack.input"].class)
                   body = RESPONSE
                   [200,
                    {
                      'Content-type'=> 'text/plain',
                      'Content-length'=> RESPONSE.join.size.to_s
                    },
                    body
                   ]
                 })
    end
    Process.detach pid
    sleep 2

    uri = URI.parse("http://0.0.0.0:8000/")
    Net::HTTP.start(uri.host, uri.port){|http|
      header = {
        "user-agent" => "Ruby/#{RUBY_VERSION} MyHttpClient"
      }
      body = "A" * 1024 * 513 # 513K
      response = http.post(uri.path, body, header)
    }
  ensure
    Process.kill(:INT, pid)
  end

end
