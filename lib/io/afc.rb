require 'io/afc/version'
require 'io/afc/afc_base'
require 'io/afc/file'

class IO
class AFC < AFCBase
private
  def dict_to_hash(dict)
    v = nil
    dict.inject({}){|s, k|
      unless v
        v = k
      else
        v = v.to_i if v=~/^\d+$/
        s[k.to_sym] = v
        v = nil
      end
      s
    }
  end

public

  ##
  # Connect to iOS-Device and yield block with handle if specified.
  # @option [:uuid=>String] Target UUID. if nil, connect to default device.
  # @option [:appid=>String] Mount application document folder if specified.
  # @return [IO::AFC or block-result]
  def self.connect(opt={}, &block)
    dev = self.new(opt[:udid], opt[:appid])
    return dev unless block_given?

    begin
      r = yield(dev)
      dev.close
    rescue Exception=>e
      dev.close
      throw e
    end
    r
  end

  ##
  # Get storage info.
  # @return [Hash]
  #
  # Example:
  # afc.statfs
  # => {:FSBlockSize=>4096,
  # :FSFreeBytes=>27478216704,
  # :FSTotalBytes=>30178787328,
  # :Model=>"iPod5,1"}
  def statfs
    dict_to_hash(super)
  end

  ##
  # Open file.
  # @param [String] path for target file.
  # @param [IO::Constants] mode for open mode. Default is IO::RDONLY.
  def open(path, mode = IO::RDONLY, &block)
    raise ArgumentError, "block is required." unless block_given?
    File.open_file(self, path, mode, &block)
  end

  ##
  # Get file/directory attribytes.
  # @param [String] path.
  # @return [Hash]
  #
  # Example:
  # afc.getattr("/Safari")
  # => {:st_birthtime=>2012-12-17 12:28:23 +0900,
  # :st_mtime=>2013-06-22 20:32:01 +0900
  # :st_ifmt=>"S_IFDIR",
  # :st_nlink=>2,
  # :st_blocks=>0,
  # :st_size=>102}
  #
  # afc.getattr("/Safari/goog-phish-shavar.db")
  # => {:st_birthtime=>2013-06-21 02:31:17 +0900,
  # :st_mtime=>2013-06-21 02:31:18 +0900,
  # :st_ifmt=>"S_IFREG",
  # :st_nlink=>1,
  # :st_blocks=>11256,
  # :st_size=>5763072}
  def getattr(path)
    r = dict_to_hash(super(path))
    [:st_birthtime, :st_mtime].each{|k|
      r[k] = Time.at(r[k] / 1000000000.0)
    }
    r
  end

  ##
  # Check exist
  # @param [String] path.
  # @return [String or nil] return type or nil.
  def exist?(path)
    begin
      getattr(path)[:st_ifmt]
    rescue Errno::ENOENT
      nil
    end
  end

  ##
  # Read symlink.
  # @param [String] path.
  # @return [String or nil] return symlink-target.
  def readlink(path)
    getattr(path)[:LinkTarget]
  end
end
end
