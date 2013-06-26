require_relative '../lib/bossan'
require 'sinatra/base'
require 'haml'

class App < Sinatra::Base
  get '/' do
    haml :index
  end
end

Bossan.listen('127.0.0.1', 8000)
Bossan.run(App)
