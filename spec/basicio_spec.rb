$LOAD_PATH << "./ext"
require 'spec_helper'
require 'io/afc/afc_base'


describe IO::AFCBase do
  context "Basic storage I/O, " do
    it "should be connect device." do
      expect(IO::AFCBase.new(nil, nil)).to be_an_instance_of(IO::AFCBase)
    end

    it "should be close." do
       afc = IO::AFCBase.new(nil, nil)
       expect(afc.closed?).to eq(false)
       expect(afc.close).to be_nil
       expect(afc.closed?).to eq(true)
    end

    context "After connect, " do
      before do
         @afc = IO::AFCBase.new(nil, nil)
      end

      it "should be get attributes." do
        attr = @afc.getattr("/")
        expect(attr).to be_an_instance_of(Array)
      end

      it "should be readdir." do
        dirs = @afc.readdir("/")
        expect(dirs).to be_an_instance_of(Array)
      end

      it "should be get statfs." do
        stat = @afc.statfs
        expect(stat).to be_an_instance_of(Array)
      end

      it "should be make a directory, rename, and unlink." do
        expect(@afc.mkdir("/tmp")).to be_true
        expect(@afc.rename("/tmp", "/temp")).to be_true
        expect(@afc.unlink("/temp")).to be_true
      end

      it "should be crerate a symlink." do
        expect(@afc.symlink("hello", "/tempfile-2")).to be_true
        expect(@afc.readdir("/")).to be_include("tempfile-2")
        stat = dict_to_hash(@afc.getattr("/tempfile-2"))
        expect(stat["LinkTarget"]).to eq("hello")
      end

      it "should be get device udid." do
        expect(@afc.device_udid).to be_an_instance_of(String)
      end

      it "should be get device display name." do
        expect(@afc.device_display_name).to be_an_instance_of(String)
      end

      it "should be get application list." do
        apps = @afc.applications
        expect(apps).to be_an_instance_of(Hash)
        apps.each{|app, val|
          expect(app).to be_an_instance_of(String)
          expect(val).to be_an_instance_of(Hash)
          expect(val["CFBundleDisplayName"]).to be_an_instance_of(String)
          expect([true, false]).to include(val["UIFileSharingEnabled"])
        }
      end

      after do
        %w{/tempfile /tempfile-2 /tmp /temp}.each{|p|
          @afc.unlink(p) rescue nil
        }
        @afc.close
      end
    end
  end
end
