require_relative "bossan/version"
require_relative "bossan/bossan_ext"
require_relative "rack/handler/bossan"

def run *args
  Bossan.run *args
end
