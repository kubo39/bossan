require 'sinatra'
require 'active_record'
require 'erb'

enable :sessions
set :views, File.dirname(__FILE__) + '/views'

DATABASE = './db/sinatr.db'
SECRET_KEY = 'development_key'
USERNAME = 'admin'
PASSWORD = 'default'

ActiveRecord::Base.establish_connection(
  :adapter => 'sqlite3',
  :database =>  DATABASE
)

get '/' do
  @entries = Entries.find(:all).map do |row|
    {:title => row.title,
      :text => row.text }
  end
  @entries ||= []
  erb :show_entries
end

post '/add' do
  halt(401) if session.include?([:logged_in])
  Entries.create(:title => params[:title],
                 :text => params[:text])
  redirect '/'
end

post '/login' do
  @err = nil
  if params[:username] != USERNAME
    @err = "Invalid username #{params[:username]}"
  elsif params[:password] != PASSWORD
    @err = "Invalid password"
  else
    session[:logged_in] = params[:username]
    redirect '/'
  end
  erb :login
end

get '/login' do
  erb :login
end

get '/logout' do
  session.delete :logged_in
  erb :show_entries
end

class Schema < ActiveRecord::Migration
  def self.up
    create_table :entries do |t|
      t.column :title, :string, :null => false
      t.column :text, :string, :null => false
    end
  end

  def self.down
    drop_table :entries
  end
end

class Entries < ActiveRecord::Base
end

def init_db
  Schema.migrate :up
end

def drop_db
  Schema.migrate :down
end
