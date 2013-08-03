#!/usr/bin/env rake
require "bundler/gem_tasks"
require "rbconfig"

task :default => [:compile, :clean, :test]

task :compile do
  pwd = Dir.pwd
  Dir.chdir File.expand_path("../ext/bossan", __FILE__)
  sh "ruby extconf.rb"
  sh "make"
  sh "mv bossan_ext.#{RbConfig::CONFIG['DLEXT']} ../../lib/bossan/"
  Dir.chdir pwd
end

task :clean do
  pwd = Dir.pwd
  Dir.chdir File.expand_path("../ext/bossan", __FILE__)
  sh "rm -f *.o Makefile mkmf.log"
  Dir.chdir pwd
end

task :test do
  sh "ruby test/driver.rb"
end
