# -*- coding: utf-8 -*-
require 'rack/handler'
require_relative '../../bossan'

module Rack
  module Handler
    module Bossan
      DEFAULT_OPTIONS = {
        :Host => '127.0.0.1',
        :Port => 8000,
        # :Verbose => false
      }

      def self.run(app, options = {})
        options = DEFAULT_OPTIONS.merge(options)
        puts "* Listening on tcp://#{options[:Host]}:#{options[:Port]}"

        ::Bossan.run(options[:Host], options[:Port], app)
      end

      def self.valid_options
        {
          "Host=HOST" => "Hostname to listen on (default: localhost)",
          "Port=PORT" => "Port to listen on (default: 8000)",
        }
      end
    end
    register :bossan, Bossan
  end
end
