require_relative '../lib/bossan'
require 'minitest/unit'
require 'test/unit/testcase'
require 'net/http'


ASSERT_RESPONSE = "Hello world!"
RESPONSE = ["Hello ", "world!"].freeze
DEFAULT_HOST = "localhost"
DEFAULT_PORT = 8000


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


class LongUrlTest < Test::Unit::TestCase

  def test_long_url1
    response = Net::HTTP.start(DEFAULT_HOST, DEFAULT_PORT) {|http|
      query = "A" * 4095
      http.get("/#{query}")
    }

    assert_equal("200", response.code)
  end

  def test_long_url2
    response = Net::HTTP.start(DEFAULT_HOST, DEFAULT_PORT) {|http|
      query = "A" * 4096
      http.get("/#{query}")
    }

    assert_equal("400", response.code)
  end
end


def server_is_wake_up? n=100
  n.times {
    begin
      Net::HTTP.start(DEFAULT_HOST, DEFAULT_PORT)
    rescue
      next
    end
    $stderr.puts "*** running success ***"
    return true
  }
  return false
end


begin
  $stderr.puts RUBY_DESCRIPTION

  pid = fork do
    trap(:INT) { Bossan.stop }
    Bossan.listen(DEFAULT_HOST, DEFAULT_PORT)
    Bossan.run(App.new)
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
