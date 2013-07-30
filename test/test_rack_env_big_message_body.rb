require_relative './bossan_test'
require "uri"
require 'minitest/autorun'

class RackEnvBigMessageBodyTest < Bossan::Test::TestCase
  def app
    proc {|env|
      @env = env.dup
      body = @env["rack.input"].class.to_s
      [200,
       {
         'Content-type'=> 'text/plain',
         'Content-length'=> body.size.to_s
       },
       [ body ]
      ]
    }
  end

  def test_query_app
    uri = URI.parse("http://#{host()}:#{port()}/")
    Net::HTTP.start(uri.host, uri.port){|http|
      header = {
        "user-agent" => "Ruby/#{RUBY_VERSION} MyHttpClient"
      }
      body = "A" * 1024 * 513 # 513K
      response = http.post(uri.path, body, header)
      assert_equal("IO", response.body)
    }
  end
end
