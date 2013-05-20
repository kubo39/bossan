require_relative '../lib/bossan'
require 'sinatra/base'

class App < Sinatra::Base
  get '/' do
    'Hello world!'
  end
end

Bossan.run('127.0.0.1', 8000, App)

