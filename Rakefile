require 'bundler/gem_tasks'

desc "Build extension."
task :ext do
  sh <<-__SCRIPT__
cd ext/io/afc/
ruby extconf.rb
make
  __SCRIPT__
end


desc "Clean."
task :clean do
  sh <<-__SCRIPT__
pushd ext/io/afc
make clean
rm -f Makefile
popd
pushd io-afc.xcodeproj
rm -rf project.xcworkspace xcuserdata
popd
  __SCRIPT__
end


desc "Debug console."
task :console do
  Rake::Task["ext"].invoke
  system "pry -I./ext -I./lib -r io/afc"
end


desc "Run test."
task :test do
  Rake::Task["ext"].invoke
  system "rspec spec/*_spec.rb"
end
