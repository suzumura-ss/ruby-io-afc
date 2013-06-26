# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'io/afc/version'

Gem::Specification.new do |spec|
  spec.name          = "io-afc"
  spec.version       = IO::AFCVersion::VERSION
  spec.authors       = ["Toshiyuki Suzumura"]
  spec.email         = ["suz.labo@amail.plala.or.jp"]
  spec.description   = %q{Access to the file system of iOS devices from Ruby.}
  spec.summary       = %q{Access to the file system of iOS devices from Ruby. Required 'libimobiledevice'.}
  spec.homepage      = "https://github.com/suzumura-ss/ruby-io-afc"
  spec.license       = "MIT"

  spec.files         = `git ls-files`.split($/)
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["lib"]
  spec.extensions    = ["ext/io/afc/extconf.rb"]

  spec.add_development_dependency "bundler", "~> 1.3"
  spec.add_development_dependency "rake"
end
