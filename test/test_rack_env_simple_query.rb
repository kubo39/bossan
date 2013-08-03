# -*- coding: utf-8 -*-
require_relative './bossan_test'
require 'net/http'
require 'json'
require 'minitest/autorun'

class RackEnvSimpleQueryTest < Bossan::Test::TestCase
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

  def test_query_app
    response = Net::HTTP.start(host(), port()){|http|
      http.get("/ABC/DEF?a=1234&bbbb=ccc")
    }

    env = JSON.parse(response.body)

    assert_equal("/ABC/DEF", env["PATH_INFO"])
    assert_equal("a=1234&bbbb=ccc", env["QUERY_STRING"])
  end
end
