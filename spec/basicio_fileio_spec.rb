$LOAD_PATH << "./ext"
require 'io/afc/afc_base'
require 'spec_helper'


describe IO::AFCBase do
  context "Basic storage I/O, " do
    context "File IO, " do
      before do
        @afc = IO::AFCBase.new(nil, nil)
      end

      it "should be create file and close." do
        f = IO::AFCBase::Descriptor.new(@afc, "/tempfile", 2)
        begin
          expect(f).to be_an_instance_of(IO::AFCBase::Descriptor)
          expect(f.closed?).to eq(false)
          expect(f.write("hello")).to eq(5)
        ensure
          expect(f.close).to be_nil
          expect(f.closed?).to eq(true)
        end

        f = IO::AFCBase::Descriptor.new(@afc, "/tempfile", 0)
        begin
          expect(f.read(-1)).to eq("hello")
          expect(f.tell).to eq(5)
          expect(f.seek(0, IO::SEEK_SET)).to be_true
          expect(f.tell).to eq(0)
        ensure
          f.close
        end

        f = IO::AFCBase::Descriptor.new(@afc, "/tempfile", 2)
        begin
          expect(f.ftruncate(2)).to be_true
        ensure
          f.close
        end
        info = @afc.getattr("/tempfile")
        size = info[info.find_index("st_size")-1].to_i
        expect(size).to eq(2)
      end

      it "should be write 0 bytes when data is nil." do
        f = IO::AFCBase::Descriptor.new(@afc, "/tempfile", 2)
        begin
          expect(f.write(nil)).to eq(0)
        ensure
          f.close
        end
      end

      after do
        @afc.unlink("/tempfile") rescue nil
        @afc.close
      end
    end
  end
end
