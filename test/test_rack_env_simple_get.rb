# -*- coding: utf-8 -*-
require 'test/unit'
require 'pp'
require 'net/http'
require_relative '../lib/bossan'


class RackEnvSimpleGetTest < Test::Unit::TestCase

  RESPONSE = ["Hello ", "world!"].freeze
  DEFAULT_HOST = "localhost"
  DEFAULT_PORT = 8000

  def test_simple_app
    r, w = IO.pipe
    pid = fork do
      r.close
      trap(:INT) { Bossan.stop }
      Bossan.listen(DEFAULT_HOST, DEFAULT_PORT)
      Bossan.run(proc {|env|
                   @env = env.dup
                   # I have no idea how to check this two values..
                   @env.delete "rack.input"
                   @env.delete "rack.errors"
                   w.write Marshal.dump(@env)
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
    env = Marshal.load(r.read)
    r.close

    assert_equal("/", env["PATH_INFO"])
    assert_equal("", env["SCRIPT_NAME"])
    assert_equal("", env["QUERY_STRING"])
    assert_equal("GET", env["REQUEST_METHOD"])
    assert_equal("localhost", env["SERVER_NAME"])
    assert_equal("8000", env["SERVER_PORT"])
    assert_not_equal("", env["HTTP_USER_AGENT"])
  ensure
    Process.kill(:INT, pid)
  end

end
