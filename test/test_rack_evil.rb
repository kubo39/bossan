require_relative '../lib/bossan'
require_relative './util'
require 'minitest/unit'
require 'test/unit/testcase'
require 'socket'


ASSERT_RESPONSE = "Hello world!"
RESPONSE = ["Hello ", "world!"].freeze
DEFAULT_HOST = "localhost"
DEFAULT_PORT = 8000
DEFAULT_PATH = "/PATH?ket=value"
DEFAULT_VERSION = "HTTP/1.0"
DEFAULT_HEADER = {
  "User-Agent"=> "Mozilla/5.0 (X11; U; Linux i686; ja; rv:1.9.2.7) Gecko/20100715 Ubuntu/10.04 (lucid) Firefox/3.6.7",
  "Accept"=> "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
  "Accept-Language"=> "ja,en-us;q=0.7,en;q=0.3",
  "Accept-Encoding"=> "gzip,deflate",
  "Accept-Charset"=> "Shift_JIS,utf-8;q=0.7,*;q=0.7",
  "Keep-Alive"=> "115",
  "Connection"=> "keep-alive",
  "Cache-Control"=> "max-age=0",
}

ERR_400 = "HTTP/1.0 400 Bad Request"

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


def send_data method
  begin
    sock = TCPSocket.new DEFAULT_HOST, DEFAULT_PORT
    sock.setsockopt(Socket::SOL_SOCKET,Socket::SO_REUSEADDR, true)
    sock.send("#{method} #{DEFAULT_PATH} #{DEFAULT_VERSION}\r\n", 0)
    sock.send("Host: #{DEFAULT_HOST}\r\n", 0)
    DEFAULT_HEADER.each_pair {|k, v|
      sock.send("#{k}: #{v}\r\n", 0)
    }
    sock.send("\r\n", 0)

    data = sock.recv(1024 * 2)
    return data
  rescue
    raise
  end
end


class BadHttpMethodTest < Test::Unit::TestCase

  def test_bad_method1
    response = send_data("")
    assert_equal(ERR_400, response.split("\r\n").first)
  end

  def test_bad_method2
    response = send_data("GET" * 100)
    assert_equal(ERR_400, response.split("\r\n").first)
  end

  def test_bad_path
    response = send_data("..")
    assert_equal(ERR_400, response.split("\r\n").first)
  end

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
