require_relative './bossan_test'
require 'minitest/autorun'

ASSERT_RESPONSE = "Hello world!"
RESPONSE = ["Hello ", "world!"].freeze

class RackErrSpecTest < Bossan::Test::TestCase
  class ErrApp
    def call env
      body = RESPONSE
      [500,
       {
         'Content-type'=> 'text/html',
       },
       RESPONSE
      ]
    end
  end

  def app
    ErrApp.new
  end

  def test_error
    response = Net::HTTP.start(host(), port()) {|http|
      http.get("/")
    }
    assert_equal("500", response.code)
    assert_equal(ASSERT_RESPONSE, response.body)
  end
end
