# -*- coding: utf-8 -*-
require_relative './bossan_test'
require 'net/http'
require 'json'
require 'minitest/autorun'

class RackEnvSimpleGetTest < Bossan::Test::TestCase
  def app
    proc {|env|
      @env = env.dup
      # I have no idea how to check this two values..
      @env.delete "rack.input"
      @env.delete "rack.errors"
      body = [JSON.dump(@env)]
      [200,
       {
         'Content-type'=> 'application/json',
         'Content-length'=> body.join.size.to_s
       },
       body
      ]
    }
  end

  def test_simple_app
    response = Net::HTTP.start(host(), port()){|http|
      http.get("/")
    }

    env = JSON.parse(response.body)

    assert_equal("/", env["PATH_INFO"])
    assert_equal("", env["SCRIPT_NAME"])
    assert_equal("", env["QUERY_STRING"])
    assert_equal("GET", env["REQUEST_METHOD"])
    assert_equal("localhost", env["SERVER_NAME"])
    assert_equal("8000", env["SERVER_PORT"])
    assert_not_equal("", env["HTTP_USER_AGENT"])
  end
end
