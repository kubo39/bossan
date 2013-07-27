require_relative '../lib/rack/handler/bossan'
require_relative './util'
require 'minitest/unit'
require 'test/unit/testcase'
require 'uri'


DEFAULT_HOST = "localhost"
DEFAULT_PORT = "8000"
ASSERT_RESPONSE = "Hello world!"
RESPONSE = ["Hello ", "world!"].freeze


class App
  def call env
    # p env
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


class RackSpecTest < Test::Unit::TestCase

  def test_get
    response = Net::HTTP.start(DEFAULT_HOST, DEFAULT_PORT) {|http|
      http.get("/")
    }
    assert_equal("200", response.code)
    assert_equal(ASSERT_RESPONSE, response.body)
  end

  def test_post
    response = Net::HTTP.post_form(URI.parse("http://#{DEFAULT_HOST}:#{DEFAULT_PORT}/"),
                                   {'key1'=> 'value1', 'key2'=> 'value2'})
    assert_equal("200", response.code)
    assert_equal(ASSERT_RESPONSE, response.body)
  end
end


begin
  $stderr.puts RUBY_DESCRIPTION
  pid = fork do
    trap(:INT) { Rack::Handler::Bossan.stop }
    Rack::Handler::Bossan.run(App.new, {
                                :Host => DEFAULT_HOST, :Port => DEFAULT_PORT
                              })
  end

  Process.detach pid

  unless server_is_wake_up?
    $stderr.puts "bossan won't wake up until you love it ..."
    exit 1
  end

  err = MiniTest::Unit.new.run(ARGV)
  exit false if err && err != 0

ensure
  Process.kill(:INT, pid)
end
