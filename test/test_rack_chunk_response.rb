require_relative './bossan_test'
require 'minitest/autorun'

class RackChunkResponseTest < Bossan::Test::TestCase
  class App
    def call env
      body = ["Hello ", "World!"]
      [200,
       {'Content-type'=> 'text/plain'},
       body
      ]
    end
  end

  def app
    App.new
  end

  def test_chunk_response
    response = Net::HTTP.start(host(), port()) {|http|
      http.get("/")
    }

    headers = response.header
    assert_equal("200", response.code)
    assert_equal("Hello World!", response.body)
    assert_equal("chunked", headers["transfer-encoding"])
    assert_equal("close", headers["connection"])
  end
end
