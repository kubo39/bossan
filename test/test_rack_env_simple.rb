# -*- coding: utf-8 -*-
require 'bossan'
require 'test/unit'
require 'pp'
require 'net/http'


class RackEnvSimpleTest < Test::Unit::TestCase

  RESPONSE = ["Hello ", "world!"].freeze
  DEFAULT_HOST = "localhost"
  DEFAULT_PORT = 8000

  def test_simple_app
    r, w = IO.pipe
    pid = fork do
      r.close
      trap(:INT) { Bossan.stop }
      Bossan.run(DEFAULT_HOST, DEFAULT_PORT,
                 proc {|env|
                   @env = env.dup
                   # I have no idea how to check this two values..
                   @env.delete "rack.input"
                   @env.delete "rack.errors"
                   # pp @env
                   w.write @env
                   w.close
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

    Net::HTTP.start(DEFAULT_HOST, DEFAULT_PORT){|http|
      http.get("/")
    }

    w.close
    env = r.read
    r.close

    env = eval "Hash[" + env.gsub("\"", "'") + "]"
    # pp env

    assert_equal(env["PATH_INFO"], "/")
    assert_equal(env["SCRIPT_NAME"], "")
    assert_equal(env["QUERY_STRING"], "")
    assert_equal(env["REQUEST_METHOD"], "GET")
  ensure
    Process.kill(:INT, pid)
  end

end
