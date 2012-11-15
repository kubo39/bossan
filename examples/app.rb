require 'bossan'
require 'sinatra/base'

class App < Sinatra::Base
  get '/' do
    'Hello world!'
  end
end

Bossan::Server.listen('127.0.0.1', 8000)
Bossan::Server.run App

