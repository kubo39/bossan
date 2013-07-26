require 'net/http'

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
