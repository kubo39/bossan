require_relative '../lib/bossan'
require_relative './util'
require 'minitest/unit'
require 'test/unit/testcase'
require 'uri'


DEFAULT_HOST = "localhost"
DEFAULT_PORT = 8000
ASSERT_RESPONSE = "Hello world!"
RESPONSE = ["Hello ", "world!"].freeze


class App
  def call env
    body = RESPONSE
    [200,
     {'Content-type'=> 'text/plain'},
     body
    ]
  end
end

class RackChunkResponseTest < Test::Unit::TestCase

  def test_chunk_response
    response = Net::HTTP.start(DEFAULT_HOST, DEFAULT_PORT) {|http|
      http.get("/")
    }

    headers = response.header
    assert_equal("200", response.code)
    assert_equal(ASSERT_RESPONSE, response.body)
    assert_equal("chunked", headers["transfer-encoding"])
    assert_equal("close", headers["connection"])
  end

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
