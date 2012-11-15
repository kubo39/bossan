#!/usr/bin/env rake
require "bundler/gem_tasks"

task :default => [:compile, :clean]

task :compile do
  Dir.chdir File.expand_path("../ext/bossan", __FILE__)
  system "ruby extconf.rb"
  system "make"
end

task :clean do
  Dir.chdir File.expand_path("../ext/bossan", __FILE__)
  system "rm -f *.o Makefile"
end
