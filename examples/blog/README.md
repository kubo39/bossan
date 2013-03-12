# This is a simple microblog powered by sqlite.

### Requirements some extension

 * ActiveRecord (ORM)
 * eRuby        (Template)
 * SQLite       (Database)


#### fire up a irb(pry)

    require './app'; init_db


### Run

    rackup config.ru -s bossan

visit http://0.0.0.0:9292