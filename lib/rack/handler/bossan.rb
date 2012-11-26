require 'rack/handler'
require 'bossan'

module Rack
  module Handler
    module Bossan
      DEFAULT_OPTIONS = {
        "Host" => '0.0.0.0',
        "Port" => 8080,
        # :Verbose => false
      }

      def self.run(app, options = {})
        options = DEFAULT_OPTIONS.merge(options)
        ::Bossan.run(options["Host"], options["Port"], app)
      end
    end
    register :bossan, Bossan
  end
end
