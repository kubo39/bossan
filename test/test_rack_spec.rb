require_relative './bossan_test'
require 'minitest/autorun'
require 'uri'

class RackSpecTest < Bossan::Test::TestCase
  ASSERT_RESPONSE = "Hello world!"
  RESPONSE = ["Hello ", "world!"].freeze

  class App
    def call env
      body = RESPONSE
      [200,
       {
         'Content-type'=> 'text/plain',
         'Content-length'=> RESPONSE.join.size.to_s
       },
       body
      ]
    end
  end

  def app
    App.new
  end

  def test_get
    response = Net::HTTP.start(host(), port()) {|http|
      http.get("/")
    }
    assert_equal("200", response.code)
    assert_equal(ASSERT_RESPONSE, response.body)
  end

  def test_post
    response = Net::HTTP.post_form(URI.parse("http://#{host()}:#{port()}/"),
                                   {'key1'=> 'value1', 'key2'=> 'value2'})
    assert_equal("200", response.code)
    assert_equal(ASSERT_RESPONSE, response.body)
  end
end

