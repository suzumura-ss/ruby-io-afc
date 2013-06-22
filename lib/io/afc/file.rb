require 'io/afc/version'
require 'io/afc/afc_base'

class IO
class AFC < AFCBase
class File < Descriptor
public

  ##
  # Open file. / Invoke from IO::AFC#open
  # @param [String] path for target file.
  # @param [IO::Constants] mode for open mode. Default is IO::RDONLY.
  def self.open_file(afc, path, mode = IO::RDONLY, &block)
    raise ArgumentError, "block is required." unless block_given?
    fd = self.new(afc, path, mode)
    begin
      r = yield(fd)
      fd.close
    rescue Exception=>e
      fd.close
      throw e
    end
    r
  end
end
end
end
