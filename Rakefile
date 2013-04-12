#!/usr/bin/env rake
require "bundler/gem_tasks"
require "rbconfig"

task :default => [:compile, :clean, :test]

task :compile do
  Dir.chdir File.expand_path("../ext/bossan", __FILE__)
  sh "ruby extconf.rb"
  sh "make"
  sh "mv bossan_ext.#{RbConfig::CONFIG['DLEXT']} ../../lib/bossan/"
end

task :clean do
  Dir.chdir File.expand_path("../ext/bossan", __FILE__)
  sh "rm -f *.o Makefile mkmf.log"
end

task :test do
  Dir.chdir File.expand_path("../test", __FILE__)
  sh "ruby test_rack_spec.rb"
end
