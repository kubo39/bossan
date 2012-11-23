require_relative "bossan/version"
require_relative "bossan/bossan_ext"

def run(host='127.0.0.1', port=8000, app)
  Bossan.run(host, port, app)
end
