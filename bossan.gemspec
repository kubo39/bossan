# -*- encoding: utf-8 -*-
require File.expand_path('../lib/bossan/version', __FILE__)
 
Gem::Specification.new do |gem|
  gem.name = "bossan"
  gem.version = Bossan::VERSION
  gem.authors = ["Hiroki Noda"]
  gem.email = ["kubo39@gmail.com"]
  gem.description = %q{high performance asynchronous rack web server}
  gem.summary = gem.description
  gem.homepage = "https://github.com/kubo39/bossan"
 
  gem.files = `git ls-files`.split($/)
  gem.extensions = ["ext/bossan/extconf.rb"]
  gem.require_paths = ["lib", "ext"]
  gem.test_files = `git ls-files -- {test,spec,features}/*`.split($/)
 
  gem.required_ruby_version = ">= 1.9.2"
  gem.add_dependency "rack", ["~> 1.2"]
end
