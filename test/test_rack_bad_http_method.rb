require_relative '../lib/bossan'
require 'test/unit'
require 'socket'
require 'net/http'

class BadHttpMethodTest < Test::Unit::TestCase

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

  private :server_is_wake_up?

  def send_data method
    begin
      sock = TCPSocket.new DEFAULT_HOST, DEFAULT_PORT
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

  def setup
    $stderr.puts RUBY_DESCRIPTION

    @pid = fork do
      trap(:INT) { Bossan.stop }
      Bossan.listen(DEFAULT_HOST, DEFAULT_PORT)
      Bossan.run(App.new)
    end
    Process.detach @pid
    unless server_is_wake_up?
      $stderr.puts "bossan won't wake up until you love it ..."
      exit 1
    end
  end

  def test_bad_method1
    response = send_data("")
    assert_equal(response.split("\r\n").first, ERR_400)
  end

  def test_bad_method2
    response = send_data("GET" * 100)
    assert_equal(response.split("\r\n").first, ERR_400)
  end

  def teardown
    Process.kill(:INT, @pid)
  end
end
