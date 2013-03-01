require 'test/unit'
require 'bossan'
require 'net/http'

class RackSpecTest < Test::Unit::TestCase

  ASSERT_RESPONSE = "Hello world!"
  RESPONSE = ["Hello ", "world!"].freeze

  class App
    attr_reader :env
    def call env
      @env = env.dup
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
        Net::HTTP.start("localhost", 8000){|http|
        }
      rescue
        next
      end
      server_is_wake_up = true
      $stderr.puts "*** running success ***"
      return true
    }
    return false
  end

  private :server_is_wake_up?

  def setup
    @pid = fork do
      trap(:INT) { Bossan.stop }
      Bossan.run("localhost", 8000, App.new)
    end
    Process.detach @pid
    unless server_is_wake_up?
      $stderr.puts "bossan won't wake up until you love it ..."
      exit 1
    end
  end

  def test_simple
    response = nil
    Net::HTTP.start("localhost", 8000){|http|
      response = http.get("/")
    }
    assert_equal("200", response.code)
    assert_equal(ASSERT_RESPONSE, response.body)
  end

  def teardown
    Process.kill(:INT, @pid)
  end
end
