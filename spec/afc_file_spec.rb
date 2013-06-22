$LOAD_PATH << "./ext"
$LOAD_PATH << "./lib"
require 'spec_helper'
require 'io/afc'


describe IO::AFC::File do
  before do
    @afc = IO::AFC.connect
  end

  context "method: open, " do
    it "should require block." do
      expect{ @afc.open("/") }.to raise_error(ArgumentError)
    end

    it "should be open '/'" do
      r = @afc.open("/"){|f|
            expect(f).to be_an_instance_of(IO::AFC::File)
            expect(f.closed?).to eq(false)
            f
          }
      expect(r).to be_an_instance_of(IO::AFC::File)
      expect(r.closed?).to eq(true)
    end

    it "should be create file and write." do
      @afc.open("/tempfile", IO::RDWR){|f|
        expect(f).to be_an_instance_of(IO::AFC::File)
        expect(f.write("Hello")).to eq(5)
      }
    end
  end

  after do
    @afc.unlink("/tempfile") rescue nil
    @afc.close
  end
end
