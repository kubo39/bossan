require_relative '../lib/bossan'
require 'minitest/unit'
require 'test/unit/testcase'
require 'net/http'
require 'tempfile'

module Bossan::Test
  class TestCase < Test::Unit::TestCase
    def setup
      super
      check_app
      start_server
    end

    def teardown
      super
      stop_server
      check_output_log
    end

    def host
      "localhost"
    end

    def port
      8000
    end

    def app
      raise("app must override")
    end

    private
    def check_app
      app
    end

    def check_output_log
      @mocked_stdout.rewind
      server_stdout_log = @mocked_stdout.read
      unless server_stdout_log == "Bye.\n"
        puts "! Something wrong on server. stdout:"
        print server_stdout_log
      end

      @mocked_stderr.rewind
      server_stderr_log = @mocked_stderr.read
      unless server_stderr_log == ""
        puts "! Something wrong on server. stderr:"
        print server_stderr_log
      end
    end

    def start_server
      @mocked_stdout = Tempfile.new("bossan.out")
      @mocked_stderr = Tempfile.new("bossan.err")

      @pid = fork do
        STDOUT.reopen(@mocked_stdout)
        STDERR.reopen(@mocked_stderr)
        trap(:INT) { Bossan.stop }
        Bossan.listen(host, port)
        Bossan.run(app)
      end

      Process.detach @pid

      unless server_is_wake_up?
        $stderr.puts "bossan won't wake up until you love it ..."
        exit 1
      end
    end

    def stop_server
      Process.kill(:INT, @pid)
    end

    def server_is_wake_up? n=100
      n.times {
        begin
          Net::HTTP.start(host, port)
        rescue
          next
        end
        return true
      }
      return false
    end
  end
end
