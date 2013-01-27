# -*- encoding: utf-8 -*-
require 'rbconfig'
require File.expand_path('../lib/bossan/version', __FILE__)
 
gem_files = Dir["LICENSE.txt",
                "lib/**/*.rb",
                "ext/**/*"]
 
 
if RbConfig::CONFIG['host_os'] =~ /linux/
  gem_files.delete "ext/bossan/picoev_kqueue.c"
elsif RbConfig::CONFIG['host_os'] =~ /darwin|(open|free)bsd/
  gem_files.delete "ext/bossan/picoev_epoll.c"
else
  STDOUT.puts "Be posix compliant is mandatory"
  exit 1
end
 
Gem::Specification.new do |gem|
  gem.name = "bossan"
  gem.version = Bossan::VERSION
  gem.authors = ["Hiroki Noda"]
  gem.email = ["kubo39@gmail.com"]
  gem.description = %q{high performance asynchronous rack web server}
  gem.summary = gem.description
  gem.homepage = "https://github.com/kubo39/bossan"
 
  gem.files = gem_files
  gem.extensions = ["ext/bossan/extconf.rb"]
  gem.require_paths = ["lib", "ext"]
 
  gem.required_ruby_version = ">= 1.9.2"
  gem.add_dependency "rack", ["~> 1.2"]
end