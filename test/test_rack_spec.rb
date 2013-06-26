require_relative '../lib/bossan'
require 'test/unit'
require 'uri'
require 'net/http'

class RackSpecTest < Test::Unit::TestCase

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

  def test_simple_get
    response = Net::HTTP.start(DEFAULT_HOST, DEFAULT_PORT) {|http|
      http.get("/")
    }

    assert_equal("200", response.code)
    assert_equal(ASSERT_RESPONSE, response.body)
  end

  def test_simple_post
    response = Net::HTTP.post_form(URI.parse("http://#{DEFAULT_HOST}:#{DEFAULT_PORT}/"),
                                   {'key1'=> 'value1', 'key2'=> 'value2'})

    assert_equal("200", response.code)
    assert_equal(ASSERT_RESPONSE, response.body)
  end

  def teardown
    Process.kill(:INT, @pid)
  end
end
