$LOAD_PATH << "./ext"
require 'io/afc/afc_base'
require 'spec_helper'


describe IO::AFCBase do
  context "Basic storage I/O, " do
    context "File control, " do
      before do
        @afc = IO::AFCBase.new(nil, nil)
        f = IO::AFCBase::Descriptor.new(@afc, "/tempfile", 2)
        begin
          f.write("hello")
        ensure
          f.close
        end
      end

      it "should be utimens." do
        t = Time.now - 200000000
        expect(@afc.utimens("/tempfile", t)).to be_true
        stat = dict_to_hash(@afc.getattr("/tempfile"))
        expect(stat["st_mtime"].to_i).to eq(t.to_i*1000_000_000)
      end

      it "should be get file size." do
        stat = dict_to_hash(@afc.getattr("/tempfile"))
        expect(stat["st_size"].to_i).to eq(5)
      end

      it "should be truncate." do
        expect(@afc.truncate("/tempfile", 1)).to be_true
        stat = dict_to_hash(@afc.getattr("/tempfile"))
        expect(stat["st_size"].to_i).to eq(1)
      end

      it "should be create hardlink." do
        expect(@afc.link("/tempfile", "/tempfile-2")).to be_true
        expect(@afc.readdir("/")).to be_include("tempfile-2")
      end

      after do
        %w{/tempfile /tempfile-2}.each{|p|
          @afc.unlink(p) rescue nil
        }
        @afc.close
      end
    end
  end
end
