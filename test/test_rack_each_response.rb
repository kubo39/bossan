require_relative './bossan_test'
require 'minitest/autorun'
require 'uri'

class RackEachResponseTest < Bossan::Test::TestCase
  class EachResponse
    def each
      yield "Hello"
      yield " "
      yield "World!"
    end
  end

  class App
    def call env
      body = EachResponse.new
      [200,
       {
         'Content-type'=> 'text/plain',
         'Content-length'=> "Hello World".size.to_s
       },
       body
      ]
    end
  end

  def app
    App.new
  end

  def test_each_response
    response = Net::HTTP.start(host(), port()) {|http|
      http.get("/")
    }
    assert_equal("200", response.code)
    assert_equal("Hello World", response.body)
  end
end
