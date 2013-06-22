$LOAD_PATH << "./ext"
$LOAD_PATH << "./lib"
require 'spec_helper'
require 'io/afc'


describe IO::AFC do
  it "should be connect." do
    r = IO::AFC.connect{|afc|
          expect(afc).to be_an_instance_of(IO::AFC)
          expect(afc.closed?).to eq(false)
          afc
        }
    expect(r).to be_an_instance_of(IO::AFC)
    expect(r.closed?).to eq(true)
  end

  it "should be connect without block." do
    afc = IO::AFC.connect
    expect(afc).to be_an_instance_of(IO::AFC)
    expect(afc.closed?).to eq(false)
    expect(afc.close).to be_nil
    expect(afc.closed?).to eq(true)
  end


  context "test for storage methods, " do
    before do
      @afc = IO::AFC.connect
    end

    it "should be mkdir and unlink." do
      expect(@afc.mkdir("/tempdir")).to be_true
      expect(@afc.unlink("/tempdir")).to be_true
    end

    it "should be getattr for directory." do
      attr = @afc.getattr("/Safari")
      expect(attr).to be_an_instance_of(Hash)
      expect(attr[:st_birthtime]).to be_an_instance_of(Time)
      expect(attr[:st_mtime]).to be_an_instance_of(Time)
      expect(attr[:st_ifmt]).to eq("S_IFDIR")
    end

    context "with tempdir, " do
      before do
        make_directory("/tempdir")
        @now = Time.now
      end

      it "should be getattr for directory (Time)." do
        attr = @afc.getattr("/tempdir")
        expect(attr[:st_birthtime].to_i).to eq(@now.to_i)
        expect(attr[:st_mtime].to_i).to eq(@now.to_i)
      end

      it "should be mkdir, rename and unlink subdir." do
        expect(@afc.mkdir("/tempdir/subdir")).to be_true
        expect(@afc.rename("/tempdir/subdir", "/tempdir/subdir-2")).to be_true
        expect(@afc.unlink("/tempdir/subdir-2")).to be_true
      end

      it "should be symlink, readlink and unlink." do
        expect(@afc.symlink("subdir", "/tempdir/subdir-2")).to be_true
        expect(@afc.readlink("/tempdir/subdir-2")).to eq("subdir")
        expect(@afc.unlink("/tempdir/subdir-2")).to be_true
      end

      it "should be nil if readlink to not a link." do
        expect(@afc.readlink("/tempdir")).to be_nil
      end

      it "should be check exists." do
        expect(@afc.exist?("/tempdir")).to be_an_instance_of(String)
        expect(@afc.exist?("/tempdir-not-exist")).to be_nil
      end

      after do
        remove_directory("/tempdir")
      end
    end

    after do
      @afc.close
    end
  end
end
