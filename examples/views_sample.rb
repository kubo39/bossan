require_relative '../lib/bossan'
require 'sinatra/base'
require 'haml'

class App < Sinatra::Base
  get '/' do
    haml :index
  end
end

Bossan.run('127.0.0.1', 8000, App)
